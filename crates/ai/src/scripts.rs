//! <gen>/<oracle> 结构化输出解析。

/// AI 生成的对拍脚本对
#[derive(Debug, Clone)]
pub struct StressScripts {
    /// 生成器源码（argv[1]=seed）
    pub generator: String,
    /// 标程源码（读输入出答案）
    pub oracle: String,
}

/// 从 AI 响应中提取 <gen>...</gen> 和 <oracle>...</oracle>。
/// 容错：也接受 markdown ``` 代码块包裹的形式。
pub fn parse(text: &str) -> Option<StressScripts> {
    let g = extract(text, "gen").or_else(|| extract_block(text, "```", 0))?;
    let o = extract(text, "oracle").or_else(|| extract_block(text, "```", 1))?;
    Some(StressScripts { generator: g, oracle: o })
}

fn extract(text: &str, tag: &str) -> Option<String> {
    let open = format!("<{tag}>");
    let close = format!("</{tag}>");
    let start = text.find(&open)? + open.len();
    let end = text[start..].find(&close)? + start;
    let body = text[start..end].trim();
    // 剥掉模型习惯加的 ```lang 围栏
    Some(strip_fences(body))
}

/// 第 n 个 ``` 代码块内容
fn extract_block(text: &str, fence: &str, n: usize) -> Option<String> {
    let mut rest = text;
    for i in 0..=n {
        let start = rest.find(fence)?;
        rest = &rest[start + fence.len()..];
        // 跳过语言标注行
        if let Some(nl) = rest.find('\n') {
            rest = &rest[nl + 1..];
        }
        let end = rest.find(fence)?;
        if i == n {
            return Some(strip_fences(rest[..end].trim()));
        }
        rest = &rest[end + fence.len()..];
    }
    None
}

fn strip_fences(s: &str) -> String {
    let t = s.trim();
    let t = t.strip_prefix("```").map(|x| x.trim_start_matches(|c: char| c.is_alphanumeric()).trim_start()).unwrap_or(t);
    t.strip_suffix("```").unwrap_or(t).trim().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_xml_tags() {
        let r = parse("前言\n<gen>\nimport sys\n</gen>\n中间\n<oracle>\nprint(42)\n</oracle>\n尾").unwrap();
        assert!(r.generator.contains("import sys"));
        assert!(r.oracle.contains("print(42)"));
    }

    #[test]
    fn parses_fenced_in_tags() {
        let r = parse("<gen>\n```python\nx=1\n```\n</gen>\n<oracle>\n```\ny=2\n```\n</oracle>").unwrap();
        assert_eq!(r.generator, "x=1");
        assert_eq!(r.oracle, "y=2");
    }
}
