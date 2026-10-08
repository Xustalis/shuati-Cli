//! shuati-judge: 判题引擎。
//! Compiler → Program / Runner → RunOutcome / Checker → Verdict 三件套正交拆分，
//! TestEngine/StressEngine 做编排。所有进程调用走 argv 数组，绝不拼 shell。

mod binding;
mod checker;
mod compiler;
mod engine;
mod env;
mod program;
mod runner;
mod stress;
mod watch;

pub use binding::*;
pub use checker::*;
pub use compiler::*;
pub use engine::*;
pub use env::*;
pub use program::*;
pub use runner::*;
pub use stress::*;
pub use watch::*;

use thiserror::Error;

#[derive(Debug, Error)]
pub enum JudgeError {
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("compile failed: {0}")]
    Compile(String),
    #[error("compiler not found for {0}")]
    CompilerNotFound(String),
    #[error("sandbox internal: {0}")]
    Sandbox(String),
    #[error("store: {0}")]
    Store(#[from] shuati_store::StoreError),
    #[error("没有测试用例：tests/ 目录为空，先在题库页/CLI 添加用例（.in + .out）")]
    NoCases,
    #[error("{0}")]
    Other(String),
}

pub type Result<T> = std::result::Result<T, JudgeError>;
