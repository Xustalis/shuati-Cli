//! shuati-fetch: OJ 题目拉取。
//! Provider trait + URL 路由 + 各平台实现。

pub mod codeforces;
mod http;
pub mod lanqiao;
pub mod leetcode;
pub mod luogu;
mod util;

use shuati_core::ProblemId;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum FetchError {
    #[error("unsupported url: {0}")]
    UnsupportedUrl(String),
    #[error("network: {0}")]
    Network(String),
    #[error("parse: {0}")]
    Parse(String),
    #[error("problem not found")]
    NotFound,
}

/// 拉到的原始题目内容（store 层负责落盘）
#[derive(Debug)]
pub struct FetchedProblem {
    pub id: ProblemId,
    pub title: String,
    pub url: String,
    /// markdown 题面
    pub statement: String,
    /// (input, output) 样例对
    pub samples: Vec<(String, String)>,
    pub time_limit_ms: Option<u64>,
    pub memory_limit_mb: Option<u64>,
    pub tags: Vec<String>,
    pub difficulty: Option<String>,
}

/// 平台 Provider
#[async_trait::async_trait]
pub trait Provider: Send + Sync {
    fn name(&self) -> &'static str;
    /// 该 URL 是否归本平台管
    fn matches(&self, url: &str) -> bool;
    /// 拉取前注入工作区配置（凭据等）。默认无操作。
    fn configure(&mut self, _config: &shuati_store::Config) {}
    /// 拉取题目
    async fn fetch(&self, url: &str) -> Result<FetchedProblem, FetchError>;
}

/// 全部 Provider 注册表
pub fn providers() -> Vec<Box<dyn Provider>> {
    vec![
        Box::new(luogu::LuoguProvider::new()),
        Box::new(codeforces::CodeforcesProvider::new()),
        Box::new(leetcode::LeetCodeProvider::new()),
        Box::new(lanqiao::LanqiaoProvider::new()),
    ]
}

/// URL → Provider 路由
pub fn route(url: &str) -> Option<Box<dyn Provider>> {
    providers().into_iter().find(|p| p.matches(url))
}

/// 一站式服务：URL → 落盘 ProblemDir
pub async fn fetch_to_dir(url: &str, ws: &shuati_store::Workspace) -> Result<shuati_store::ProblemDir, FetchError> {
    let mut provider = route(url).ok_or_else(|| FetchError::UnsupportedUrl(url.to_string()))?;
    provider.configure(&shuati_store::Config::load(&ws.config_path()));
    let fp = provider.fetch(url).await?;
    materialize(fp, ws).map_err(|e| FetchError::Parse(e.to_string()))
}

/// FetchedProblem → 磁盘。
/// 题目已存在（重复拉取）时：更新标题/难度/标签，保留用户自定义的
/// limits/checker/题面；样例只补缺失文件，不覆盖用户改过的用例。
pub fn materialize(fp: FetchedProblem, ws: &shuati_store::Workspace) -> shuati_store::Result<shuati_store::ProblemDir> {
    let dir = ws.problem_dir(&fp.id);
    let pd = match shuati_store::ProblemDir::open(&dir) {
        Ok(existing) => {
            let mut pd = existing;
            pd.meta.title = fp.title;
            pd.meta.url = fp.url;
            pd.meta.difficulty = fp.difficulty;
            pd.meta.tags = fp.tags;
            pd.save_meta()?;
            if pd.read_statement().unwrap_or_default().trim().is_empty() {
                pd.write_statement(&fp.statement)?;
            }
            pd
        }
        Err(_) => {
            let mut meta = shuati_core::Problem {
                id: fp.id.clone(),
                title: fp.title,
                url: fp.url,
                difficulty: fp.difficulty,
                tags: fp.tags,
                limits: shuati_core::Limits::default(),
                statement: "statement.md".into(),
                checker: Default::default(),
                created_at: shuati_store::now_ts(),
            };
            if let Some(ms) = fp.time_limit_ms {
                meta.limits.cpu_ms = ms;
                meta.limits.wall_ms = (ms * 3).max(2000);
            }
            if let Some(mb) = fp.memory_limit_mb {
                meta.limits.memory_mb = mb;
            }
            let pd = shuati_store::ProblemDir::create(&dir, &meta)?;
            pd.write_statement(&fp.statement)?;
            pd
        }
    };
    for (i, (input, output)) in fp.samples.iter().enumerate() {
        let name = format!("{:04}", i + 1);
        // 已存在的用例文件不覆盖（用户可能改过数据）
        if pd.tests_dir().join(format!("{name}.in")).exists() {
            continue;
        }
        pd.write_case(&name, input, Some(output), shuati_core::CaseOrigin::Sample)?;
    }
    Ok(pd)
}
