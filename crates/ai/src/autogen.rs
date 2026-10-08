//! AI 自动生成对拍脚本管线：
//! gen_scripts → 写临时文件 → 编译 → dry-run（gen 出输入、oracle 出答案）
//! → 失败把 stderr 喂回 repair_scripts 重试 ≤3 轮。
//! 保证返回的脚本是"验证过能跑"的，绝不静默交付坏脚本。

use crate::{AiClient, AiError, Result};
use shuati_core::{Lang, Limits, SourceFile};
use shuati_judge as judge;
use std::path::{Path, PathBuf};

const MAX_ATTEMPTS: u32 = 3;

/// dry-run 验证通过才落盘。
/// gen 写到 `gen_dir/gen.<ext>`，oracle 写到 `oracle_dir/brute.<ext>`——
/// 必须与 find_script 的探测目录一致（gen/ 和 solutions/），否则下次找不到。
pub async fn produce_scripts(
    client: &AiClient,
    statement: &str,
    lang: Lang,
    gen_dir: &Path,
    oracle_dir: &Path,
    cpp_cfg: &str,
    py_cfg: &str,
    mut on_status: impl FnMut(&str),
) -> Result<(PathBuf, PathBuf)> {
    let lang_name = match lang {
        Lang::Cpp => "cpp",
        Lang::Python => "python",
    };
    let compiler = match lang {
        Lang::Cpp => cpp_cfg,
        Lang::Python => py_cfg,
    };
    let tc = judge::Toolchain::detect(lang, compiler)
        .map_err(|e| AiError::Api(format!("工具链: {e}")))?;

    on_status("AI 生成 gen/oracle 脚本…");
    let mut scripts = client.gen_scripts(statement, lang_name).await?;

    for attempt in 1..=MAX_ATTEMPTS {
        // 写入临时目录验证
        let tmp = tempfile::tempdir().map_err(|e| AiError::Api(e.to_string()))?;
        let ext = match lang {
            Lang::Cpp => "cpp",
            Lang::Python => "py",
        };
        let gen_tmp = tmp.path().join(format!("gen.{ext}"));
        let oracle_tmp = tmp.path().join(format!("oracle.{ext}"));
        std::fs::write(&gen_tmp, &scripts.generator).map_err(|e| AiError::Api(e.to_string()))?;
        std::fs::write(&oracle_tmp, &scripts.oracle).map_err(|e| AiError::Api(e.to_string()))?;

        let gen_src = SourceFile { path: gen_tmp.clone(), lang };
        let oracle_src = SourceFile { path: oracle_tmp.clone(), lang };

        on_status(&format!("dry-run 验证（第 {attempt} 次）…"));
        match dry_run(&gen_src, &oracle_src, &tc).await {
            Ok(()) => {
                // 验证通过 → 落盘到约定子目录
                std::fs::create_dir_all(gen_dir).map_err(|e| AiError::Api(e.to_string()))?;
                std::fs::create_dir_all(oracle_dir).map_err(|e| AiError::Api(e.to_string()))?;
                let gen_dest = gen_dir.join(format!("gen.{ext}"));
                let brute_dest = oracle_dir.join(format!("brute.{ext}"));
                std::fs::write(&gen_dest, &scripts.generator).map_err(|e| AiError::Api(e.to_string()))?;
                std::fs::write(&brute_dest, &scripts.oracle).map_err(|e| AiError::Api(e.to_string()))?;
                on_status("脚本验证通过");
                return Ok((gen_dest, brute_dest));
            }
            Err(err) if attempt < MAX_ATTEMPTS => {
                on_status(&format!("验证失败，让 AI 修复: {}", truncate(&err, 120)));
                scripts = client
                    .repair_scripts(statement, lang_name, &scripts, &err)
                    .await?;
            }
            Err(err) => {
                return Err(AiError::Parse(format!(
                    "AI 生成的脚本经 {MAX_ATTEMPTS} 轮修复仍不可用: {}",
                    truncate(&err, 300)
                )));
            }
        }
    }
    unreachable!()
}

/// dry-run：编译两个脚本，跑 3 个 seed 的 gen → oracle，任何一步挂都算失败
async fn dry_run(gen_src: &SourceFile, oracle_src: &SourceFile, tc: &judge::Toolchain) -> std::result::Result<(), String> {
    let gen_prog = judge::compile_source(gen_src, tc)
        .await
        .map_err(|e| format!("gen 编译失败: {e}"))?;
    let oracle_prog = match judge::compile_source(oracle_src, tc).await {
        Ok(p) => p,
        Err(e) => {
            judge::cleanup_program(&gen_prog);
            return Err(format!("oracle 编译失败: {e}"));
        }
    };

    let result = dry_run_seeds(&gen_prog, &oracle_prog).await;
    judge::cleanup_program(&gen_prog);
    judge::cleanup_program(&oracle_prog);
    result
}

async fn dry_run_seeds(gen_prog: &judge::Program, oracle_prog: &judge::Program) -> std::result::Result<(), String> {
    let tmp = tempfile::tempdir().map_err(|e| e.to_string())?;
    let td = tmp.path();
    let glim = Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 16 };
    let rlim = Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 16 };

    for seed in [1u64, 42, 7] {
        // gen(seed) → input
        let gen_io = judge::Io {
            stdin: None,
            stdout: td.join(format!("g{seed}.out")),
            stderr: td.join(format!("g{seed}.err")),
        };
        let mut gen_with_seed = gen_prog.clone();
        gen_with_seed.argv.push(seed.to_string());
        let g = judge::run_program(&gen_with_seed, &gen_io, &glim, None)
            .await
            .map_err(|e| e.to_string())?;
        if g.status != judge::RunStatus::Ok {
            let err = std::fs::read_to_string(&gen_io.stderr).unwrap_or_default();
            return Err(format!("gen 运行失败(seed={seed}, {:?}): {}", g.status, truncate(&err, 200)));
        }
        let input = std::fs::read_to_string(&gen_io.stdout).unwrap_or_default();
        if input.trim().is_empty() {
            return Err(format!("gen 输出为空(seed={seed})"));
        }

        // oracle 跑这份输入 → output
        let in_f = td.join(format!("case{seed}.in"));
        std::fs::write(&in_f, &input).map_err(|e| e.to_string())?;
        let oracle_io = judge::Io {
            stdin: Some(in_f),
            stdout: td.join(format!("o{seed}.out")),
            stderr: td.join(format!("o{seed}.err")),
        };
        let o = judge::run_program(oracle_prog, &oracle_io, &rlim, None)
            .await
            .map_err(|e| e.to_string())?;
        if o.status != judge::RunStatus::Ok {
            let err = std::fs::read_to_string(&oracle_io.stderr).unwrap_or_default();
            return Err(format!("oracle 运行失败(seed={seed}, {:?}): {}", o.status, truncate(&err, 200)));
        }
        let output = std::fs::read_to_string(&oracle_io.stdout).unwrap_or_default();
        if output.trim().is_empty() {
            return Err(format!("oracle 输出为空(seed={seed})"));
        }
    }
    Ok(())
}

fn truncate(s: &str, n: usize) -> String {
    if s.len() <= n { s.to_string() } else { format!("{}…", s.chars().take(n).collect::<String>()) }
}
