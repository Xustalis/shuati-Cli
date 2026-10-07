use crate::{Result, StoreError};
use std::path::PathBuf;

/// 题库根目录（一个 workspace）。
/// 布局见设计草案 §4：state.json / config.toml / problems/<src>__<slug>/
#[derive(Debug, Clone)]
pub struct Workspace {
    pub root: PathBuf,
}

impl Workspace {
    /// 打开已有工作区；不存在则创建
    pub fn open(root: impl Into<PathBuf>) -> Result<Self> {
        let root = root.into();
        std::fs::create_dir_all(root.join("problems"))?;
        Ok(Self { root })
    }

    /// 默认工作区：可执行文件同级 `shuati-data/`；目录不可写则回退系统数据目录
    pub fn default_dir() -> PathBuf {
        if let Ok(exe) = std::env::current_exe() {
            if let Some(dir) = exe.parent() {
                let portable = dir.join("shuati-data");
                // 已存在就用；否则探测是否可创建（不可写→回退）
                if portable.exists() || std::fs::create_dir(&portable).is_ok() {
                    return portable;
                }
            }
        }
        dirs_fallback()
    }

    pub fn config_path(&self) -> PathBuf {
        self.root.join("config.toml")
    }

    pub fn state_path(&self) -> PathBuf {
        self.root.join("state.json")
    }

    pub fn problems_dir(&self) -> PathBuf {
        self.root.join("problems")
    }

    /// 题目目录路径
    pub fn problem_dir(&self, id: &shuati_core::ProblemId) -> PathBuf {
        self.problems_dir().join(id.dir_name())
    }

    /// 列出全部题目目录名
    pub fn list_problem_dirs(&self) -> Result<Vec<PathBuf>> {
        let mut out = Vec::new();
        let dir = self.problems_dir();
        if !dir.exists() {
            return Ok(out);
        }
        for entry in std::fs::read_dir(dir)? {
            let entry = entry?;
            if entry.file_type()?.is_dir() {
                out.push(entry.path());
            }
        }
        out.sort();
        Ok(out)
    }
}

fn dirs_fallback() -> PathBuf {
    // 不引入 dirs crate 的轻量回退
    #[cfg(target_os = "windows")]
    {
        if let Ok(appdata) = std::env::var("APPDATA") {
            return PathBuf::from(appdata).join("shuati");
        }
    }
    #[cfg(not(target_os = "windows"))]
    {
        if let Ok(home) = std::env::var("HOME") {
            return PathBuf::from(home).join(".local").join("share").join("shuati");
        }
    }
    PathBuf::from("shuati-data")
}

impl From<StoreError> for String {
    fn from(e: StoreError) -> String {
        e.to_string()
    }
}
