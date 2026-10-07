use serde::{Deserialize, Serialize};
use std::fmt;
use std::path::PathBuf;

/// OJ 来源平台
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Source {
    Luogu,
    LeetCode,
    Codeforces,
    Lanqiao,
    Local,
}

impl fmt::Display for Source {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            Source::Luogu => "luogu",
            Source::LeetCode => "leetcode",
            Source::Codeforces => "codeforces",
            Source::Lanqiao => "lanqiao",
            Source::Local => "local",
        };
        f.write_str(s)
    }
}

/// 题目唯一标识。规范形式 "luogu:P1001" / "local:xxx"。
/// 目录名使用 `luogu__P1001`（`:` 在 Windows 文件名非法）。
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ProblemId {
    pub source: Source,
    pub slug: String,
}

impl ProblemId {
    pub fn new(source: Source, slug: impl Into<String>) -> Self {
        Self { source, slug: slug.into() }
    }

    /// 目录安全形式：`luogu__P1001`
    pub fn dir_name(&self) -> String {
        format!("{}__{}", self.source, self.slug)
    }

    pub fn parse(s: &str) -> Option<Self> {
        let (src, slug) = s.split_once(':')?;
        let source = match src.to_ascii_lowercase().as_str() {
            "luogu" => Source::Luogu,
            "leetcode" => Source::LeetCode,
            "codeforces" | "cf" => Source::Codeforces,
            "lanqiao" => Source::Lanqiao,
            "local" => Source::Local,
            _ => return None,
        };
        Some(Self::new(source, slug.to_string()))
    }
}

impl fmt::Display for ProblemId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.source, self.slug)
    }
}

/// 资源限制（每题可配）
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct Limits {
    /// CPU 时间（毫秒）
    pub cpu_ms: u64,
    /// 墙钟时间（毫秒），防 I/O 死等
    pub wall_ms: u64,
    /// 内存（MB）
    pub memory_mb: u64,
    /// 输出文件大小上限（MB），防写爆磁盘
    pub output_mb: u64,
}

impl Default for Limits {
    fn default() -> Self {
        Self { cpu_ms: 2000, wall_ms: 5000, memory_mb: 256, output_mb: 16 }
    }
}

/// 题目元数据（problem.toml 的内存形态）
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Problem {
    pub id: ProblemId,
    pub title: String,
    #[serde(default)]
    pub url: String,
    #[serde(default)]
    pub difficulty: Option<String>,
    #[serde(default)]
    pub tags: Vec<String>,
    #[serde(default)]
    pub limits: Limits,
    /// 题面文件相对路径（相对题目目录）
    #[serde(default = "default_statement")]
    pub statement: PathBuf,
    /// 比较器配置
    #[serde(default)]
    pub checker: crate::CheckerSpec,
    #[serde(default)]
    pub created_at: i64,
}

fn default_statement() -> PathBuf {
    PathBuf::from("statement.md")
}

/// 一个测试用例
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TestCase {
    pub name: String,
    pub input: String,
    /// None = 对拍场景下由 oracle 提供
    pub expected: Option<String>,
    pub origin: CaseOrigin,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum CaseOrigin {
    Sample,
    Generated,
    Stress,
}

/// 待编译的源文件描述
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceFile {
    pub path: PathBuf,
    pub lang: Lang,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Lang {
    Cpp,
    Python,
}

impl Lang {
    pub fn from_path(path: &std::path::Path) -> Option<Self> {
        match path.extension()?.to_str()? {
            "cpp" | "cc" | "cxx" | "c++" => Some(Lang::Cpp),
            "py" | "pyw" => Some(Lang::Python),
            _ => None,
        }
    }
}
