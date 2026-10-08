use crate::{Result, atomic_write};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// 应用级易失状态（删了只丢界面记忆）
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct AppState {
    /// 当前选中题目 "luogu:P1001"
    #[serde(default)]
    pub current_problem: Option<String>,
    /// 当前被评测的源文件（GUI 拖入的）
    #[serde(default)]
    pub watch_file: Option<PathBuf>,
    /// 保存即评测开关
    #[serde(default)]
    pub watch_enabled: bool,
    /// 最近一次的总结（问题 id -> verdict 字符串）
    #[serde(default)]
    pub last_verdicts: std::collections::BTreeMap<String, String>,
}

impl AppState {
    pub fn load(path: &std::path::Path) -> Self {
        std::fs::read_to_string(path).ok().and_then(|t| serde_json::from_str(&t).ok()).unwrap_or_default()
    }

    pub fn save(&self, path: &std::path::Path) -> Result<()> {
        Ok(atomic_write(path, serde_json::to_string_pretty(self)?.as_bytes())?)
    }
}
