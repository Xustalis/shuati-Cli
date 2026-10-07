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

/// 对拍屏状态
#[derive(Default)]
pub struct StressState {
    pub running: bool,
    pub done: u32,
    pub matched: u32,
    pub hack_count: u32,
    pub gen_fail: u32,
    pub oracle_fail: u32,
    pub hacks: Vec<shuati_judge::HackCase>,
    pub report: Option<shuati_judge::StressReport>,
    /// 输入框
    pub gen_path: String,
    pub oracle_path: String,
    pub rounds_input: String,
    pub seed_input: String,
    pub keep_going: bool,
    /// 展开哪个 hack
    pub open_hack: Option<usize>,
    pub msg: String,
}

/// AI 状态（hint/诊断文本 + busy）
#[derive(Default)]
pub struct AiState {
    pub busy: bool,
    pub text: String,
    pub show: bool,
}

/// 用例编辑器状态（评测屏下半区）
#[derive(Default)]
pub struct CaseEditor {
    /// 已载入题目的用例列表（name, in, out）
    pub cases: Vec<(String, String, String)>,
    /// 载入自哪个题目
    pub loaded_for: Option<String>,
    /// 新增用例输入框
    pub new_in: String,
    pub new_out: String,
    pub show_add: bool,
    /// 正在编辑的用例名（None=新增模式）
    pub editing: Option<String>,
    /// 查看哪个用例详情
    pub view_case: Option<usize>,
    pub msg: String,
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
    /// 题面是否展开
    pub show_statement: bool,
    /// 用例编辑器
    pub cases: CaseEditor,

    // ── 题库屏
    pub new_title: String,
    pub url_input: String,
    pub lib_msg: String,
    pub fetching: bool,

    // ── 对拍屏
    pub stress: StressState,

    // ── AI
    pub ai: AiState,

    // ── 设置屏（编辑中的副本）
    pub cfg_cpp: String,
    pub cfg_py: String,
    pub cfg_api_key: String,
    pub cfg_api_base: String,
    pub cfg_model: String,
    pub cfg_editor: String,
    pub env: Option<shuati_judge::EnvReport>,
}

impl ShuatiApp {
    pub fn new(cc: &eframe::CreationContext) -> Self {
        // egui 默认字体无 CJK 字形，必须先装中文字体否则全屏豆腐块
        crate::fonts::install(&cc.egui_ctx);
        crate::fonts::tune_text_styles(&cc.egui_ctx);

        let root = store::Workspace::default_dir();
        let (ws, init_err) = match store::Workspace::open(&root) {
            Ok(w) => (w, None),
            Err(e) => (store::Workspace { root: root.clone() }, Some(format!("工作区初始化失败: {e}"))),
        };
        let cfg = store::Config::load(&ws.config_path());
        let state = store::AppState::load(&ws.state_path());
        let problems = store::load_all_problems(&ws);
        let bridge = Bridge::spawn(root.clone());

        let mut app = Self {
            cfg_cpp: cfg.compiler_cpp.clone(),
            cfg_py: cfg.compiler_python.clone(),
            cfg_api_key: cfg.ai.api_key.clone(),
            cfg_api_base: cfg.ai.api_base.clone(),
            cfg_model: cfg.ai.model.clone(),
            cfg_editor: cfg.editor.clone(),
            ws,
            cfg,
            state,
            problems,
            tab: Tab::Test,
            bridge,
            run: RunState { error: init_err, ..Default::default() },
            _watcher: None,
            watch_rx: None,
            src_input: String::new(),
            show_statement: false,
            cases: CaseEditor::default(),
            new_title: String::new(),
            url_input: String::new(),
            lib_msg: String::new(),
            fetching: false,
            stress: StressState {
                rounds_input: "100".into(),
                ..Default::default()
            },
            ai: AiState::default(),
            env: None,
        };
        // 恢复 watch 状态
        if let Some(f) = app.state.watch_file.clone() {
            app.set_watch_file(f);
        }
        app.refresh_env();
        app.reload_cases();
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
        self.state.current_problem = Some(id.clone());
        self.save_state();
        self.reload_cases();
        // 换题时对拍路径自动重探测
        self.autodetect_stress_paths();
    }

    pub fn save_state(&self) {
        let _ = self.state.save(&self.ws.state_path());
    }

    pub fn reload_problems(&mut self) {
        self.problems = store::load_all_problems(&self.ws);
    }

    /// 载入当前题目的用例到编辑器
    pub fn reload_cases(&mut self) {
        let Some(pd) = self.current_problem() else {
            self.cases.cases.clear();
            self.cases.loaded_for = None;
            return;
        };
        let pid = pd.meta.id.to_string();
        match pd.load_cases() {
            Ok(cases) => {
                self.cases.cases = cases
                    .into_iter()
                    .map(|c| (c.name, c.input, c.expected.unwrap_or_default()))
                    .collect();
                self.cases.loaded_for = Some(pid);
            }
            Err(e) => {
                self.cases.msg = format!("读用例失败: {e}");
            }
        }
    }

    /// 用例编辑器：新增一个用例
    pub fn add_case(&mut self, input: &str, output: &str) {
        let Some(pd) = self.current_problem() else { return };
        let name = if let Some(n) = &self.cases.editing {
            n.clone()
        } else {
            // 下一个编号
            let next = self
                .cases
                .cases
                .iter()
                .filter_map(|(n, _, _)| n.parse::<u32>().ok())
                .max()
                .unwrap_or(0)
                + 1;
            format!("{next:04}")
        };
        match pd.write_case(&name, input, output, shuati_core::CaseOrigin::Sample) {
            Ok(()) => {
                self.cases.msg = format!("已{}用例 {name}", if self.cases.editing.is_some() { "更新" } else { "添加" });
                self.cases.editing = None;
                self.reload_cases();
            }
            Err(e) => self.cases.msg = format!("添加失败: {e}"),
        }
    }

    /// 用例编辑器：删除用例
    pub fn delete_case(&mut self, name: &str) {
        let Some(pd) = self.current_problem() else { return };
        match pd.delete_case(name) {
            Ok(()) => {
                self.cases.msg = format!("已删除 {name}");
                self.reload_cases();
            }
            Err(e) => self.cases.msg = format!("删除失败: {e}"),
        }
    }

    /// 对拍路径自动探测：gen/gen.* 和 solutions/brute.*
    pub fn autodetect_stress_paths(&mut self) {
        let Some(pd) = self.current_problem() else { return };
        let gen_dir = pd.gen_dir();
        let sol_dir = pd.solutions_dir();
        if self.stress.gen_path.is_empty() {
            if let Some(p) = shuati_judge::find_script(&gen_dir, &["gen"]) {
                self.stress.gen_path = p.display().to_string();
            }
        }
        if self.stress.oracle_path.is_empty() {
            if let Some(p) = shuati_judge::find_script(&sol_dir, &["brute", "oracle", "std"]) {
                self.stress.oracle_path = p.display().to_string();
            }
        }
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

    /// 触发拉题
    pub fn trigger_fetch(&mut self, url: String) {
        self.fetching = true;
        self.lib_msg = "拉取中…".into();
        self.bridge.request(Task::FetchProblem { url });
    }

    /// 触发对拍
    pub fn trigger_stress(&mut self) {
        let Some(pd) = self.current_problem() else {
            self.stress.msg = "先选题目".into();
            return;
        };
        let problem_dir = pd.dir.clone();
        let limits = pd.meta.limits;
        let checker = pd.meta.checker.clone();
        let found = pd.find_solution();
        let Some(user) = self.state.watch_file.clone().or(found) else {
            self.stress.msg = "没有用户解——先在评测页选定源文件".into();
            return;
        };
        if self.stress.gen_path.is_empty() || self.stress.oracle_path.is_empty() {
            self.autodetect_stress_paths();
        }
        let (gp, op) = (self.stress.gen_path.trim(), self.stress.oracle_path.trim());
        if gp.is_empty() || op.is_empty() {
            self.stress.msg = "需要生成器和标程路径（gen/gen.* 和 solutions/brute.*）".into();
            return;
        }
        let mk = |p: &str| -> Option<shuati_core::SourceFile> {
            let path = PathBuf::from(p);
            let lang = shuati_core::Lang::from_path(&path)?;
            Some(shuati_core::SourceFile { path, lang })
        };
        let (Some(g), Some(u), Some(o)) = (mk(gp), mk(&user.display().to_string()), mk(op)) else {
            self.stress.msg = "文件类型不支持（.cpp/.py）".into();
            return;
        };
        let rounds = self.stress.rounds_input.trim().parse::<u32>().unwrap_or(100).max(1);
        // 空 seed → 时间戳当随机起点
        let seed = self
            .stress
            .seed_input
            .trim()
            .parse::<u64>()
            .unwrap_or((shuati_store::now_ts() as u64) % 1_000_000 + 1);

        self.bridge.cancel_current();
        let cancel = Arc::new(AtomicBool::new(false));
        self.stress.running = true;
        self.stress.done = 0;
        self.stress.matched = 0;
        self.stress.hack_count = 0;
        self.stress.gen_fail = 0;
        self.stress.oracle_fail = 0;
        self.stress.hacks.clear();
        self.stress.report = None;
        self.stress.msg.clear();

        self.bridge.request(Task::RunStress {
            problem_dir,
            gen_src: g,
            user_src: u,
            oracle_src: o,
            rounds,
            seed_start: seed,
            jobs: 4,
            limits,
            checker,
            cpp_cfg: self.cfg.compiler_cpp.clone(),
            py_cfg: self.cfg.compiler_python.clone(),
            stop_on_fail: !self.stress.keep_going,
            cancel,
        });
    }

    /// 触发 AI 提示/诊断
    pub fn trigger_ai(&mut self, explain: bool) {
        let Some(pd) = self.current_problem() else {
            self.ai.text = "先选题目".into();
            self.ai.show = true;
            return;
        };
        let statement = pd.read_statement().unwrap_or_default();
        // 代码：watch 文件 > solutions/main
        let code = self
            .state
            .watch_file
            .as_ref()
            .and_then(|p| std::fs::read_to_string(p).ok())
            .or_else(|| pd.find_solution().and_then(|p| std::fs::read_to_string(p).ok()))
            .unwrap_or_else(|| "（无代码）".into());
        // 判题上下文
        let verdict_info = self
            .run
            .report
            .as_ref()
            .map(|r| {
                let fails: Vec<String> = r
                    .cases
                    .iter()
                    .filter(|c| c.verdict != Verdict::Ac)
                    .take(3)
                    .map(|c| {
                        format!(
                            "{}: {}（期望 {} 实际 {}）",
                            c.name,
                            c.verdict,
                            c.expected.lines().next().unwrap_or(""),
                            c.actual.lines().next().unwrap_or("")
                        )
                    })
                    .collect();
                format!("判定 {}，{}/{}。失败:\n{}", r.verdict, r.pass, r.total, fails.join("\n"))
            })
            .unwrap_or_else(|| "（尚未评测）".into());

        self.ai.busy = true;
        self.ai.show = true;
        self.ai.text = "AI 思考中…".into();
        self.bridge.request(Task::AiAsk {
            api_base: self.cfg.ai.api_base.clone(),
            api_key: self.cfg.ai.api_key.clone(),
            model: self.cfg.ai.model.clone(),
            statement,
            code,
            verdict_info,
            explain,
        });
    }

    /// 触发 AI 生成对拍脚本
    pub fn trigger_ai_scripts(&mut self) {
        let Some(pd) = self.current_problem() else {
            self.stress.msg = "先选题目".into();
            return;
        };
        let statement = pd.read_statement().unwrap_or_default();
        let dest = pd.dir.clone();
        // 脚本语言跟用户解一致
        let lang = self
            .state
            .watch_file
            .as_ref()
            .and_then(|p| shuati_core::Lang::from_path(p))
            .unwrap_or(shuati_core::Lang::Cpp);
        self.ai.busy = true;
        self.ai.show = true;
        self.ai.text = "AI 生成 + dry-run 验证中，可能要 1-2 分钟…".into();
        self.bridge.request(Task::AiScripts {
            api_base: self.cfg.ai.api_base.clone(),
            api_key: self.cfg.ai.api_key.clone(),
            model: self.cfg.ai.model.clone(),
            statement,
            lang,
            dest_dir: dest,
            cpp_cfg: self.cfg.compiler_cpp.clone(),
            py_cfg: self.cfg.compiler_python.clone(),
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
                    self.stress.running = false;
                    self.run.error = Some(msg);
                }
                Event::FetchedOk(id) => {
                    self.fetching = false;
                    self.lib_msg = format!("已拉取 {id}");
                    self.reload_problems();
                    self.select_problem(id);
                }
                Event::FetchFailed(msg) => {
                    self.fetching = false;
                    self.lib_msg = format!("拉取失败: {msg}");
                }
                Event::StressTick(done, matched, hacks, gen_fail, oracle_fail) => {
                    self.stress.done = done;
                    self.stress.matched = matched;
                    self.stress.hack_count = hacks;
                    self.stress.gen_fail = gen_fail;
                    self.stress.oracle_fail = oracle_fail;
                }
                Event::StressHack(case) => {
                    self.stress.hacks.push(*case);
                }
                Event::StressDone(report) => {
                    self.stress.running = false;
                    self.stress.report = Some(*report);
                }
                Event::AiText(t) => {
                    self.ai.busy = false;
                    self.ai.text = t;
                    self.ai.show = true;
                }
                Event::ScriptsReady(g, o) => {
                    self.ai.busy = false;
                    self.ai.text = format!("脚本已生成并验证通过:\n  gen    = {}\n  oracle = {}", g.display(), o.display());
                    self.ai.show = true;
                    self.stress.gen_path = g.display().to_string();
                    self.stress.oracle_path = o.display().to_string();
                }
            }
        }
        if self.run.running || self.stress.running || self.state.watch_enabled || self.fetching || self.ai.busy {
            ctx.request_repaint_after(std::time::Duration::from_millis(100));
        }
    }
}

impl eframe::App for ShuatiApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.drain_events(ctx);

        // 拖拽处理：.cpp/.py → 源文件；.in → 追加用例
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        for f in &dropped {
            let Some(p) = &f.path else { continue };
            if shuati_core::Lang::from_path(p).is_some() {
                self.set_watch_file(p.clone());
                // 拖入即评测：用户意图明确，不要求先开 watch 开关
                if !self.run.running {
                    self.trigger_test();
                }
            } else if p.extension().is_some_and(|e| e == "in") {
                // 拖入 .in → 加到用例
                if let (Ok(input), Some(pd)) = (
                    std::fs::read_to_string(p),
                    self.current_problem().map(|x| x.dir.clone()),
                ) {
                    let out_p = p.with_extension("out");
                    let output = std::fs::read_to_string(&out_p).unwrap_or_default();
                    let next = self.cases.cases.len() as u32 + 1;
                    let name = format!("{next:04}");
                    let pd2 = shuati_store::ProblemDir::open(&pd);
                    if let Ok(pd2) = pd2 {
                        let _ = pd2.write_case(&name, &input, &output, shuati_core::CaseOrigin::Sample);
                        self.reload_cases();
                        self.cases.msg = format!("拖入用例 {name}");
                    }
                }
            }
        }

        crate::views::render(self, ctx);
    }
}
