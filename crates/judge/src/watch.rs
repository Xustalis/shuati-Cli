//! 保存即评测：notify watcher + 去抖。
//! 文件变更 → 触发回调（回调里应调度 TestEngine）。

use notify::{Event, EventKind, RecursiveMode, Watcher};
use std::path::PathBuf;
use std::sync::mpsc::{channel, Receiver};
use std::time::{Duration, Instant};

/// 防抖间隔：连续保存只触发一次
const DEBOUNCE: Duration = Duration::from_millis(300);

/// 启动文件监视；文件变化时向返回的 Receiver 发送路径。
/// 返回 (watcher 句柄, receiver)——watcher 必须保持存活。
pub fn watch_file(path: PathBuf) -> notify::Result<(notify::RecommendedWatcher, Receiver<PathBuf>)> {
    let (tx, rx) = channel();
    let target = path.clone();

    let mut watcher = notify::recommended_watcher(move |res: notify::Result<Event>| {
        if let Ok(ev) = res {
            let dominated = matches!(
                ev.kind,
                EventKind::Modify(_) | EventKind::Create(_) | EventKind::Any
            );
            if dominated && ev.paths.iter().any(|p| p == &target) {
                let _ = tx.send(target.clone());
            }
        }
    })?;

    // watch 父目录（文件可能被编辑器替换式保存）
    let parent = path.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf();
    watcher.watch(&parent, RecursiveMode::NonRecursive)?;

    // 去抖线程：收通知 → 300ms 内合并 → 转发
    let (deb_tx, deb_rx) = channel();
    std::thread::spawn(move || {
        let mut last = Instant::now() - DEBOUNCE;
        while rx.recv().is_ok() {
            // 排空队列中的重复事件
            while rx.try_recv().is_ok() {}
            if last.elapsed() >= DEBOUNCE {
                last = Instant::now();
                if deb_tx.send(path.clone()).is_err() {
                    break;
                }
            } else {
                std::thread::sleep(DEBOUNCE - last.elapsed());
                last = Instant::now();
                if deb_tx.send(path.clone()).is_err() {
                    break;
                }
            }
        }
    });

    Ok((watcher, deb_rx))
}
