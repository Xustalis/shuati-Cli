//! shuati-judge: 判题引擎。
//! Compiler → Program / Runner → RunOutcome / Checker → Verdict 三件套正交拆分，
//! TestEngine/StressEngine 做编排。所有进程调用走 argv 数组，绝不拼 shell。

mod program;
mod compiler;
mod runner;
mod checker;
mod engine;
mod watch;
mod env;

pub use program::*;
pub use compiler::*;
pub use runner::*;
pub use checker::*;
pub use engine::*;
pub use watch::*;
pub use env::*;

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
    #[error("no test cases")]
    NoCases,
    #[error("{0}")]
    Other(String),
}

pub type Result<T> = std::result::Result<T, JudgeError>;
