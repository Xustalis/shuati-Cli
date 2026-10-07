//! shuati-gui: egui 主入口。

mod app;
mod fonts;
mod worker;
mod views;

fn main() -> eframe::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .with_writer(std::io::stderr)
        .init();

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([960.0, 640.0])
            .with_min_inner_size([720.0, 480.0])
            .with_title("刷题助手 · shuati"),
        ..Default::default()
    };

    eframe::run_native(
        "shuati",
        options,
        Box::new(|cc| Ok(Box::new(app::ShuatiApp::new(cc)))),
    )
}
