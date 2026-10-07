//! 输出比较器：tokens / exact / float{eps} / spj{外部脚本}。

use crate::{JudgeError, Result};
use shuati_core::{CheckCtx, CheckerSpec, Verdict};
use std::path::PathBuf;

/// 同步比较（纯计算，不阻塞）
pub fn check(spec: &CheckerSpec, ctx: &CheckCtx) -> Result<Verdict> {
    match spec {
        CheckerSpec::Tokens => Ok(check_tokens(ctx.actual, ctx.expected)),
        CheckerSpec::Exact => Ok(if ctx.actual == ctx.expected { Verdict::Ac } else { Verdict::Wa }),
        CheckerSpec::Float { eps } => Ok(check_float(ctx.actual, ctx.expected, *eps)),
        CheckerSpec::Spj { .. } => Err(JudgeError::Other(
            "spj checker requires async check_async".into(),
        )),
    }
}

/// 异步入口（spj 需要跑外部程序）
pub async fn check_async(spec: &CheckerSpec, ctx: &CheckCtx<'_>, tmpdir: &std::path::Path) -> Result<Verdict> {
    if let CheckerSpec::Spj { program } = spec {
        return check_spj(program, ctx, tmpdir).await;
    }
    check(spec, ctx)
}

fn check_tokens(actual: &str, expected: &str) -> Verdict {
    let mut a = actual.split_whitespace();
    let mut e = expected.split_whitespace();
    loop {
        match (a.next(), e.next()) {
            (None, None) => return Verdict::Ac,
            (Some(x), Some(y)) if x == y => continue,
            _ => return Verdict::Wa,
        }
    }
}

fn check_float(actual: &str, expected: &str, eps: f64) -> Verdict {
    let mut a = actual.split_whitespace();
    let mut e = expected.split_whitespace();
    loop {
        match (a.next(), e.next()) {
            (None, None) => return Verdict::Ac,
            (Some(x), Some(y)) => {
                if x == y {
                    continue;
                }
                match (x.parse::<f64>(), y.parse::<f64>()) {
                    (Ok(fx), Ok(fy)) if (fx - fy).abs() <= eps => continue,
                    (Ok(fx), Ok(fy)) if fx.abs() > 1e-9 && ((fx - fy) / fx).abs() <= eps => continue,
                    _ => return Verdict::Wa,
                }
            }
            _ => return Verdict::Wa,
        }
    }
}

/// testlib 约定：checker <input> <actual> <expected>，exit 0 = AC，否则 WA
async fn check_spj(program: &PathBuf, ctx: &CheckCtx<'_>, tmpdir: &std::path::Path) -> Result<Verdict> {
    use tokio::process::Command;
    std::fs::create_dir_all(tmpdir)?;
    let in_f = tmpdir.join("spj_input.txt");
    let act_f = tmpdir.join("spj_actual.txt");
    let exp_f = tmpdir.join("spj_expected.txt");
    std::fs::write(&in_f, ctx.input)?;
    std::fs::write(&act_f, ctx.actual)?;
    std::fs::write(&exp_f, ctx.expected)?;

    let exe = if program.extension().is_some_and(|e| e == "py") {
        let py = crate::detect_toolchain(shuati_core::Lang::Python, "")
            .ok_or(JudgeError::CompilerNotFound("python".into()))?;
        py
    } else {
        program.clone()
    };

    let mut cmd = Command::new(&exe);
    if exe != *program {
        cmd.arg(program);
    }
    let out = cmd
        .arg(&in_f)
        .arg(&act_f)
        .arg(&exp_f)
        .output()
        .await
        .map_err(JudgeError::Io)?;

    Ok(if out.status.success() { Verdict::Ac } else { Verdict::Wa })
}
