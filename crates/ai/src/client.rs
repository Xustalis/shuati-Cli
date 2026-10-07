//! OpenAI 兼容 chat 客户端（DeepSeek/通义/Kimi 等同协议服务通用）。

use crate::{AiError, Result};
use serde_json::json;

pub struct AiClient {
    pub api_base: String,
    pub api_key: String,
    pub model: String,
    client: reqwest::Client,
}

impl AiClient {
    pub fn new(api_base: &str, api_key: &str, model: &str) -> Result<Self> {
        if api_key.trim().is_empty() {
            return Err(AiError::NotConfigured);
        }
        let client = reqwest::Client::builder()
            .timeout(std::time::Duration::from_secs(120))
            .build()
            .map_err(|e| AiError::Api(e.to_string()))?;
        Ok(Self {
            api_base: api_base.trim_end_matches('/').to_string(),
            api_key: api_key.to_string(),
            model: model.to_string(),
            client,
        })
    }

    /// 单轮 chat
    pub async fn chat(&self, system: &str, user: &str) -> Result<String> {
        let url = format!("{}/chat/completions", self.api_base);
        let body = json!({
            "model": self.model,
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
            "temperature": 0.3,
            "max_tokens": 4096,
            "stream": false,
        });
        let resp = self
            .client
            .post(&url)
            .bearer_auth(&self.api_key)
            .json(&body)
            .send()
            .await
            .map_err(|e| AiError::Api(e.to_string()))?;
        let status = resp.status();
        let text = resp.text().await.map_err(|e| AiError::Api(e.to_string()))?;
        if !status.is_success() {
            return Err(AiError::Api(format!("HTTP {status}: {}", &text[..text.len().min(300)])));
        }
        let v: serde_json::Value =
            serde_json::from_str(&text).map_err(|e| AiError::Parse(e.to_string()))?;
        v.get("choices")
            .and_then(|c| c.get(0))
            .and_then(|c| c.get("message"))
            .and_then(|m| m.get("content"))
            .and_then(|c| c.as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| AiError::Parse("no content".into()))
    }

    /// 提示：不给完整答案，只指方向
    pub async fn hint(&self, statement: &str, code: &str, verdict_info: &str) -> Result<String> {
        let user = format!(
            "题目:\n{statement}\n\n我的代码:\n```\n{code}\n```\n\n判题结果: {verdict_info}\n\n请给一个提示：指出大概方向，但不要直接给完整代码。"
        );
        self.chat(crate::prompts::HINT_SYS, &user).await
    }

    /// 诊断：解释为什么错
    pub async fn explain(&self, statement: &str, code: &str, verdict_info: &str, io_detail: &str) -> Result<String> {
        let user = format!(
            "题目:\n{statement}\n\n我的代码:\n```\n{code}\n```\n\n判题结果: {verdict_info}\n\n相关输入输出:\n```\n{io_detail}\n```\n\n请诊断出错原因，简洁指出 bug 在哪。"
        );
        self.chat(crate::prompts::EXPLAIN_SYS, &user).await
    }

    /// 生成对拍脚本对（gen + oracle）
    pub async fn gen_scripts(&self, statement: &str, lang: &str) -> Result<crate::StressScripts> {
        let user = format!(
            "题目:\n{statement}\n\n请用 {lang} 写出 <gen>（随机数据生成器，argv[1] 为 seed）和 <oracle>（暴力标程）。"
        );
        let text = self.chat(crate::prompts::GEN_SYS, &user).await?;
        crate::scripts::parse(&text).ok_or_else(|| AiError::Parse("响应中没有 <gen>/<oracle> 块".into()))
    }

    /// 修复脚本：带错误信息再要一次
    pub async fn repair_scripts(&self, statement: &str, lang: &str, broken: &crate::StressScripts, error: &str) -> Result<crate::StressScripts> {
        let user = format!(
            "题目:\n{statement}\n\n上次生成的脚本运行出错:\n{error}\n\n坏的 gen:\n```{lang}\n{}\n```\n坏的 oracle:\n```{lang}\n{}\n```\n\n请修正后重新输出完整的 <gen> 和 <oracle>（{lang}）。",
            broken.generator, broken.oracle
        );
        let text = self.chat(crate::prompts::GEN_SYS, &user).await?;
        crate::scripts::parse(&text).ok_or_else(|| AiError::Parse("响应中没有 <gen>/<oracle> 块".into()))
    }
}
