use crate::{Result, atomic_write};
use serde::{Deserialize, Serialize};

/// config.toml
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    /// 显式指定的编译器路径（空 = 自动探测）
    #[serde(default)]
    pub compiler_cpp: String,
    #[serde(default)]
    pub compiler_python: String,
    #[serde(default = "default_lang")]
    pub default_lang: String,
    /// AI 配置
    #[serde(default)]
    pub ai: AiConfig,
    /// 编辑器命令（可选，用于"打开题面"）
    #[serde(default)]
    pub editor: String,
    /// 蓝桥云课登录 Cookie（拉取完整题面需要；也可用 SHUATI_LANQIAO_COOKIE 环境变量）
    #[serde(default)]
    pub lanqiao_cookie: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AiConfig {
    #[serde(default)]
    pub api_key: String,
    #[serde(default = "default_api_base")]
    pub api_base: String,
    #[serde(default = "default_model")]
    pub model: String,
}

impl Default for AiConfig {
    fn default() -> Self {
        Self { api_key: String::new(), api_base: default_api_base(), model: default_model() }
    }
}

fn default_lang() -> String {
    "cpp".to_string()
}
fn default_api_base() -> String {
    "https://api.deepseek.com/v1".to_string()
}
fn default_model() -> String {
    "deepseek-chat".to_string()
}

impl Default for Config {
    fn default() -> Self {
        Self {
            compiler_cpp: String::new(),
            compiler_python: String::new(),
            default_lang: default_lang(),
            ai: AiConfig::default(),
            editor: String::new(),
            lanqiao_cookie: String::new(),
        }
    }
}

impl AiConfig {
    pub fn enabled(&self) -> bool {
        crate::secrets::resolve_api_key(&self.api_key).is_some()
    }

    /// 实际生效的 key：SHUATI_API_KEY 环境变量 > 系统钥匙串 > config.toml
    pub fn resolved_key(&self) -> Option<String> {
        crate::secrets::resolve_api_key(&self.api_key)
    }
}

impl Config {
    pub fn load(path: &std::path::Path) -> Self {
        match std::fs::read_to_string(path) {
            Ok(text) => match toml::from_str(&text) {
                Ok(c) => c,
                Err(e) => {
                    // 解析失败必须让用户知道——静默回退默认会让配置"莫名失效"
                    tracing::warn!("config.toml 解析失败，使用默认配置: {e}");
                    Self::default()
                }
            },
            Err(_) => Self::default(),
        }
    }

    pub fn save(&self, path: &std::path::Path) -> Result<()> {
        Ok(atomic_write(path, toml::to_string_pretty(self)?.as_bytes())?)
    }
}
