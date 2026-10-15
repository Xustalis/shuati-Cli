//! LeetCode（力扣）：GraphQL API。
//! - leetcode.cn / leetcode-cn.com → leetcode.cn/graphql（中文站，国内可直连）
//! - leetcode.com → leetcode.com/graphql
//!
//! 注意：LeetCode 是"函数式"题目（实现给定函数而非读写 stdin）。
//! 样例输入用官方 `exampleTestcaseList`（判题参数格式，每行一个参数），
//! 期望输出从题面 HTML 的 <pre> 块里按"输出/Output"标签解析，按顺序对齐。

use crate::{FetchError, FetchedProblem, Provider, http, util};
use regex::Regex;
use scraper::{Html, Selector};
use shuati_core::{ProblemId, Source};
use std::sync::LazyLock;

static SLUG_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"/problems/([^/?#]+)").unwrap());

const QUERY: &str = r#"query questionData($titleSlug: String!) {
    question(titleSlug: $titleSlug) {
        questionFrontendId
        title
        translatedTitle
        difficulty
        content
        translatedContent
        topicTags { name translatedName }
        exampleTestcaseList
    }
}"#;

pub struct LeetCodeProvider;

impl Default for LeetCodeProvider {
    fn default() -> Self {
        Self::new()
    }
}

impl LeetCodeProvider {
    pub fn new() -> Self {
        Self
    }
}

/// URL 用的站点（决定 GraphQL 端点和语言偏好）
fn is_cn(url: &str) -> bool {
    url.contains("leetcode.cn") || url.contains("leetcode-cn.com")
}

/// 题面 <pre> 块 → (展示用输入, 期望输出)。只收带输出标签的块。
fn sample_blocks(content_html: &str) -> Vec<(String, String)> {
    let frag = Html::parse_fragment(content_html);
    let pre_sel = Selector::parse("pre").unwrap();
    let mut out = Vec::new();
    for pre in frag.select(&pre_sel) {
        // 不加分隔符：strong 标签把"输入："和值切成相邻 text 节点，pre 本身有真实换行
        let text: String = pre.text().collect();
        if let Some(pair) = parse_pre(&text) {
            out.push(pair);
        }
    }
    out
}

/// 标签行：`输入：`/`输出：`/`解释：`/`Input:`/`Output:`/`Explanation:`（中英文冒号都认）
fn label<'a>(line: &'a str, names: &[&str]) -> Option<&'a str> {
    let t = line.trim();
    for n in names {
        if let Some(rest) = t.strip_prefix(n) {
            let rest = rest.strip_prefix(['：', ':']).unwrap_or(rest);
            return Some(rest.trim());
        }
    }
    None
}

/// 单块 <pre> 文本 → (input, output)。输出行后到下一个标签行之间的内容都归输出。
fn parse_pre(text: &str) -> Option<(String, String)> {
    const IN: &[&str] = &["输入", "Input"];
    const OUT: &[&str] = &["输出", "Output"];
    const STOP: &[&str] = &["解释", "Explanation", "提示", "Note"];
    let mut input = String::new();
    let mut output = String::new();
    let mut in_out = false;
    for line in text.lines() {
        if label(line, STOP).is_some() || label(line, IN).is_some() {
            in_out = false;
        }
        if let Some(v) = label(line, IN) {
            if input.is_empty() {
                input = v.to_string();
            }
            continue;
        }
        if let Some(v) = label(line, OUT) {
            output = v.to_string();
            in_out = true;
            continue;
        }
        if in_out && !line.trim().is_empty() {
            output.push('\n');
            output.push_str(line.trim_end());
        }
    }
    let output = output.trim().to_string();
    if output.is_empty() { None } else { Some((input, output)) }
}

fn difficulty_str(d: &str) -> String {
    match d {
        "Easy" => "简单",
        "Medium" => "中等",
        "Hard" => "困难",
        other => other,
    }
    .to_string()
}

/// GraphQL 响应 → FetchedProblem（纯函数，可单测）
fn parse_question(q: &serde_json::Value, slug: &str, cn: bool, url: &str) -> Result<FetchedProblem, FetchError> {
    let get = |k: &str| q.get(k).and_then(|v| v.as_str());

    let fid = get("questionFrontendId").unwrap_or("");
    let t = if cn {
        get("translatedTitle").or_else(|| get("title"))
    } else {
        get("title").or_else(|| get("translatedTitle"))
    }
    .unwrap_or(slug);
    let title = if fid.is_empty() { t.to_string() } else { format!("{fid}. {t}") };

    let content_html = if cn {
        get("translatedContent").filter(|c| !c.is_empty()).or_else(|| get("content")).unwrap_or("")
    } else {
        get("content").filter(|c| !c.is_empty()).or_else(|| get("translatedContent")).unwrap_or("")
    };

    let tags: Vec<String> = q
        .get("topicTags")
        .and_then(|t| t.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|x| {
                    let pick = if cn { "translatedName" } else { "name" };
                    x.get(pick)
                        .and_then(|v| v.as_str())
                        .or_else(|| x.get("name").and_then(|v| v.as_str()))
                        .map(String::from)
                })
                .collect()
        })
        .unwrap_or_default();

    // exampleTestcaseList = 判题参数格式输入；输出从题面 pre 块解析，按序对齐
    let inputs: Vec<String> = q
        .get("exampleTestcaseList")
        .and_then(|l| l.as_array())
        .map(|arr| {
            arr.iter().filter_map(|v| v.as_str().map(|s| s.trim().to_string())).filter(|s| !s.is_empty()).collect()
        })
        .unwrap_or_default();
    let blocks = sample_blocks(content_html);
    let samples: Vec<(String, String)> = if !inputs.is_empty() {
        inputs
            .iter()
            .enumerate()
            .map(|(i, inp)| (inp.clone(), blocks.get(i).map(|b| b.1.clone()).unwrap_or_default()))
            .collect()
    } else {
        blocks
    };

    let mut stmt = format!(
        "# {title}\n\n> {url}\n\n> LeetCode 为函数式题目：样例输入是判题参数格式（每行一个参数），程序需自行从 stdin 解析。\n\n"
    );
    stmt.push_str(&util::html_to_text(content_html));
    stmt.push('\n');

    Ok(FetchedProblem {
        id: ProblemId::new(Source::LeetCode, slug.to_string()),
        title,
        url: url.to_string(),
        statement: stmt,
        samples,
        time_limit_ms: None, // LeetCode 不公布单题时限，用默认
        memory_limit_mb: None,
        tags,
        difficulty: get("difficulty").map(difficulty_str),
    })
}

#[async_trait::async_trait]
impl Provider for LeetCodeProvider {
    fn name(&self) -> &'static str {
        "leetcode"
    }

    fn matches(&self, url: &str) -> bool {
        url.contains("leetcode.cn") || url.contains("leetcode.com") || url.contains("leetcode-cn.com")
    }

    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError> {
        let caps = SLUG_RE.captures(url).ok_or_else(|| FetchError::UnsupportedUrl(url.to_string()))?;
        let slug = caps[1].to_string();
        let cn = is_cn(url);
        let api = if cn { "https://leetcode.cn/graphql/" } else { "https://leetcode.com/graphql/" };

        let body = serde_json::json!({ "query": QUERY, "variables": { "titleSlug": slug } });
        let text = http::post_json(api, &body, &[("Referer", url)]).await?;
        let v: serde_json::Value =
            serde_json::from_str(&text).map_err(|e| FetchError::Parse(format!("graphql json: {e}")))?;
        let q = v.get("data").and_then(|d| d.get("question")).filter(|q| !q.is_null()).ok_or(FetchError::NotFound)?;
        parse_question(q, &slug, cn, url)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CN_CONTENT: &str = r#"
<p>给定一个整数数组 <code>nums</code>&nbsp;和一个整数目标值 <code>target</code>。</p>
<p><strong class="example">示例 1：</strong></p>
<pre>
<strong>输入：</strong>nums = [2,7,11,15], target = 9
<strong>输出：</strong>[0,1]
<strong>解释：</strong>因为 nums[0] + nums[1] == 9 ，返回 [0, 1] 。
</pre>
<p><strong class="example">示例 2：</strong></p>
<pre>
<strong>输入：</strong>nums = [3,3], target = 6
<strong>输出：</strong>[0,1]
</pre>
"#;

    #[test]
    fn pre_blocks_extract_outputs() {
        let b = sample_blocks(CN_CONTENT);
        assert_eq!(b.len(), 2);
        assert_eq!(b[0].0, "nums = [2,7,11,15], target = 9");
        assert_eq!(b[0].1, "[0,1]");
        assert_eq!(b[1].1, "[0,1]");
    }

    #[test]
    fn en_labels_work() {
        let html = "<pre>\n<strong>Input:</strong> s = \"abc\"\n<strong>Output:</strong> 3\n<strong>Explanation:</strong> x\n</pre>";
        let b = sample_blocks(html);
        assert_eq!(b, vec![("s = \"abc\"".to_string(), "3".to_string())]);
    }

    #[test]
    fn graphql_json_parses() {
        let q = serde_json::json!({
            "questionFrontendId": "1",
            "title": "Two Sum",
            "translatedTitle": "两数之和",
            "difficulty": "Easy",
            "translatedContent": CN_CONTENT,
            "content": "<p>en</p>",
            "topicTags": [{"name":"Array","translatedName":"数组"},{"name":"Hash Table","translatedName":"哈希表"}],
            "exampleTestcaseList": ["[2,7,11,15]\n9", "[3,3]\n6"]
        });
        let fp = parse_question(&q, "two-sum", true, "https://leetcode.cn/problems/two-sum/").unwrap();
        assert_eq!(fp.id.to_string(), "leetcode:two-sum");
        assert_eq!(fp.title, "1. 两数之和");
        assert_eq!(fp.difficulty.as_deref(), Some("简单"));
        assert_eq!(fp.tags, vec!["数组", "哈希表"]);
        assert_eq!(fp.samples[0], ("[2,7,11,15]\n9".to_string(), "[0,1]".to_string()));
        assert_eq!(fp.samples[1], ("[3,3]\n6".to_string(), "[0,1]".to_string()));
        assert!(fp.statement.contains("函数式题目"));
    }

    #[test]
    fn en_url_uses_english_fields() {
        let q = serde_json::json!({
            "questionFrontendId": "1", "title": "Two Sum", "translatedTitle": "两数之和",
            "difficulty": "Easy", "content": "<p>en content</p>", "translatedContent": "<p>中文</p>",
            "topicTags": [{"name":"Array","translatedName":"数组"}], "exampleTestcaseList": []
        });
        let fp = parse_question(&q, "two-sum", false, "https://leetcode.com/problems/two-sum/").unwrap();
        assert_eq!(fp.title, "1. Two Sum");
        assert_eq!(fp.tags, vec!["Array"]);
        assert!(fp.statement.contains("en content"));
    }
}
