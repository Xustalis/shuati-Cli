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
            let preview: String = text.chars().take(300).collect();
            return Err(AiError::Api(format!("HTTP {status}: {preview}")));
        }
        let v: serde_json::Value = serde_json::from_str(&text).map_err(|e| AiError::Parse(e.to_string()))?;
        v.get("choices")
            .and_then(|c| c.get(0))
            .and_then(|c| c.get("message"))
            .and_then(|m| m.get("content"))
            .and_then(|c| c.as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| AiError::Parse("no content".into()))
    }

    /// 单轮 chat，SSE 流式：on_delta 逐段收到增量文本，返回完整拼接结果。
    /// UI 可以实时滚动显示，不用干等几十秒。
    pub async fn chat_stream<F: FnMut(&str) + Send>(
        &self,
        system: &str,
        user: &str,
        on_delta: &mut F,
    ) -> Result<String> {
        let url = format!("{}/chat/completions", self.api_base);
        let body = json!({
            "model": self.model,
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
            "temperature": 0.3,
            "max_tokens": 4096,
            "stream": true,
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
        if !status.is_success() {
            let text = resp.text().await.map_err(|e| AiError::Api(e.to_string()))?;
            let preview: String = text.chars().take(300).collect();
            return Err(AiError::Api(format!("HTTP {status}: {preview}")));
        }

        use futures_util::StreamExt;
        let mut acc = String::new();
        let mut buf = String::new();
        let mut stream = resp.bytes_stream();
        while let Some(chunk) = stream.next().await {
            let chunk = chunk.map_err(|e| AiError::Api(e.to_string()))?;
            // SSE 事件以空行分隔；\r\n 服务器先归一化成 \n
            buf.push_str(&String::from_utf8_lossy(&chunk).replace('\r', ""));
            while let Some(pos) = buf.find("\n\n") {
                // 事件体 + 分隔空行一起出队，lines() 会忽略尾部空行
                let event: String = buf.drain(..pos + 2).collect();
                for piece in parse_sse_event(&event) {
                    match piece {
                        SsePiece::Done => return Ok(acc),
                        SsePiece::Text(d) => {
                            acc.push_str(&d);
                            on_delta(&d);
                        }
                    }
                }
            }
        }
        if acc.is_empty() {
            return Err(AiError::Parse("stream ended without content".into()));
        }
        Ok(acc)
    }

    /// 流式提示
    pub async fn hint_stream<F: FnMut(&str) + Send>(
        &self,
        statement: &str,
        code: &str,
        verdict_info: &str,
        on_delta: &mut F,
    ) -> Result<String> {
        let user = format!(
            "题目:\n{statement}\n\n我的代码:\n```\n{code}\n```\n\n判题结果: {verdict_info}\n\n请给一个提示：指出大概方向，但不要直接给完整代码。"
        );
        self.chat_stream(crate::prompts::HINT_SYS, &user, on_delta).await
    }

    /// 流式诊断
    pub async fn explain_stream<F: FnMut(&str) + Send>(
        &self,
        statement: &str,
        code: &str,
        verdict_info: &str,
        io_detail: &str,
        on_delta: &mut F,
    ) -> Result<String> {
        let user = format!(
            "题目:\n{statement}\n\n我的代码:\n```\n{code}\n```\n\n判题结果: {verdict_info}\n\n相关输入输出:\n```\n{io_detail}\n```\n\n请诊断出错原因，简洁指出 bug 在哪。"
        );
        self.chat_stream(crate::prompts::EXPLAIN_SYS, &user, on_delta).await
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
    pub async fn repair_scripts(
        &self,
        statement: &str,
        lang: &str,
        broken: &crate::StressScripts,
        error: &str,
    ) -> Result<crate::StressScripts> {
        let user = format!(
            "题目:\n{statement}\n\n上次生成的脚本运行出错:\n{error}\n\n坏的 gen:\n```{lang}\n{}\n```\n坏的 oracle:\n```{lang}\n{}\n```\n\n请修正后重新输出完整的 <gen> 和 <oracle>（{lang}）。",
            broken.generator, broken.oracle
        );
        let text = self.chat(crate::prompts::GEN_SYS, &user).await?;
        crate::scripts::parse(&text).ok_or_else(|| AiError::Parse("响应中没有 <gen>/<oracle> 块".into()))
    }
}

/// SSE 事件里能拆出的东西
enum SsePiece {
    Done,
    Text(String),
}

/// 解析一个 SSE 事件体（空行分隔的一段）：提取所有 data: 行里的 delta.content。
/// 非 data 行（comment/event:/id:）和坏 JSON 一律跳过——容错优先。
fn parse_sse_event(event: &str) -> Vec<SsePiece> {
    event
        .lines()
        .filter_map(|line| {
            let data = line.strip_prefix("data:")?.trim();
            if data == "[DONE]" {
                return Some(SsePiece::Done);
            }
            let v = serde_json::from_str::<serde_json::Value>(data).ok()?;
            v.pointer("/choices/0/delta/content")
                .and_then(|c| c.as_str())
                .filter(|s| !s.is_empty())
                .map(|s| SsePiece::Text(s.to_string()))
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sse_event_extracts_delta_text() {
        let ev = "data: {\"choices\":[{\"delta\":{\"content\":\"你好\"}}]}\n";
        let pieces = parse_sse_event(ev);
        assert!(matches!(&pieces[..], [SsePiece::Text(t)] if t == "你好"));
    }

    #[test]
    fn sse_event_handles_done_and_junk() {
        assert!(matches!(&parse_sse_event("data: [DONE]\n")[..], [SsePiece::Done]));
        assert!(parse_sse_event("data: not-json\n").is_empty());
        assert!(parse_sse_event(": keep-alive\n\nevent: ping\n").is_empty());
        // role-only delta（首包常见）没有 content → 忽略
        assert!(parse_sse_event("data: {\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}\n").is_empty());
    }

    #[test]
    fn sse_event_multi_line_data() {
        // 一个事件多 data 行（少见但合法）：全都要
        let ev = "data: {\"choices\":[{\"delta\":{\"content\":\"a\"}}]}\ndata: {\"choices\":[{\"delta\":{\"content\":\"b\"}}]}\n";
        let texts: Vec<String> = parse_sse_event(ev)
            .into_iter()
            .filter_map(|p| match p {
                SsePiece::Text(t) => Some(t),
                _ => None,
            })
            .collect();
        assert_eq!(texts, ["a", "b"]);
    }
}
