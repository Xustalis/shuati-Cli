//! Codeforces：
//! - API（contest.standings）拿标题/难度/tags —— 稳定不被拦
//! - HTML 页抓题面+样例 —— Cloudflare 防护下可能 403，失败时降级为仅元数据

use crate::{FetchError, FetchedProblem, Provider, http};
use regex::Regex;
use scraper::{Html, Selector};
use shuati_core::{ProblemId, Source};
use std::sync::LazyLock;

static URL_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"(?:problemset/problem|contest|gym)/(\d+)(?:/problem)?/([A-Za-z]\d*)").unwrap());

pub struct CodeforcesProvider;

impl Default for CodeforcesProvider {
    fn default() -> Self {
        Self::new()
    }
}

impl CodeforcesProvider {
    pub fn new() -> Self {
        Self
    }
}

/// CF rating → 难度词
fn rating_str(r: u64) -> String {
    match r {
        ..=1000 => "入门",
        1001..=1400 => "普及",
        1401..=1800 => "提高",
        1801..=2200 => "省选",
        _ => "NOI",
    }
    .to_string()
}

/// pre 块文本：CF 把行包在 div.test-example-line，无包裹时是 <br> 分行
fn pre_text(el: &scraper::ElementRef) -> String {
    let sel_line = Selector::parse("div.test-example-line").unwrap();
    let lines: Vec<String> = el.select(&sel_line).map(|d| d.text().collect::<String>()).collect();
    if !lines.is_empty() {
        return lines.join("\n") + "\n";
    }
    let html = el.inner_html().replace("<br>", "\n").replace("<br/>", "\n").replace("<br />", "\n");
    let re = Regex::new(r"<[^>]+>").unwrap();
    re.replace_all(&html, "").to_string()
}

/// API 元数据
struct ApiMeta {
    title: String,
    rating: Option<u64>,
    tags: Vec<String>,
}

async fn fetch_meta(contest: &str, index: &str) -> Option<ApiMeta> {
    // 官方限制：standings 只能匿名无参 GET
    let url = format!("https://codeforces.com/api/contest.standings?contestId={contest}");
    let text = http::get_text(&url).await.ok()?;
    let v: serde_json::Value = serde_json::from_str(&text).ok()?;
    if v.get("status")?.as_str()? != "OK" {
        return None;
    }
    let problems = v.get("result")?.get("problems")?.as_array()?;
    for p in problems {
        if p.get("index").and_then(|i| i.as_str()) == Some(index) {
            return Some(ApiMeta {
                title: p.get("name")?.as_str()?.to_string(),
                rating: p.get("rating").and_then(|r| r.as_u64()),
                tags: p
                    .get("tags")
                    .and_then(|t| t.as_array())
                    .map(|a| a.iter().filter_map(|x| x.as_str().map(String::from)).collect())
                    .unwrap_or_default(),
            });
        }
    }
    None
}

struct PageData {
    statement: String,
    samples: Vec<(String, String)>,
    time_ms: Option<u64>,
    mem_mb: Option<u64>,
}

/// 元素文本节点拼接（带空格分隔，避免相邻节点粘连）
fn el_text(el: &scraper::ElementRef) -> String {
    el.text().collect::<Vec<_>>().join(" ").trim().to_string()
}

/// 纯函数：从 problem-statement HTML 提取 limits/样例/题面段落（可单测）
fn parse_page(html: &str) -> Result<PageData, FetchError> {
    let doc = Html::parse_document(html);
    let stmt_el = doc.select(&Selector::parse("div.problem-statement").unwrap()).next().ok_or(FetchError::NotFound)?;

    static NUM_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"(\d+\.?\d*)").unwrap());
    let first_num = |el: &scraper::ElementRef| -> Option<f64> {
        let t = el_text(el);
        NUM_RE.find(&t).and_then(|m| m.as_str().parse::<f64>().ok())
    };

    let time_ms = stmt_el
        .select(&Selector::parse("div.time-limit").unwrap())
        .next()
        .and_then(|el| first_num(&el))
        .map(|sec| (sec * 1000.0) as u64);
    let mem_mb = stmt_el
        .select(&Selector::parse("div.memory-limit").unwrap())
        .next()
        .and_then(|el| first_num(&el))
        .map(|mb| mb as u64);

    let ins: Vec<String> =
        stmt_el.select(&Selector::parse("div.sample-test div.input pre").unwrap()).map(|e| pre_text(&e)).collect();
    let outs: Vec<String> =
        stmt_el.select(&Selector::parse("div.sample-test div.output pre").unwrap()).map(|e| pre_text(&e)).collect();
    let samples: Vec<(String, String)> = ins.into_iter().zip(outs).collect();

    // 题面段落
    let mut statement = String::new();
    for p in stmt_el.select(&Selector::parse("p").unwrap()) {
        let t = p.text().collect::<String>();
        let t = t.trim();
        if !t.is_empty() {
            statement.push_str(t);
            statement.push_str("\n\n");
        }
    }

    Ok(PageData { statement, samples, time_ms, mem_mb })
}

/// 尝试爬 HTML 题面（可能被 Cloudflare 403）
async fn fetch_page(contest: &str, index: &str) -> Result<PageData, FetchError> {
    let url = format!("https://codeforces.com/problemset/problem/{contest}/{index}");
    let html = http::get_text(&url).await?;
    if html.contains("Your browser is being checked") || html.len() < 20000 {
        return Err(FetchError::Network("Cloudflare 拦截".into()));
    }
    parse_page(&html)
}

#[async_trait::async_trait]
impl Provider for CodeforcesProvider {
    fn name(&self) -> &'static str {
        "codeforces"
    }

    fn matches(&self, url: &str) -> bool {
        url.contains("codeforces.com")
    }

    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError> {
        let caps = URL_RE.captures(url).ok_or_else(|| FetchError::UnsupportedUrl(url.to_string()))?;
        let contest = caps[1].to_string();
        let index = caps[2].to_uppercase();
        let slug = format!("{contest}{index}");
        let page_url = format!("https://codeforces.com/problemset/problem/{contest}/{index}");

        // 并拉：API 元数据 + HTML 页面
        let (meta, page) = tokio::join!(fetch_meta(&contest, &index), fetch_page(&contest, &index));

        let title = meta.as_ref().map(|m| format!("{contest}{index}. {}", m.title)).unwrap_or_else(|| slug.clone());

        let (statement, samples, time_ms, mem_mb) = match page {
            Ok(pd) => (pd.statement, pd.samples, pd.time_ms, pd.mem_mb),
            Err(e) => {
                tracing::warn!("CF 页面抓取失败({e})，仅保存元数据");
                (
                    format!("题面抓取失败（{e}）。CF 有反爬防护，请到 {page_url} 手动复制题面和样例。"),
                    vec![],
                    None,
                    None,
                )
            }
        };

        let mut stmt = format!("# {title}\n\n> {page_url}\n\n{statement}");
        for (i, (inp, outp)) in samples.iter().enumerate() {
            stmt.push_str(&format!(
                "## 样例 {}\n输入:\n```\n{}\n```\n输出:\n```\n{}\n```\n\n",
                i + 1,
                inp.trim_end(),
                outp.trim_end()
            ));
        }

        Ok(FetchedProblem {
            id: ProblemId::new(Source::Codeforces, slug),
            title,
            url: page_url,
            statement: stmt,
            samples,
            time_limit_ms: time_ms,
            memory_limit_mb: mem_mb,
            tags: meta.as_ref().map(|m| m.tags.clone()).unwrap_or_default(),
            difficulty: meta.and_then(|m| m.rating.map(rating_str)),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // 模拟 CF problem-statement 的真实 DOM 结构（文本节点在 div 下混杂排布）
    const FIXTURE: &str = r##"
<html><body>
<div class="problem-statement">
  <div class="header">
    <div class="title">A. Add Two Numbers</div>
    <div class="time-limit"><div class="property-title">time limit per test</div>2 seconds</div>
    <div class="memory-limit"><div class="property-title">memory limit per test</div>256 megabytes</div>
  </div>
  <div><p>Given two integers, print their sum.</p></div>
  <div class="sample-test">
    <div class="input"><div class="title">Input</div>
      <pre><div class="test-example-line">1 2</div></pre>
    </div>
    <div class="output"><div class="title">Output</div>
      <pre><div class="test-example-line">3</div></pre>
    </div>
  </div>
</div>
</body></html>"##;

    #[test]
    fn parses_limits_from_property_divs() {
        let d = parse_page(FIXTURE).unwrap();
        assert_eq!(d.time_ms, Some(2000));
        assert_eq!(d.mem_mb, Some(256));
    }

    #[test]
    fn parses_samples_and_statement() {
        let d = parse_page(FIXTURE).unwrap();
        assert_eq!(d.samples, vec![("1 2\n".to_string(), "3\n".to_string())]);
        assert!(d.statement.contains("print their sum"));
    }

    #[test]
    fn missing_statement_is_notfound() {
        assert!(matches!(parse_page("<html><body></body></html>"), Err(FetchError::NotFound)));
    }
}
