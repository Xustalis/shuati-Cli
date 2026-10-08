//! shuati-gui: egui 主入口。
// Windows 下 release 不弹控制台黑窗
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod app;
mod fonts;
mod views;
mod worker;

fn main() -> eframe::Result<()> {
    // --root <dir> 指定工作区（默认 exe 旁 shuati-data/）
    let mut args = std::env::args().skip(1);
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
