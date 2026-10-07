//! shuati-fetch: OJ 题目拉取。M3 里程碑实现具体平台。
//! 先定 trait 契约，URL 路由与各平台 Provider。

use shuati_core::ProblemId;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum FetchError {
    #[error("unsupported url: {0}")]
    UnsupportedUrl(String),
    #[error("network: {0}")]
    Network(String),
    #[error("parse: {0}")]
    Parse(String),
    #[error("not implemented: {0}")]
    NotImplemented(String),
}

/// 拉到的原始题目内容（store 层负责落盘）
#[derive(Debug)]
pub struct FetchedProblem {
    pub id: ProblemId,
    pub title: String,
    pub url: String,
    /// markdown 题面
    pub statement: String,
    /// (input, output) 样例对
    pub samples: Vec<(String, String)>,
    pub time_limit_ms: Option<u64>,
    pub memory_limit_mb: Option<u64>,
    pub tags: Vec<String>,
    pub difficulty: Option<String>,
}

/// 平台 Provider
#[async_trait::async_trait]
pub trait Provider: Send + Sync {
    /// 该 URL 是否归本平台管
    fn matches(&self, url: &str) -> bool;
    /// 拉取题目
    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError>;
}

/// URL → Provider 路由。M3 逐个注册实现。
pub fn route(_url: &str) -> Option<Box<dyn Provider>> {
    None
}
