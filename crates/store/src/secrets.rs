//! API key 存取：环境变量 > 系统钥匙串 > config.toml。
//! 钥匙串不可用的平台（如无 Secret Service 的 Linux）静默降级到配置文件。

const SERVICE: &str = "shuati";
const NAME: &str = "ai.api_key";

/// key 从哪来的（设置页展示用）
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeySource {
    /// SHUATI_API_KEY 环境变量
    Env,
    /// 系统钥匙串（Windows 凭据管理器 / macOS Keychain / Secret Service）
    Keyring,
    /// config.toml 明文（旧方式，仍在兼容期内）
    File,
    None,
}

/// 实际生效的 key。`file_key` 传 config.toml 里的 ai.api_key。
pub fn resolve_api_key(file_key: &str) -> Option<String> {
    let env = std::env::var("SHUATI_API_KEY").ok().filter(|k| !k.trim().is_empty());
    pick_key(env, keyring_get(), file_key)
}

/// 优先级选择的纯逻辑（可测）：env > keyring > file
fn pick_key(env: Option<String>, ring: Option<String>, file_key: &str) -> Option<String> {
    env.or(ring).or_else(|| if file_key.trim().is_empty() { None } else { Some(file_key.to_string()) })
}

/// key 的来源（用于 UI 展示"key 已在系统钥匙串"之类的提示）
pub fn key_source(file_key: &str) -> KeySource {
    if std::env::var("SHUATI_API_KEY").is_ok_and(|k| !k.trim().is_empty()) {
        return KeySource::Env;
    }
    if keyring_get().is_some() {
        return KeySource::Keyring;
    }
    if !file_key.trim().is_empty() {
        return KeySource::File;
    }
    KeySource::None
}

/// 把 key 存进系统钥匙串。返回 Err 说明平台不支持，调用方回退写 config.toml。
pub fn store_api_key(key: &str) -> std::io::Result<()> {
    let e = keyring::Entry::new(SERVICE, NAME).map_err(|e| io_err(format!("{e}")))?;
    e.set_password(key).map_err(|e| io_err(format!("{e}")))
}

/// 删掉钥匙串里的 key（用户清空输入框时用；不存在不算错）
pub fn delete_api_key() {
    if let Ok(e) = keyring::Entry::new(SERVICE, NAME) {
        let _ = e.delete_credential();
    }
}

fn keyring_get() -> Option<String> {
    keyring::Entry::new(SERVICE, NAME).ok()?.get_password().ok().filter(|s| !s.trim().is_empty())
}

fn io_err(msg: String) -> std::io::Error {
    std::io::Error::new(std::io::ErrorKind::Unsupported, msg)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn precedence_env_over_keyring_over_file() {
        let s = |x: &str| Some(x.to_string());
        assert_eq!(pick_key(s("e"), s("r"), "f"), s("e"));
        assert_eq!(pick_key(None, s("r"), "f"), s("r"));
        assert_eq!(pick_key(None, None, "f"), s("f"));
        assert_eq!(pick_key(s("e"), None, "f"), s("e"));
    }

    #[test]
    fn empty_file_key_yields_none() {
        assert_eq!(pick_key(None, None, ""), None);
        assert_eq!(pick_key(None, None, "   "), None);
    }
}
