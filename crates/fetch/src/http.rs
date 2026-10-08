//! 共享 HTTP 客户端。

use crate::FetchError;
use std::sync::OnceLock;

static CLIENT: OnceLock<reqwest::Client> = OnceLock::new();

pub fn client() -> &'static reqwest::Client {
    CLIENT.get_or_init(|| {
        reqwest::Client::builder()
            .user_agent("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36")
            .cookie_store(true)
            .timeout(std::time::Duration::from_secs(20))
            .connect_timeout(std::time::Duration::from_secs(10))
            .build()
            .expect("reqwest client")
    })
}

pub async fn get_text(url: &str) -> Result<String, FetchError> {
    let resp = client().get(url).send().await.map_err(|e| FetchError::Network(e.to_string()))?;
    if !resp.status().is_success() {
        return Err(FetchError::Network(format!("HTTP {} on {}", resp.status(), url)));
    }
    resp.text().await.map_err(|e| FetchError::Network(e.to_string()))
}
