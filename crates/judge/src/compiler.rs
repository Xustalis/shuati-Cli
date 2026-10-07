//! 编译器抽象：源码 → Program。
//! argv 数组调用，无 shell 拼接——命令注入在类型层面不存在。

use crate::{detect_toolchain, JudgeError, Program, Result};
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
        let executable = detect_toolchain(lang, configured)
            .ok_or_else(|| JudgeError::CompilerNotFound(format!("{lang:?}")))?;
        Ok(Self { lang, executable })
    }
}

/// 编译源文件 → Program。
/// - cpp: g++ -O2 -std=c++17 src -o out（标准取第一个可用的，不循环重试一堆 flag）
/// - python: 不编译，Program.argv = [python, script]
pub async fn compile_source(src: &SourceFile, tc: &Toolchain) -> Result<Program> {
    match tc.lang {
        Lang::Cpp => compile_cpp(src, tc).await,
        Lang::Python => prepare_python(src, tc).await,
    }
}

async fn compile_cpp(src: &SourceFile, tc: &Toolchain) -> Result<Program> {
    let src_path = src.path.canonicalize().unwrap_or_else(|_| src.path.clone());
    let out_dir = src_path.parent().unwrap_or_else(|| std::path::Path::new("."));
    let artifact = out_dir.join(if cfg!(windows) { "shuati_exe.exe" } else { "shuati_exe" });

    // 标准探测：优先 c++17（兼容性最广），失败再试 c++20
    let mut last_err = String::new();
    for std_flag in ["-std=c++17", "-std=c++20"] {
        if artifact.exists() {
            let _ = std::fs::remove_file(&artifact);
        }
        let output = timeout(
            COMPILE_TIMEOUT,
            Command::new(&tc.executable)
                .arg("-O2")
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
            return Ok(Program {
                argv: vec![artifact.to_string_lossy().to_string()],
                lang: Lang::Cpp,
                artifact,
                workdir: out_dir.to_path_buf(),
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
    // 语法快速检查：python -m py_compile
    let output = timeout(
        COMPILE_TIMEOUT,
        Command::new(&tc.executable)
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
        return Err(JudgeError::Compile(
            String::from_utf8_lossy(&output.stderr).trim().to_string(),
        ));
    }
    Ok(Program {
        argv: vec![
            tc.executable.to_string_lossy().to_string(),
            src_path.to_string_lossy().to_string(),
        ],
        lang: Lang::Python,
        artifact: src_path.clone(),
        workdir: src_path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf(),
    })
}
