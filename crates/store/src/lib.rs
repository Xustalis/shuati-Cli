//! shuati-store: 工作区文件布局 + 原子读写。无数据库，文件即真相。

mod workspace;
mod problem_dir;
mod state;
mod config;

pub use workspace::*;
pub use problem_dir::*;
pub use state::*;
pub use config::*;

use std::io;
use std::path::PathBuf;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum StoreError {
    #[error("io error: {0}")]
    Io(#[from] io::Error),
    #[error("toml parse: {0}")]
    Toml(#[from] toml::de::Error),
    #[error("toml serialize: {0}")]
    TomlSer(#[from] toml::ser::Error),
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    #[error("problem not found: {0}")]
    NotFound(String),
    #[error("problem already exists at {0}")]
    Exists(PathBuf),
    #[error("invalid manifest at {0}: {1}")]
    Invalid(PathBuf, String),
}

pub type Result<T> = std::result::Result<T, StoreError>;

pub mod secrets;

/// 原子写：写临时文件再 rename，崩溃不留半截文件。
pub fn atomic_write(path: &std::path::Path, data: &[u8]) -> io::Result<()> {
    use std::io::Write;
    let dir = path.parent().ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "no parent"))?;
    std::fs::create_dir_all(dir)?;
    let mut tmp = tempfile::NamedTempFile::new_in(dir)?;
    tmp.write_all(data)?;
    tmp.flush()?;
    tmp.as_file().sync_all()?;
    tmp.persist(path).map_err(|e| e.error)?;
    Ok(())
}
