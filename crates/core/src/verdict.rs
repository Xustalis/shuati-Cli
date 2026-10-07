use serde::{Deserialize, Serialize};
use std::fmt;

/// 判题结果
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Verdict {
    Ac,
    Wa,
    Tle,
    Mle,
    Re,
    Ce,
    /// 系统错误（沙箱自身故障等）
    Se,
}

impl Verdict {
    pub fn is_success(self) -> bool {
        matches!(self, Verdict::Ac)
    }
}

impl fmt::Display for Verdict {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            Verdict::Ac => "AC",
            Verdict::Wa => "WA",
            Verdict::Tle => "TLE",
            Verdict::Mle => "MLE",
            Verdict::Re => "RE",
            Verdict::Ce => "CE",
            Verdict::Se => "SE",
        };
        f.write_str(s)
    }
}
