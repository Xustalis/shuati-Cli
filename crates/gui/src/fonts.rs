//! CJK 字体加载：egui 默认字体无中文字形，全部显示豆腐块。
//! 按平台候选路径找系统中文字体，解析成功后挂到 Proportional/Monospace 两个
//! 字体族尾部做 fallback（拉丁仍用默认字体，中文走 CJK）。

use std::sync::Arc;

/// 各平台中文字体候选（按优先级）
const CANDIDATES: &[&str] = &[
    // macOS
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/STHeiti Medium.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
    // Windows
    "C:\\Windows\\Fonts\\msyh.ttc",
    "C:\\Windows\\Fonts\\msyh.ttf",
    "C:\\Windows\\Fonts\\simhei.ttf",
    "C:\\Windows\\Fonts\\simsun.ttc",
    // Linux
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/arphic/uming.ttc",
];

/// ttc 集合里最多尝试的面数
const MAX_FACES: u32 = 16;

/// 安装 CJK 字体到 egui context。返回加载成功的字体路径（None = 没找到）。
pub fn install(ctx: &egui::Context) -> Option<String> {
    let mut fonts = egui::FontDefinitions::default();
    let mut loaded: Option<String> = None;

    'files: for path in CANDIDATES {
        let Ok(bytes) = std::fs::read(path) else { continue };
        // ttc 集合：逐个 face index 试，第一个能解析的用
        for index in 0..MAX_FACES {
            if ab_glyph::FontVec::try_from_vec_and_index(bytes.clone(), index).is_ok() {
                let mut data = egui::FontData::from_owned(bytes.clone());
                data.index = index;
                fonts.font_data.insert("cjk".to_string(), Arc::new(data));
                loaded = Some(format!("{path}#{index}"));
                break 'files;
            }
            // index 0 都解析不了 → 不是字体文件，换下一个候选
            if index == 0 {
                break;
            }
        }
    }

    if let Some(found) = &loaded {
        fonts.families.entry(egui::FontFamily::Proportional).or_default().push("cjk".to_string());
        fonts.families.entry(egui::FontFamily::Monospace).or_default().push("cjk".to_string());
        ctx.set_fonts(fonts);
        tracing::info!("CJK font loaded: {found}");
    } else {
        tracing::warn!("no CJK font found on system; Chinese text will show as boxes");
    }
    loaded
}

/// 适当放大默认字号：中文在 egui 默认 12.5pt 下偏小
pub fn tune_text_styles(ctx: &egui::Context) {
    use egui::{FontId, TextStyle};
    ctx.style_mut(|style| {
        for ts in [TextStyle::Body, TextStyle::Button, TextStyle::Heading, TextStyle::Monospace] {
            style.text_styles.entry(ts).and_modify(|f| *f = FontId::new(f.size * 1.15, f.family.clone()));
        }
    });
}
