//! shuati-core: 纯领域模型，零 I/O 依赖。
//! 所有上层 crate（judge/store/fetch/ai/gui/cli）共享的词汇表。

mod checker;
mod problem;
mod report;
mod verdict;

pub use checker::*;
pub use problem::*;
pub use report::*;
pub use verdict::*;
