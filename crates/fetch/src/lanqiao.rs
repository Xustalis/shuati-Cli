//! 蓝桥云课（lanqiao.cn）：`GET /api/v2/problems/{id}/`。
//! 题目内容需要登录：cookie 来自环境变量 `SHUATI_LANQIAO_COOKIE` 或
//! config.toml 的 `lanqiao_cookie`（经 Provider::configure 注入）。
//! 未登录/被拦时降级：只落标题 + 配置指引，样例为空由用户手动 `case` 补。

use crate::{FetchError, FetchedProblem, Provider, http, util};
use regex::Regex;
use shuati_core::{ProblemId, Source};
use std::sync::LazyLock;

static ID_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"problems/(\d+)").unwrap());

pub struct LanqiaoProvider {
    cookie: Option<String>,
}

impl Default for LanqiaoProvider {
    fn default() -> Self {
        Self::new()
    }
}

impl LanqiaoProvider {
    pub fn new() -> Self {
        Self { cookie: None }
    }
}

/// 题面字段常见 key（API 返回结构随栏目类型不同，逐个试）
const CONTENT_KEYS: &[&str] = &[
    "content",
    "description",
    "body",
    "detail",
    "problem_description",
    "statement",
    "problem",
    "introduction",
    "html",
];

/// 在 obj 及其嵌套对象里找题面 HTML 字符串
fn find_description(json: &serde_json::Value) -> String {
    let in_obj = |o: &serde_json::Value| -> Option<String> {
        CONTENT_KEYS.iter().find_map(|k| o.get(k).and_then(|v| v.as_str()).map(String::from)).filter(|s| !s.is_empty())
    };
    if let Some(d) = in_obj(json) {
        return d;
    }
    for nest in ["oj", "question_answer", "data"] {
        if let Some(d) = json.get(nest).and_then(&in_obj) {
            return d;
        }
    }
    // 深搜：找最长的像 HTML/题面的字符串
    fn deep<'a>(v: &'a serde_json::Value, best: &mut Option<&'a str>) {
        match v {
            serde_json::Value::Object(m) => m.values().for_each(|x| deep(x, best)),
            serde_json::Value::Array(a) => a.iter().for_each(|x| deep(x, best)),
            serde_json::Value::String(s) => {
                let looks = s.contains("<p") || s.contains("样例") || s.contains("输入") || s.len() > 50;
                if looks && best.map(|b| s.len() > b.len()).unwrap_or(true) {
                    *best = Some(s);
                }
            }
            _ => {}
        }
    }
    let mut best = None;
    deep(json, &mut best);
    best.unwrap_or("").to_string()
}

fn difficulty_str(v: &serde_json::Value) -> Option<String> {
    let s = match v {
        serde_json::Value::String(s) => match s.as_str() {
            "简单" | "easy" | "Easy" => "简单",
            "困难" | "hard" | "Hard" => "困难",
            _ => "中等",
        },
        serde_json::Value::Number(n) => match n.as_i64() {
            Some(1) => "简单",
            Some(3) => "困难",
            _ => "中等",
        },
        _ => return None,
    };
    Some(s.to_string())
}

fn tags_of(json: &serde_json::Value) -> Vec<String> {
    json.get("tags")
        .and_then(|t| t.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|x| {
                    x.as_str().map(String::from).or_else(|| x.get("name").and_then(|n| n.as_str()).map(String::from))
                })
                .collect()
        })
        .unwrap_or_default()
}

fn samples_of(json: &serde_json::Value) -> Vec<(String, String)> {
    json.get("examples")
        .and_then(|e| e.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|ex| {
                    let inp = ex.get("input").and_then(|v| v.as_str())?.trim().to_string();
                    let out = ex.get("output").and_then(|v| v.as_str()).unwrap_or("").trim().to_string();
                    Some((inp, out))
                })
                .collect()
        })
        .unwrap_or_default()
}

/// API JSON → FetchedProblem（纯函数，可单测）
fn parse_api(json: &serde_json::Value, id: &str, url: &str) -> FetchedProblem {
    let obj = json.get("data").filter(|d| d.is_object()).unwrap_or(json);
    let title = obj
        .get("title")
        .and_then(|v| v.as_str())
        .or_else(|| obj.get("name").and_then(|v| v.as_str()))
        .map(|t| format!("LQ{id}. {t}"))
        .unwrap_or_else(|| format!("蓝桥云课 #{id}"));

    let mut stmt = format!("# {title}\n\n> {url}\n\n");
    let desc = find_description(obj);
    if desc.is_empty() {
        stmt.push_str("题面内容请到蓝桥云课网站查看。");
    } else {
        stmt.push_str(&util::html_to_text(&desc));
    }
    stmt.push('\n');

    FetchedProblem {
        id: ProblemId::new(Source::Lanqiao, id.to_string()),
        title,
        url: url.to_string(),
        statement: stmt,
        samples: samples_of(obj),
        time_limit_ms: None,
        memory_limit_mb: None,
        tags: tags_of(obj),
        difficulty: obj.get("difficulty").and_then(difficulty_str),
    }
}

/// 未登录/抓取失败时的降级题目（保住 id/目录，样例靠手动补）
fn degraded(id: &str, url: &str, why: &str, has_cookie: bool) -> FetchedProblem {
    let hint = if has_cookie {
        "已配置 Cookie 仍无法获取（Cookie 可能过期）。更新 config.toml 的 lanqiao_cookie 后重新 `shuati add`。"
    } else {
        "蓝桥云课题目需要登录才能看到完整内容。把浏览器 Cookie 写进 config.toml 的 \
         `lanqiao_cookie` 字段（或设环境变量 SHUATI_LANQIAO_COOKIE）后重新 `shuati add`；\
         也可以在评测页手动补题面和样例。"
    };
    FetchedProblem {
        id: ProblemId::new(Source::Lanqiao, id.to_string()),
        title: format!("蓝桥云课 #{id}"),
        url: url.to_string(),
        statement: format!("# 蓝桥云课 #{id}\n\n> {url}\n\n> ⚠ {hint}\n\n抓取详情：{why}\n"),
        samples: vec![],
        time_limit_ms: None,
        memory_limit_mb: None,
        tags: vec![],
        difficulty: None,
    }
}

#[async_trait::async_trait]
impl Provider for LanqiaoProvider {
    fn name(&self) -> &'static str {
        "lanqiao"
    }

    fn matches(&self, url: &str) -> bool {
        url.contains("lanqiao.cn")
    }

    fn configure(&mut self, cfg: &shuati_store::Config) {
        // 环境变量优先，config.toml 兜底（与 api_key 的优先级约定一致）
        self.cookie = std::env::var("SHUATI_LANQIAO_COOKIE")
            .ok()
            .filter(|s| !s.trim().is_empty())
            .or_else(|| Some(cfg.lanqiao_cookie.trim().to_string()).filter(|s| !s.is_empty()));
    }

    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError> {
        let caps = ID_RE.captures(url).ok_or_else(|| FetchError::UnsupportedUrl(url.to_string()))?;
        let id = caps[1].to_string();
        let api = format!("https://www.lanqiao.cn/api/v2/problems/{id}/");

        let mut headers: Vec<(&str, &str)> = vec![("Accept", "application/json"), ("Referer", url)];
        if let Some(c) = &self.cookie {
            headers.push(("Cookie", c));
        }
        match http::get(&api, &headers).await {
            Ok(text) => match serde_json::from_str::<serde_json::Value>(&text) {
                Ok(v) => Ok(parse_api(&v, &id, url)),
                Err(e) => Ok(degraded(&id, url, &format!("JSON 解析失败: {e}"), self.cookie.is_some())),
            },
            Err(e) => {
                let why = match &e {
                    FetchError::Network(m) => m.clone(),
                    other => other.to_string(),
                };
                Ok(degraded(&id, url, &why, self.cookie.is_some()))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_full_api_response() {
        let v = serde_json::json!({
            "title": "门牌制作", "difficulty": "简单", "tags": ["枚举", {"name": "模拟"}],
            "content": "<p>小蓝要为门牌号制作数字。</p><p>输入一个数。</p>",
            "examples": [{"input": "2020", "output": "624"}]
        });
        let fp = parse_api(&v, "1234", "https://www.lanqiao.cn/problems/1234/learning/");
        assert_eq!(fp.id.to_string(), "lanqiao:1234");
        assert_eq!(fp.title, "LQ1234. 门牌制作");
        assert_eq!(fp.difficulty.as_deref(), Some("简单"));
        assert_eq!(fp.tags, vec!["枚举", "模拟"]);
        assert_eq!(fp.samples, vec![("2020".to_string(), "624".to_string())]);
        assert!(fp.statement.contains("门牌号制作数字"));
    }

    #[test]
    fn nested_data_and_int_difficulty() {
        let v = serde_json::json!({
            "data": {"name": "测试题", "difficulty": 3, "description": "<p>描述</p>",
                     "examples": [{"input": "1", "output": "2"}]}
        });
        let fp = parse_api(&v, "5", "u");
        assert_eq!(fp.title, "LQ5. 测试题");
        assert_eq!(fp.difficulty.as_deref(), Some("困难"));
        assert!(fp.statement.contains("描述"));
        assert_eq!(fp.samples.len(), 1);
    }

    #[test]
    fn desc_in_nested_oj() {
        let v = serde_json::json!({"name": "t", "oj": {"content": "<p>题面在 oj 里</p>"}});
        let fp = parse_api(&v, "9", "u");
        assert!(fp.statement.contains("题面在 oj 里"));
    }

    #[test]
    fn deep_search_finds_longest_html() {
        let v = serde_json::json!({"weird": {"nested": ["short", "<p>这是一段很长的题面内容，超过五十字……………………………………………………</p>"]}});
        let fp = parse_api(&v, "7", "u");
        assert!(fp.statement.contains("很长的题面内容"));
    }
}
