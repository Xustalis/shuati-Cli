//! 应用主状态与事件循环。

use crate::worker::{Bridge, Event, Task};
use shuati_core::{TestReport, Verdict};
use shuati_store as store;
use std::path::PathBuf;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

#[derive(PartialEq, Clone, Copy)]
pub enum Tab {
    Test,
    Library,
    Stress,
    Settings,
}

/// 评测运行状态（UI 侧）
#[derive(Default)]
pub struct RunState {
    pub running: bool,
    pub compiling: bool,
    pub done: usize,
    pub total: usize,
    pub live_cases: Vec<(String, Verdict)>,
    pub report: Option<TestReport>,
    pub error: Option<String>,
    /// diff 弹窗：选中哪个 case
    pub diff_case: Option<usize>,
}

pub struct ShuatiApp {
    pub ws: store::Workspace,
    pub cfg: store::Config,
    pub state: store::AppState,
    pub problems: Vec<store::ProblemDir>,
    pub tab: Tab,
    pub bridge: Bridge,

    // ── 评测屏
    pub run: RunState,
    /// watch 句柄（保持存活才有用）
    _watcher: Option<notify::RecommendedWatcher>,
    pub watch_rx: Option<std::sync::mpsc::Receiver<PathBuf>>,
    /// 新建源文件路径输入
    pub src_input: String,

    // ── 题库屏
    pub new_title: String,
    pub url_input: String,
    pub lib_msg: String,

    // ── 设置屏（编辑中的副本）
    pub cfg_cpp: String,
    pub cfg_py: String,
    pub cfg_api_key: String,
    pub cfg_editor: String,
    pub env: Option<shuati_judge::EnvReport>,
}

impl ShuatiApp {
    pub fn new(_cc: &eframe::CreationContext) -> Self {
        let root = store::Workspace::default_dir();
        let ws = store::Workspace::open(&root).expect("open workspace");
        let cfg = store::Config::load(&ws.config_path());
        let state = store::AppState::load(&ws.state_path());
        let problems = store::load_all_problems(&ws);
        let bridge = Bridge::spawn(root.clone());

        let mut app = Self {
            cfg_cpp: cfg.compiler_cpp.clone(),
            cfg_py: cfg.compiler_python.clone(),
            cfg_api_key: cfg.ai.api_key.clone(),
            cfg_editor: cfg.editor.clone(),
            ws,
            cfg,
            state,
            problems,
            tab: Tab::Test,
            bridge,
            run: RunState::default(),
            _watcher: None,
            watch_rx: None,
            src_input: String::new(),
            new_title: String::new(),
            url_input: String::new(),
            lib_msg: String::new(),
            env: None,
        };
        // 恢复 watch 状态
        if let Some(f) = app.state.watch_file.clone() {
            app.set_watch_file(f);
        }
        app.refresh_env();
        app
    }

    pub fn refresh_env(&mut self) {
        self.env = Some(shuati_judge::check_env(&self.cfg.compiler_cpp, &self.cfg.compiler_python));
    }

    pub fn current_problem(&self) -> Option<&store::ProblemDir> {
        let cur = self.state.current_problem.as_ref()?;
        self.problems.iter().find(|p| &p.meta.id.to_string() == cur)
    }

    pub fn select_problem(&mut self, id: String) {
        self.state.current_problem = Some(id);
        self.save_state();
    }

    pub fn save_state(&self) {
        let _ = self.state.save(&self.ws.state_path());
    }

    pub fn reload_problems(&mut self) {
        self.problems = store::load_all_problems(&self.ws);
    }

    /// 设置被监视文件（拖入或恢复）
    pub fn set_watch_file(&mut self, path: PathBuf) {
        match shuati_judge::watch_file(path.clone()) {
            Ok((w, rx)) => {
                self._watcher = Some(w);
                self.watch_rx = Some(rx);
                self.state.watch_file = Some(path);
            }
            Err(e) => {
                self.run.error = Some(format!("watch 失败: {e}"));
                self._watcher = None;
                self.watch_rx = None;
                self.state.watch_file = None;
            }
        }
        self.save_state();
    }

    /// 触发一次评测（按钮或 watch 回调）
    pub fn trigger_test(&mut self) {
        let (Some(pd), Some(src)) = (
            self.current_problem().map(|p| p.dir.clone()),
            self.state.watch_file.clone(),
        ) else {
            self.run.error = Some("先选题目和源文件".into());
            return;
        };
        let problem_id = self.current_problem().unwrap().meta.id.clone();

        self.bridge.cancel_current();
        let cancel = Arc::new(AtomicBool::new(false));
        self.run = RunState { running: true, ..Default::default() };

        let compiler_cfg = match shuati_core::Lang::from_path(&src) {
            Some(shuati_core::Lang::Cpp) => self.cfg.compiler_cpp.clone(),
            Some(shuati_core::Lang::Python) => self.cfg.compiler_python.clone(),
            None => String::new(),
        };
        let limits = self.current_problem().unwrap().meta.limits;
        let checker = self.current_problem().unwrap().meta.checker.clone();

        self.bridge.request(Task::RunTest {
            problem_dir: pd,
            problem_id,
            src,
            limits,
            checker,
            compiler_cfg,
            cancel,
        });
    }

    /// 处理后台事件（每帧调用）
    fn drain_events(&mut self, ctx: &egui::Context) {
        // watch 事件 → 自动重测
        if self.state.watch_enabled {
            if let Some(rx) = &self.watch_rx {
                if rx.try_recv().is_ok() && !self.run.running {
                    self.trigger_test();
                }
            }
        }
        while let Some(ev) = self.bridge.try_recv() {
            match ev {
                Event::Compiling => self.run.compiling = true,
                Event::CaseDone(i, total, name, v) => {
                    self.run.compiling = false;
                    self.run.done = i + 1;
                    self.run.total = total;
                    self.run.live_cases.push((name, v));
                }
                Event::RunDone(report) => {
                    self.run.running = false;
                    self.run.compiling = false;
                    self.state
                        .last_verdicts
                        .insert(report.problem.to_string(), report.verdict.to_string());
                    self.save_state();
                    self.run.report = Some(*report);
                }
                Event::Failed(msg) => {
                    self.run.running = false;
                    self.run.compiling = false;
                    self.run.error = Some(msg);
                }
            }
        }
        if self.run.running || self.state.watch_enabled {
            ctx.request_repaint_after(std::time::Duration::from_millis(100));
        }
    }
}

impl eframe::App for ShuatiApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.drain_events(ctx);

        // 拖拽处理
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        if let Some(f) = dropped.first() {
            if let Some(p) = &f.path {
                if shuati_core::Lang::from_path(p).is_some() {
                    self.set_watch_file(p.clone());
                    if self.state.watch_enabled && !self.run.running {
                        self.trigger_test();
                    }
                }
            }
        }

        crate::views::render(self, ctx);
    }
}
