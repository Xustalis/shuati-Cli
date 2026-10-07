use crate::{ProblemId, Verdict};
use serde::{Deserialize, Serialize};

/// 单个测试点的结果
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CaseResult {
    pub name: String,
    pub verdict: Verdict,
    pub cpu_ms: u64,
    pub memory_kb: u64,
    /// 失败详情（编译错误 / RE stderr / WA 摘要）
    #[serde(default)]
    pub message: String,
    /// 反序列化时可选，供报告落盘
    #[serde(default)]
    pub input: String,
    #[serde(default)]
    pub actual: String,
    #[serde(default)]
    pub expected: String,
}

/// 一次评测的完整报告
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestReport {
    pub problem: ProblemId,
    pub timestamp: i64,
    pub kind: RunKind,
    pub verdict: Verdict,
    pub pass: usize,
    pub total: usize,
    pub cases: Vec<CaseResult>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum RunKind {
    Test,
    Stress,
    Gen,
}
