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
        CheckerSpec::Spj { .. } => Err(JudgeError::Other("spj checker requires async check_async".into())),
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
        crate::detect_toolchain(shuati_core::Lang::Python, "").ok_or(JudgeError::CompilerNotFound("python".into()))?
    } else {
        program.clone()
    };

    let mut cmd = Command::new(&exe);
    cmd.kill_on_drop(true);
    if exe != *program {
        cmd.arg(program);
    }
    // spj 最坏 10s：坏掉的 checker 不能把引擎挂死
    let out = tokio::time::timeout(std::time::Duration::from_secs(10), cmd.arg(&in_f).arg(&act_f).arg(&exp_f).output())
        .await
        .map_err(|_| JudgeError::Other("spj checker timeout (10s)".into()))?
        .map_err(JudgeError::Io)?;

    Ok(if out.status.success() { Verdict::Ac } else { Verdict::Wa })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ctx<'a>(input: &'a str, actual: &'a str, expected: &'a str) -> CheckCtx<'a> {
        CheckCtx { input, actual, expected }
    }

    #[test]
    fn tokens_ignores_whitespace_layout() {
        assert_eq!(check_tokens("1 2\n3\n", "1 2 3"), Verdict::Ac);
        assert_eq!(check_tokens("  1  2  ", "1 2"), Verdict::Ac);
        assert_eq!(check_tokens("1 2", "1 3"), Verdict::Wa);
        assert_eq!(check_tokens("1 2", "1 2 3"), Verdict::Wa);
        assert_eq!(check_tokens("", ""), Verdict::Ac);
    }

    #[test]
    fn exact_is_byte_sensitive() {
        assert_eq!(check(&CheckerSpec::Exact, &ctx("", "1 2\n", "1 2\n")).unwrap(), Verdict::Ac);
        // 尾随换行差异在 exact 下是 WA，在 tokens 下是 AC——两种模式的边界要清楚
        assert_eq!(check(&CheckerSpec::Exact, &ctx("", "1 2\n", "1 2")).unwrap(), Verdict::Wa);
        assert_eq!(check(&CheckerSpec::Tokens, &ctx("", "1 2\n", "1 2")).unwrap(), Verdict::Ac);
    }

    #[test]
    fn float_eps_abs_and_rel() {
        let spec = CheckerSpec::Float { eps: 1e-4 };
        assert_eq!(check(&spec, &ctx("", "0.5", "0.50001")).unwrap(), Verdict::Ac); // abs
        assert_eq!(check(&spec, &ctx("", "1000.05", "1000.0")).unwrap(), Verdict::Ac); // rel
        assert_eq!(check(&spec, &ctx("", "0.5", "0.6")).unwrap(), Verdict::Wa);
        // 数量不等、非数字 token → WA
        assert_eq!(check(&spec, &ctx("", "1.0 2.0", "1.0")).unwrap(), Verdict::Wa);
        assert_eq!(check(&spec, &ctx("", "abc", "1.0")).unwrap(), Verdict::Wa);
        // NaN 不应被当成浮点相等
        assert_eq!(check(&spec, &ctx("", "NaN", "NaN")).unwrap(), Verdict::Ac); // 同字符串走 x==y 快速路
        assert_eq!(check(&spec, &ctx("", "NaN", "1.0")).unwrap(), Verdict::Wa);
    }
}
