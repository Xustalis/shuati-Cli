//! 平台题面共用的 HTML→文本工具（LeetCode/蓝桥的题面字段都是 HTML）。

/// 粗略 HTML → 纯文本：保留换行/段落/列表结构，剥掉标签，反转义实体。
/// `<sup>` 转 `^`、`<sub>` 转 `_`，让 `10<sup>4</sup>` 读起来是 `10^4`。
pub fn html_to_text(html: &str) -> String {
    let mut s = html.to_string();
    for (tag, rep) in [
        ("<sup>", "^"),
        ("</sup>", ""),
        ("<sub>", "_"),
        ("</sub>", ""),
        ("<br>", "\n"),
        ("<br/>", "\n"),
        ("<br />", "\n"),
        ("</p>", "\n\n"),
        ("</li>", "\n"),
        ("<li>", "- "),
        ("</pre>", "\n"),
        ("&nbsp;", " "),
    ] {
        s = s.replace(tag, rep);
    }
    // 剥掉剩余标签
    let mut out = String::with_capacity(s.len());
    let mut in_tag = false;
    for c in s.chars() {
        match c {
            '<' => in_tag = true,
            '>' if in_tag => in_tag = false,
            _ if !in_tag => out.push(c),
            _ => {}
        }
    }
    let out = unescape(&out);
    // 折叠 3+ 连续空行
    let mut lines = Vec::new();
    let mut blank = 0;
    for line in out.lines() {
        let t = line.trim_end();
        if t.trim().is_empty() {
            blank += 1;
            if blank <= 1 {
                lines.push(String::new());
            }
        } else {
            blank = 0;
            lines.push(t.to_string());
        }
    }
    lines.join("\n").trim().to_string()
}

fn unescape(s: &str) -> String {
    s.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"").replace("&#39;", "'").replace("&amp;", "&")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strips_tags_and_keeps_structure() {
        let html = "<p>第一行</p><p>第二&nbsp;行<br>换行</p><ul><li>项1</li><li>项2</li></ul>";
        let t = html_to_text(html);
        assert!(t.contains("第一行"));
        assert!(t.contains("第二 行\n换行"));
        assert!(t.contains("- 项1\n- 项2"));
    }

    #[test]
    fn sup_becomes_caret() {
        assert_eq!(html_to_text("<code>10<sup>4</sup></code>"), "10^4");
    }

    #[test]
    fn unescapes_entities() {
        assert_eq!(html_to_text("a &lt; b &amp;&amp; c &gt; d"), "a < b && c > d");
    }

    #[test]
    fn collapses_blank_runs() {
        assert_eq!(html_to_text("a</p></p></p><p>b"), "a\n\nb");
    }
}
