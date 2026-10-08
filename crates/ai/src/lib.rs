//! shuati-ai: AI 助手。
//! - hint: 根据题面+代码+判题结果给提示（不泄题解）
//! - explain: 解释 WA/TLE/RE 原因
//! - gen_scripts: 生成对拍 gen/oracle，结构化输出 + 可重试

pub mod autogen;
mod client;
pub mod prompts;
pub mod scripts;

pub use autogen::{ScriptDirs, produce_scripts};
pub use client::AiClient;
pub use scripts::StressScripts;

use thiserror::Error;

#[derive(Debug, Error)]
pub enum AiError {
    #[error("ai 未配置（设置页填 api_key）")]
    NotConfigured,
    #[error("api: {0}")]
    Api(String),
    #[error("响应解析失败: {0}")]
    Parse(String),
}

pub type Result<T> = std::result::Result<T, AiError>;
