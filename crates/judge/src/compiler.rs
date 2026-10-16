//! 编译器抽象：源码 → Program。
//! argv 数组调用，无 shell 拼接——命令注入在类型层面不存在。

use crate::{JudgeError, Program, Result, detect_toolchain};
use shuati_core::{Lang, SourceFile};
use std::path::PathBuf;
use std::process::Stdio;
use std::time::Duration;
use tokio::process::Command;
use tokio::time::timeout;

const COMPILE_TIMEOUT: Duration = Duration::from_secs(60);

/// 语言编译配置
pub struct Toolchain {
    pub lang: Lang,
    pub executable: PathBuf,
}

impl Toolchain {
    pub fn detect(lang: Lang, configured: &str) -> Result<Self> {
        let executable =
            detect_toolchain(lang, configured).ok_or_else(|| JudgeError::CompilerNotFound(format!("{lang:?}")))?;
        Ok(Self { lang, executable })
    }
}

/// 编译源文件 → Program。
/// - cpp: g++ -O2 -std=c++17 src -o out（标准取第一个可用的，不循环重试一堆 flag）
/// - python: 不编译，Program.argv = [python, script]
pub async fn compile_source(src: &SourceFile, tc: &Toolchain) -> Result<Program> {
    match tc.lang {
        Lang::Cpp => compile_cpp(src, tc, None).await,
        Lang::Python => prepare_python(src, tc).await,
    }
}

/// 带缓存的编译：源文件 mtime+大小+编译器不变 → 直接复用产物。
/// 缓存布局: cache_dir/<hash>/shuati_exe + meta.txt
pub async fn compile_cached(src: &SourceFile, tc: &Toolchain, cache_dir: &std::path::Path) -> Result<Program> {
    match tc.lang {
        Lang::Cpp => compile_cpp(src, tc, Some(cache_dir)).await,
        Lang::Python => prepare_python(src, tc).await,
    }
}

/// 缓存键：源文件绝对路径 + 编译器路径 → 目录名
fn cache_key(src_path: &std::path::Path, compiler: &std::path::Path) -> String {
    use std::hash::{Hash, Hasher};
    let mut h = std::collections::hash_map::DefaultHasher::new();
    src_path.to_string_lossy().hash(&mut h);
    compiler.to_string_lossy().hash(&mut h);
    format!("{:016x}", h.finish())
}

/// 源文件指纹：mtime(ns) + size，变了就得重编
fn src_fingerprint(src_path: &std::path::Path) -> String {
    match std::fs::metadata(src_path) {
        Ok(m) => {
            let mt = m
                .modified()
                .ok()
                .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
                .map(|d| d.as_nanos())
                .unwrap_or(0);
            format!("{}|{}", mt, m.len())
        }
        Err(_) => String::new(),
    }
}

async fn compile_cpp(src: &SourceFile, tc: &Toolchain, cache_dir: Option<&std::path::Path>) -> Result<Program> {
    let src_path = src.path.canonicalize().unwrap_or_else(|_| src.path.clone());
    let (out_dir, cached) = match cache_dir {
        Some(d) => {
            let dir = d.join(cache_key(&src_path, &tc.executable));
            std::fs::create_dir_all(&dir).map_err(JudgeError::Io)?;
            (dir, true)
        }
        None => (src_path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf(), false),
    };
    let artifact = out_dir.join(if cfg!(windows) { "shuati_exe.exe" } else { "shuati_exe" });
    let meta_path = out_dir.join("meta.txt");
    let fingerprint = src_fingerprint(&src_path);

    // 缓存命中：指纹一致且产物在 → 零编译直接返回
    if cached && artifact.exists() && std::fs::read_to_string(&meta_path).ok().as_deref() == Some(fingerprint.as_str())
    {
        return Ok(Program {
            argv: vec![artifact.to_string_lossy().to_string()],
            lang: Lang::Cpp,
            artifact,
            workdir: src_path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf(),
            cached: true,
        });
    }

    // 标准探测：优先 c++17（兼容性最广），失败再试 c++20
    let mut last_err = String::new();
    for std_flag in ["-std=c++17", "-std=c++20"] {
        if artifact.exists() {
            let _ = std::fs::remove_file(&artifact);
        }
        let mut cmd = Command::new(&tc.executable);
        // 超时返回后 drop 会杀掉子进程，不留编译器孤儿
        cmd.kill_on_drop(true);
        let output = timeout(
            COMPILE_TIMEOUT,
            cmd.arg("-O2")
                .arg(std_flag)
                .arg(&src_path)
                .arg("-o")
                .arg(&artifact)
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::piped())
                .output(),
        )
        .await
        .map_err(|_| JudgeError::Compile(format!("compile timeout ({}s)", COMPILE_TIMEOUT.as_secs())))?
        .map_err(JudgeError::Io)?;

        if output.status.success() && artifact.exists() {
            if cached {
                let _ = std::fs::write(&meta_path, &fingerprint);
            }
            return Ok(Program {
                argv: vec![artifact.to_string_lossy().to_string()],
                lang: Lang::Cpp,
                artifact,
                workdir: src_path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf(),
                cached,
            });
        }
        last_err = String::from_utf8_lossy(&output.stderr).to_string();
        // flag 不识别才换下一个标准；语法错误直接报
        if !last_err.contains(std_flag) {
            break;
        }
    }
    Err(JudgeError::Compile(last_err.trim().to_string()))
}

async fn prepare_python(src: &SourceFile, tc: &Toolchain) -> Result<Program> {
    let src_path = src.path.canonicalize().unwrap_or_else(|_| src.path.clone());
    // 语法快速检查：python -B -m py_compile（-B 避免在用户目录产 __pycache__）
    let mut cmd = Command::new(&tc.executable);
    cmd.kill_on_drop(true);
    let output = timeout(
        COMPILE_TIMEOUT,
        cmd.arg("-B")
            .arg("-m")
            .arg("py_compile")
            .arg(&src_path)
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::piped())
            .output(),
    )
    .await
    .map_err(|_| JudgeError::Compile("py_compile timeout".into()))?
    .map_err(JudgeError::Io)?;

    if !output.status.success() {
        return Err(JudgeError::Compile(String::from_utf8_lossy(&output.stderr).trim().to_string()));
    }
    Ok(Program {
        argv: vec![tc.executable.to_string_lossy().to_string(), src_path.to_string_lossy().to_string()],
        lang: Lang::Python,
        artifact: src_path.clone(),
        workdir: src_path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf(),
        cached: false,
    })
}
