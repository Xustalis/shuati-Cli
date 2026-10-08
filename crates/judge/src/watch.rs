//! 保存即评测：notify watcher + 去抖。
//! 文件变更 → 触发回调（回调里应调度 TestEngine）。
//! 去抖用 leading-edge：首事件即发，窗口期内后续事件忽略——不开额外线程。

use notify::{Event, EventKind, RecursiveMode, Watcher};
use std::path::PathBuf;
use std::sync::Mutex;
use std::sync::mpsc::{Receiver, channel};
use std::time::{Duration, Instant};

/// 防抖间隔：连续保存只触发一次
const DEBOUNCE: Duration = Duration::from_millis(300);

/// 启动文件监视；文件变化时向返回的 Receiver 发送路径。
/// 返回 (watcher 句柄, receiver)——watcher 必须保持存活。
pub fn watch_file(path: PathBuf) -> notify::Result<(notify::RecommendedWatcher, Receiver<PathBuf>)> {
    let (tx, rx) = channel();
    // 规范化路径：notify 事件给的是绝对路径，调用方给的可能是相对/含符号链接路径
    let target = path.canonicalize().unwrap_or_else(|_| path.clone());
    let last = Mutex::new(Instant::now() - DEBOUNCE);
    let hit_path = target.clone();

    let mut watcher = notify::recommended_watcher(move |res: notify::Result<Event>| {
        let Ok(ev) = res else { return };
        let dominated = matches!(ev.kind, EventKind::Modify(_) | EventKind::Create(_) | EventKind::Any);
        if !dominated || !ev.paths.iter().any(|p| p == &hit_path) {
            return;
        }
        // leading-edge 去抖：窗口期外的事件立刻发，期内的丢弃
        let mut g = last.lock().unwrap();
        if g.elapsed() >= DEBOUNCE {
            *g = Instant::now();
            let _ = tx.send(hit_path.clone());
        }
    })?;

    // watch 父目录（文件可能被编辑器替换式保存）
    let parent = target.parent().unwrap_or_else(|| std::path::Path::new(".")).to_path_buf();
    watcher.watch(&parent, RecursiveMode::NonRecursive)?;

    Ok((watcher, rx))
}
