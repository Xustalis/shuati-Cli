//! 洛谷：页面内嵌 <script id="lentille-context" type="application/json">。

use crate::{http, FetchError, FetchedProblem, Provider};
use regex::Regex;
use shuati_core::{ProblemId, Source};
use std::sync::LazyLock;

static PID_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"problem/(P\d+|B\d+|AT\w+|CF\w+|SP\w+|UVA\w+)").unwrap());

pub struct LuoguProvider;

impl LuoguProvider {
    pub fn new() -> Self {
        Self
    }
}

/// 从页面 HTML 提取 lentille-context JSON
fn extract_lentille(html: &str) -> Result<serde_json::Value, FetchError> {
    static RE: LazyLock<Regex> = LazyLock::new(|| {
        Regex::new(r#"(?s)<script[^>]+id="lentille-context"[^>]*>(.*?)</script>"#).unwrap()
    });
    let caps = RE
        .captures(html)
        .ok_or_else(|| FetchError::Parse("no lentille-context".into()))?;
    let raw = caps[1].trim();
    // 优先按原始 JSON 解析；失败再按 HTML-encode 解析
    if let Ok(v) = serde_json::from_str(raw) {
        return Ok(v);
    }
    let decoded = raw
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&amp;", "&");
    serde_json::from_str(&decoded).map_err(|e| FetchError::Parse(format!("lentille json: {e}")))
}

/// 洛谷 difficulty 数值 → 字符串
fn difficulty_str(d: i64) -> Option<String> {
    Some(
        match d {
            0 => "暂无评定",
            1 => "入门",
            2 => "普及−",
            3 => "普及/提高−",
            4 => "普及+/提高",
            5 => "提高+/省选−",
            6 => "省选/NOI−",
            7 => "NOI/NOI+/CTSC",
            _ => return None,
        }
        .to_string(),
    )
}

#[async_trait::async_trait]
impl Provider for LuoguProvider {
    fn name(&self) -> &'static str {
        "luogu"
    }

    fn matches(&self, url: &str) -> bool {
        url.contains("luogu.com.cn")
    }

    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError> {
        let caps = PID_RE
            .captures(url)
            .ok_or_else(|| FetchError::UnsupportedUrl(url.to_string()))?;
        let slug = caps[1].to_string();

        // 页面内嵌 <script id="lentille-context"> 里有完整 JSON
        let page = format!("https://www.luogu.com.cn/problem/{slug}");
        let html = http::get_text(&page).await?;
        let root = extract_lentille(&html)?;

        // lentille 结构：{ data: { problem: {...} } }（旧版是 currentData.problem，兼容两种）
        let prob = ["data", "currentData"]
            .iter()
            .find_map(|k| root.get(k).and_then(|d| d.get("problem")))
            .ok_or_else(|| FetchError::Parse("no problem in response".into()))?;

        // 题面内容在 contenu（法语 locale 优先字段）或 content 里
        let body = prob
            .get("contenu")
            .or_else(|| prob.get("content"))
            .filter(|c| c.is_object())
            .unwrap_or(prob);

        let get = |k: &str| {
            body.get(k)
                .and_then(|v| v.as_str())
                .or_else(|| prob.get(k).and_then(|v| v.as_str()))
        };

        let title = get("name")
            .or_else(|| get("title"))
            .unwrap_or(&slug)
            .to_string();

        // 样例 [[in, out], ...]
        let samples: Vec<(String, String)> = prob
            .get("samples")
            .and_then(|s| s.as_array())
            .map(|arr| {
                arr.iter()
                    .filter_map(|p| {
                        let pair = p.as_array()?;
                        if pair.len() >= 2 {
                            Some((
                                pair[0].as_str().unwrap_or("").to_string(),
                                pair[1].as_str().unwrap_or("").to_string(),
                            ))
                        } else {
                            None
                        }
                    })
                    .collect()
            })
            .unwrap_or_default();

        // 时限/内存：limits.time = [ms,...], limits.memory = [KB,...]
        let time_ms = prob
            .get("limits")
            .and_then(|l| l.get("time"))
            .and_then(|t| t.as_array())
            .and_then(|a| a.iter().filter_map(|v| v.as_u64()).max());
        let mem_mb = prob
            .get("limits")
            .and_then(|l| l.get("memory"))
            .and_then(|t| t.as_array())
            .and_then(|a| a.iter().filter_map(|v| v.as_u64()).max())
            .map(|kb| kb / 1024);

        let difficulty = prob
            .get("difficulty")
            .and_then(|d| d.as_i64())
            .and_then(difficulty_str);

        let tags: Vec<String> = prob
            .get("tags")
            .and_then(|t| t.as_array())
            .map(|arr| {
                // tags 是数字 id 数组，映射表不完整就放 id；够用
                arr.iter()
                    .filter_map(|v| v.as_i64().map(|i| i.to_string()))
                    .collect()
            })
            .unwrap_or_default();

        // 题面组装 markdown（洛谷字段：formatI/formatO）
        let mut stmt = format!("# {title}\n\n> {url}\n\n");
        for (key, label) in [
            ("background", "题目背景"),
            ("description", "题目描述"),
            ("formatI", "输入格式"),
            ("formatO", "输出格式"),
            ("hint", "说明/提示"),
        ] {
            if let Some(s) = get(key) {
                let s = s.trim();
                if !s.is_empty() {
                    stmt.push_str(&format!("## {label}\n\n{s}\n\n"));
                }
            }
        }
        if !samples.is_empty() {
            stmt.push_str("## 样例\n\n");
            for (i, (inp, outp)) in samples.iter().enumerate() {
                stmt.push_str(&format!("### 输入 {}\n```\n{}\n```\n### 输出 {}\n```\n{}\n```\n\n",
                    i + 1, inp.trim_end(), i + 1, outp.trim_end()));
            }
        }

        Ok(FetchedProblem {
            id: ProblemId::new(Source::Luogu, slug),
            title,
            url: url.to_string(),
            statement: stmt,
            samples,
            time_limit_ms: time_ms,
            memory_limit_mb: mem_mb,
            tags,
            difficulty,
        })
    }
}
