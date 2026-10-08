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

/// 洛谷 tag id → 中文名。覆盖官方题库筛选项里的常见算法/来源标签；
/// 未收录的 id（年份/省份/小众项）原样返回数字串，不影响使用。
fn tag_name(id: i64) -> String {
    let name = match id {
        // 基础算法
        1 => "模拟",
        2 => "字符串",
        3 => "动态规划 DP",
        4 => "搜索",
        5 => "数学",
        6 => "图论",
        7 => "贪心",
        8 => "计算几何",
        11 => "树形数据结构",
        12 => "递推",
        13 => "博弈论",
        44 => "线性数据结构",
        45 => "二分",
        54 => "递归",
        111 => "枚举",
        112 => "分治",
        113 => "排序",
        254 => "前缀和",
        314 => "位运算",
        318 => "构造",
        330 => "差分",
        504 => "STL",
        // 搜索进阶
        126 => "广度优先搜索 BFS",
        127 => "深度优先搜索 DFS",
        128 => "剪枝",
        129 => "记忆化搜索",
        130 => "启发式搜索",
        131 => "迭代加深搜索",
        132 => "启发式迭代加深搜索 IDA*",
        133 => "Dancing Links",
        134 => "爬山算法",
        135 => "模拟退火",
        136 => "随机调整",
        137 => "遗传算法",
        263 => "A* 算法",
        380 => "折半搜索",
        // DP 细分
        139 => "背包 DP",
        141 => "数位 DP",
        144 => "区间 DP",
        152 => "树形 DP",
        323 => "轮廓线 DP",
        444 => "线性 DP",
        464 => "状压 DP",
        // 数据结构
        42 => "线段树",
        47 => "并查集",
        50 => "平衡树",
        51 => "堆",
        53 => "树状数组",
        56 => "单调队列",
        65 => "颜色段均摊（珂朵莉树 ODT）",
        215 => "可并堆",
        229 => "动态树 LCT",
        232 => "树套树",
        233 => "可持久化线段树",
        234 => "可持久化",
        287 => "栈",
        288 => "队列",
        321 => "K-D Tree",
        377 => "李超线段树",
        378 => "吉司机线段树",
        379 => "线段树合并",
        // 图论
        68 => "Kruskal 重构树",
        79 => "网络流",
        155 => "图论建模",
        158 => "图遍历",
        159 => "拓扑排序",
        160 => "最短路",
        166 => "生成树",
        172 => "平面图",
        173 => "最小环",
        174 => "负权环",
        175 => "连通块",
        176 => "2-SAT",
        177 => "平面图欧拉公式",
        179 => "强连通分量",
        180 => "Tarjan",
        181 => "双连通分量",
        182 => "欧拉回路",
        185 => "差分约束",
        186 => "仙人掌",
        187 => "二分图",
        189 => "一般图的最大匹配",
        197 => "上下界网络流",
        198 => "最小割",
        204 => "费用流",
        350 => "圆方树",
        450 => "弦图",
        476 => "Floyd 算法",
        530 => "广义串并联图",
        541 => "Hall 定理",
        // 计算几何
        283 => "三维计算几何",
        286 => "向量",
        291 => "凸包",
        292 => "叉积",
        293 => "线段相交",
        295 => "半平面交",
        299 => "旋转卡壳",
        449 => "极角排序",
        482 => "平面几何",
        525 => "闵可夫斯基和",
        // 博弈论
        311 => "博弈树",
        370 => "Nim 积",
        445 => "SG 函数",
        // 数学/杂项
        100 => "cdq 分治",
        122 => "信息论",
        235 => "哈希 hashing",
        300 => "字典树 Trie",
        316 => "整体二分",
        364 => "Dilworth 定理",
        376 => "分类讨论",
        396 => "梯度下降法",
        412 => "拉格朗日乘数法",
        417 => "拉格朗日插值法",
        480 => "单位根反演",
        496 => "后缀平衡树",
        101 => "后缀自动机 SAM",
        // 杂项/来源
        107 => "Special Judge",
        108 => "O2优化",
        337 => "洛谷月赛",
        342 => "CSP-S 提高级",
        502 => "CERC",
        335 => "ICPC",
        _ => return id.to_string(),
    };
    name.to_string()
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
                arr.iter()
                    .filter_map(|v| v.as_i64().map(tag_name))
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

#[cfg(test)]
mod tests {
    use super::tag_name;

    #[test]
    fn tag_names_common() {
        assert_eq!(tag_name(1), "模拟");
        assert_eq!(tag_name(7), "贪心");
        assert_eq!(tag_name(127), "深度优先搜索 DFS");
        assert_eq!(tag_name(42), "线段树");
        assert_eq!(tag_name(254), "前缀和");
    }

    #[test]
    fn tag_unknown_falls_back_to_id() {
        assert_eq!(tag_name(9999), "9999");
    }
}
