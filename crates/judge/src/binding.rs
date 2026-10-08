//! 源文件头部 `shuati:` 标记 → 显式题目绑定。
//! 支持 `// shuati: luogu P1001`、`// shuati: luogu:P1001`、`# shuati: local:xxx`。

use shuati_core::ProblemId;
use std::path::Path;

/// 读源文件前 16 行找 `shuati:` 标记，返回解析到的题目 id。
pub fn binding_comment(src: &Path) -> Option<ProblemId> {
    let text = std::fs::read_to_string(src).ok()?;
    for line in text.lines().take(16) {
        let Some(pos) = line.find("shuati:") else { continue };
        let rest = line[pos + "shuati:".len()..].trim();
        if rest.is_empty() {
            continue;
        }
        let toks: Vec<&str> = rest.split_whitespace().collect();
        // 候选："luogu:P1001" 整词 或 "luogu P1001" 两词拼接
        let cands = [
            toks[0].to_string(),
            format!("{}:{}", toks[0], toks.get(1).copied().unwrap_or_default()),
        ];
        for cand in cands {
            if let Some(id) = ProblemId::parse(&cand) {
                return Some(id);
            }
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use shuati_core::Source;
    use std::path::PathBuf;

    fn tmp_src(content: &str) -> (tempfile::TempDir, PathBuf) {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("main.cpp");
        std::fs::write(&p, content).unwrap();
        (d, p)
    }

    #[test]
    fn parses_colon_form() {
        let (_d, p) = tmp_src("// shuati: luogu:P1001\n#include <cstdio>\n");
        let id = binding_comment(&p).unwrap();
        assert_eq!(id.source, Source::Luogu);
        assert_eq!(id.slug, "P1001");
    }

    #[test]
    fn parses_space_form() {
        let (_d, p) = tmp_src("// shuati: luogu P1001\n");
        assert_eq!(binding_comment(&p).unwrap().slug, "P1001");
    }

    #[test]
    fn python_hash_comment() {
        let (_d, p) = tmp_src("# shuati: local:test\nprint(1)\n");
        assert_eq!(binding_comment(&p).unwrap().slug, "test");
    }

    #[test]
    fn no_marker() {
        let (_d, p) = tmp_src("#include <bits/stdc++.h>\nint main(){}\n");
        assert!(binding_comment(&p).is_none());
    }
}
