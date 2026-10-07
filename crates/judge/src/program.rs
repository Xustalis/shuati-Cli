use shuati_core::Lang;
use std::path::PathBuf;

/// 一次编译的产物：可直接执行的 argv 模板。
/// cpp → argv = ["./main_exe"]；python → argv = ["python3", "main.py"]
#[derive(Debug, Clone)]
pub struct Program {
    pub argv: Vec<String>,
    pub lang: Lang,
    /// 编译产物路径（用于清理），python 为源文件本身
    pub artifact: PathBuf,
    /// 运行工作目录
    pub workdir: PathBuf,
    /// 是否来自编译缓存（true 时 cleanup 不删除）
    pub cached: bool,
}

/// IO 重定向描述
#[derive(Debug, Clone)]
pub struct Io {
    pub stdin: Option<PathBuf>,
    pub stdout: PathBuf,
    pub stderr: PathBuf,
}
