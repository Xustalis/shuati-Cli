//! 后台任务桥：tokio runtime 线程 + mpsc 事件。
//! 规则：UI 与任务事件走同一 mpsc；每个任务持 cancel flag；禁止裸 thread detach。

use shuati_core::{Lang, SourceFile, TestReport, Verdict};
use shuati_judge as judge;
use shuati_store as store;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use tokio::sync::mpsc;

/// 后台 → UI 事件
#[derive(Debug)]
pub enum Event {
    /// 编译中
    Compiling,
    /// 单点结果（case_index, total, name, verdict）
    CaseDone(usize, usize, String, Verdict),
    /// 全部完成
    RunDone(Box<TestReport>),
    /// 错误（编译/环境）
    Failed(String),
}

/// UI → 后台 命令
pub enum Task {
    /// 评测：problem_dir + 源文件
    RunTest {
        problem_dir: PathBuf,
        problem_id: shuati_core::ProblemId,
        src: PathBuf,
        limits: shuati_core::Limits,
        checker: shuati_core::CheckerSpec,
        compiler_cfg: String,
        cancel: Arc<AtomicBool>,
    },
}

pub struct Bridge {
    pub tx: mpsc::UnboundedSender<Task>,
    pub rx: std::sync::mpsc::Receiver<Event>,
    /// 当前运行任务的取消标志
    pub current_cancel: Arc<Mutex<Option<Arc<AtomicBool>>>>,
}

impl Bridge {
    /// 启动 tokio runtime 线程
    pub fn spawn(_ws_root: PathBuf) -> Self {
        let (task_tx, mut task_rx) = mpsc::unbounded_channel::<Task>();
        let (ev_tx, ev_rx) = std::sync::mpsc::channel::<Event>();
        let cancel_slot: Arc<Mutex<Option<Arc<AtomicBool>>>> = Arc::new(Mutex::new(None));

        let slot = cancel_slot.clone();
        std::thread::spawn(move || {
            let rt = tokio::runtime::Runtime::new().expect("tokio runtime");
            rt.block_on(async move {
                while let Some(task) = task_rx.recv().await {
                    match task {
                        Task::RunTest { problem_dir, problem_id, src, limits, checker, compiler_cfg, cancel } => {
                            *slot.lock().unwrap() = Some(cancel.clone());
                            let ev = ev_tx.clone();
                            tokio::spawn(async move {
                                run_test(problem_dir, problem_id, src, limits, checker, compiler_cfg, cancel, ev).await;
                            });
                        }
                    }
                }
            });
        });

        // 任务处理（NewProblem/Reload 需要 store，放这里处理）
        // —— 简化：这些走同一个 runtime 线程太重，改由 UI 线程直接做（同步 IO 很快）
        Self { tx: task_tx, rx: ev_rx, current_cancel: cancel_slot }
    }

    pub fn request(&self, t: Task) {
        let _ = self.tx.send(t);
    }

    /// 取消当前任务
    pub fn cancel_current(&self) {
        if let Some(c) = self.current_cancel.lock().unwrap().as_ref() {
            c.store(true, Ordering::Relaxed);
        }
    }

    /// 非阻塞取事件
    pub fn try_recv(&self) -> Option<Event> {
        self.rx.try_recv().ok()
    }
}

async fn run_test(
    problem_dir: PathBuf,
    problem_id: shuati_core::ProblemId,
    src: PathBuf,
    limits: shuati_core::Limits,
    checker: shuati_core::CheckerSpec,
    compiler_cfg: String,
    cancel: Arc<AtomicBool>,
    ev: std::sync::mpsc::Sender<Event>,
) {
    let pd = match store::ProblemDir::open(&problem_dir) {
        Ok(p) => p,
        Err(e) => {
            let _ = ev.send(Event::Failed(format!("题目目录错误: {e}")));
            return;
        }
    };
    let cases = match pd.load_cases() {
        Ok(c) if !c.is_empty() => c,
        Ok(_) => {
            let _ = ev.send(Event::Failed("tests/ 下没有用例".into()));
            return;
        }
        Err(e) => {
            let _ = ev.send(Event::Failed(format!("读用例失败: {e}")));
            return;
        }
    };
    let lang = match Lang::from_path(&src) {
        Some(l) => l,
        None => {
            let _ = ev.send(Event::Failed("不支持的文件类型".into()));
            return;
        }
    };
    let tc = match judge::Toolchain::detect(lang, &compiler_cfg) {
        Ok(t) => t,
        Err(e) => {
            let _ = ev.send(Event::Failed(e.to_string()));
            return;
        }
    };

    let _ = ev.send(Event::Compiling);
    let engine = judge::TestEngine::new(tc);
    let src_file = SourceFile { path: src, lang };

    let ev2 = ev.clone();
    let progress: judge::ProgressFn = Arc::new(move |i, total, r| {
        let _ = ev2.send(Event::CaseDone(i, total, r.name.clone(), r.verdict));
    });

    let params = judge::RunParams {
        problem: &problem_id,
        solution: &src_file,
        cases: &cases,
        limits: &limits,
        checker: &checker,
        jobs: 4,
        progress: Some(progress),
        cancel: Some(cancel),
    };

    match engine.run(&params).await {
        Ok(report) => {
            let _ = pd.save_report(&report);
            let _ = ev.send(Event::RunDone(Box::new(report)));
        }
        Err(judge::JudgeError::Compile(msg)) => {
            let _ = ev.send(Event::Failed(format!("编译失败:\n{msg}")));
        }
        Err(e) => {
            let _ = ev.send(Event::Failed(e.to_string()));
        }
    }
}
