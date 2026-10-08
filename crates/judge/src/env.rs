//! 编译器/解释器自动探测。
//! 探测顺序：显式配置 → PATH → Dev-C++ 捆绑 MinGW → 平台常见路径。

use shuati_core::Lang;
use std::path::PathBuf;

/// 探测指定语言的工具链，返回可执行文件路径
pub fn detect_toolchain(lang: Lang, configured: &str) -> Option<PathBuf> {
    if !configured.is_empty() {
        let p = PathBuf::from(configured);
        if p.exists() {
            return Some(p);
        }
    }
    match lang {
        Lang::Cpp => detect_cpp(),
        Lang::Python => detect_python(),
    }
}

fn in_path(names: &[&str]) -> Option<PathBuf> {
    names.iter().find_map(|n| which::which(n).ok())
}

fn detect_cpp() -> Option<PathBuf> {
    // 1. PATH
    if let Some(p) = in_path(&["g++", "clang++"]) {
        return Some(p);
    }
    // 2. Windows: Dev-C++ 捆绑的 MinGW/TDM-GCC
    #[cfg(target_os = "windows")]
    {
        let candidates = [
            r"C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\g++.exe",
            r"C:\Program Files\Dev-Cpp\MinGW64\bin\g++.exe",
            r"C:\Dev-Cpp\MinGW64\bin\g++.exe",
            r"C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe",
            r"C:\Program Files\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe",
            r"C:\TDM-GCC-64\bin\g++.exe",
            r"C:\msys64\mingw64\bin\g++.exe",
            r"C:\msys64\ucrt64\bin\g++.exe",
        ];
        for c in candidates {
            let p = PathBuf::from(c);
            if p.exists() {
                return Some(p);
            }
        }
    }
    // 3. macOS: xcode command line tools
    #[cfg(target_os = "macos")]
    {
        let candidates = ["/usr/bin/clang++", "/opt/homebrew/bin/g++", "/usr/local/bin/g++"];
        for c in candidates {
            let p = PathBuf::from(c);
            if p.exists() {
                return Some(p);
            }
        }
    }
    None
}

fn detect_python() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    let names = ["python", "python3", "py"];
    #[cfg(not(target_os = "windows"))]
    let names = ["python3", "python"];
    in_path(&names)
}

/// 环境体检报告（doctor 命令/设置屏用）
#[derive(Debug)]
pub struct EnvReport {
    pub cpp: Option<PathBuf>,
    pub python: Option<PathBuf>,
}

pub fn check_env(cfg_cpp: &str, cfg_py: &str) -> EnvReport {
    EnvReport { cpp: detect_toolchain(Lang::Cpp, cfg_cpp), python: detect_toolchain(Lang::Python, cfg_py) }
}
