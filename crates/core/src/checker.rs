use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// 比较器配置（problem.toml 可配）
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum CheckerSpec {
    /// 逐 token 比对（默认）
    Tokens,
    /// 字节级严格比对
    Exact,
    /// token 比对，数值 token 按 eps 容差
    Float { eps: f64 },
    /// 外部校验器，testlib 约定: `checker input actual expected`，exit 0 = AC
    Spj { program: PathBuf },
}

impl Default for CheckerSpec {
    fn default() -> Self {
        CheckerSpec::Tokens
    }
}

/// 传给 Checker 的上下文
pub struct CheckCtx<'a> {
    pub input: &'a str,
    pub actual: &'a str,
    pub expected: &'a str,
}
