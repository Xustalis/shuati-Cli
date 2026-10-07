//! shuati-ai: AI 助手。M4 里程碑实现。
//! 契约：Provider trait + 脚本生成自检管线。

use thiserror::Error;

#[derive(Debug, Error)]
pub enum AiError {
    #[error("ai not configured")]
    NotConfigured,
    #[error("api: {0}")]
    Api(String),
    #[error("bad response: {0}")]
    Parse(String),
}

/// AI 生成的对拍脚本对
#[derive(Debug)]
pub struct StressScripts {
    /// 生成器源码（收 seed 出测试输入）
    pub generator: String,
    /// 标程源码（读输入出答案）
    pub oracle: String,
    /// 语言（"cpp" | "python"）
    pub lang: String,
}
