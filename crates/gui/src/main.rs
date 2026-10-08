//! shuati-gui: egui 主入口。
// Windows 下 release 不弹控制台黑窗
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod app;
mod fonts;
mod views;
mod worker;

fn main() -> eframe::Result<()> {
    let raw: Vec<String> = std::env::args().skip(1).collect();

    // 除 --root <dir> 外还有别的参数 → 用户其实在敲 CLI：转发给同目录 shuati-cli。
    // 单个 exe 心智模型：`shuati test …` 也能跑（输出透传、退出码带回）。
    let mut cliish = false;
    let mut it = raw.iter().peekable();
    while let Some(a) = it.next() {
        if a == "--root" {
            it.next();
        } else {
            cliish = true;
        }
    }
    if cliish && let Some(status) = forward_to_cli(&raw) {
        std::process::exit(status);
    }

    // --root <dir> 指定工作区（默认 exe 旁 shuati-data/）
    let mut args = raw.iter();
    let mut root = None;
    while let Some(a) = args.next() {
        if a == "--root" {
            root = args.next().map(std::path::PathBuf::from);
        }
    }
    let root = root.unwrap_or_else(shuati_store::Workspace::default_dir);

    // stderr + 滚动文件双写（GUI release 无控制台，文件是唯一的日志出口）
    {
        use tracing_subscriber::fmt::writer::MakeWriterExt;
        let appender = tracing_appender::rolling::daily(root.join("logs"), "shuati-gui.log");
        tracing_subscriber::fmt()
            .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
            .with_writer(std::io::stderr.and(appender))
            .with_ansi(false)
            .init();
    }

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([960.0, 640.0])
            .with_min_inner_size([720.0, 480.0])
            .with_title("刷题助手 · shuati"),
        ..Default::default()
    };

    eframe::run_native("shuati", options, Box::new(|cc| Ok(Box::new(app::ShuatiApp::new(cc, root)))))
}

/// `shuati <子命令>` → 转给同目录 shuati-cli 跑，退出码透传。
/// 找不到 shuati-cli（用户只拷了一个 exe）→ 打提示返回 None，继续开 GUI。
fn forward_to_cli(args: &[String]) -> Option<i32> {
    let cli = std::env::current_exe()
        .ok()
        .and_then(|e| e.parent().map(|d| d.join(format!("shuati-cli{}", std::env::consts::EXE_SUFFIX))))
        .filter(|p| p.is_file())?;
    match std::process::Command::new(&cli).args(args).status() {
        Ok(s) => Some(s.code().unwrap_or(1)),
        Err(_) => None,
    }
}
