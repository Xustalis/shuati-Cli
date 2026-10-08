//! 应用主状态与事件循环。

use crate::worker::{self, Bridge, Event, Task};
use shuati_core::{Lang, SourceFile, TestReport, Verdict};
use shuati_store as store;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::atomic::AtomicBool;

#[derive(PartialEq, Clone, Copy)]
pub enum Tab {
    /// 快速运行（默认首屏）：贴输入直接跑
    Quick,
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
    /// diff 弹窗：选中哪个 case（按名字——live_cases 完成顺序与报告顺序不同）
    pub diff_case: Option<String>,
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
    /// 批量造例组数输入
    pub gen_count_input: String,
    /// 造例进行中
    pub genning: bool,
    pub msg: String,
}

/// 快速运行状态：贴输入直接跑，不需要题目
#[derive(Default)]
pub struct QuickState {
    pub input: String,
    pub expected: String,
    /// 是否展开期望输出框
    pub show_expected: bool,
    pub running: bool,
    pub result: Option<worker::QuickResult>,
    pub msg: String,
}

/// AI 状态（hint/诊断文本 + busy）
#[derive(Default)]
pub struct AiState {
    pub busy: bool,
    pub text: String,
    pub show: bool,
    /// SSE 流式中：首个 delta 到达时清掉"思考中"占位文本
    pub streaming: bool,
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

/// 题目设置编辑器（时限/内存/题面）
#[derive(Default)]
pub struct ProblemEditor {
    pub show: bool,
    pub cpu_ms: String,
    pub mem_mb: String,
    pub title: String,
    /// checker 选择（"tokens"|"exact"|"float"）
    pub checker: String,
    pub eps: String,
    pub statement: String,
    pub edit_statement: bool,
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
    /// 题面是否展开
    pub show_statement: bool,
    /// 题面缓存（题目id, 内容）：展开态每帧渲染，不能每帧读盘
    pub stmt_cache: Option<(String, String)>,
    /// 用例编辑器
    pub cases: CaseEditor,

    // ── 题库屏
    pub new_title: String,
    pub url_input: String,
    pub lib_msg: String,
    pub fetching: bool,

    // ── 快速运行屏
    pub quick: QuickState,

    // ── 对拍屏
    pub stress: StressState,

    // ── AI
    pub ai: AiState,
    /// 题目设置编辑器
    pub prob_edit: ProblemEditor,

    // ── 设置屏（编辑中的副本）
    pub cfg_cpp: String,
    pub cfg_py: String,
    pub cfg_api_key: String,
    pub cfg_api_base: String,
    pub cfg_model: String,
    pub cfg_editor: String,
    /// AI 连接测试状态
    pub cfg_msg: String,
    pub ai_pinging: bool,
    pub env: Option<shuati_judge::EnvReport>,
    /// watch 在评测运行中收到保存 → 跑完补一次重测
    pub pending_retest: bool,
    /// 删除题目二次确认（内存态，不落 state.json）
    pub confirm_delete: Option<String>,
    /// AI key 来源缓存：keyring 查询有 IPC 开销，不能每帧调，refresh_env 时刷新
    pub key_src: store::secrets::KeySource,
}

impl ShuatiApp {
    pub fn new(cc: &eframe::CreationContext, root: PathBuf) -> Self {
        // egui 默认字体无 CJK 字形，必须先装中文字体否则全屏豆腐块
        crate::fonts::install(&cc.egui_ctx);
        crate::fonts::tune_text_styles(&cc.egui_ctx);
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
            // 旧配置可能存过空串——空则回落到 DeepSeek 默认
            cfg_api_base: if cfg.ai.api_base.is_empty() {
                "https://api.deepseek.com/v1".into()
            } else {
                cfg.ai.api_base.clone()
            },
            cfg_model: if cfg.ai.model.is_empty() { "deepseek-chat".into() } else { cfg.ai.model.clone() },
            cfg_editor: cfg.editor.clone(),
            cfg_msg: String::new(),
            ai_pinging: false,
            ws,
            cfg,
            state,
            problems,
            tab: Tab::Quick,
            bridge,
            run: RunState { error: init_err, ..Default::default() },
            _watcher: None,
            watch_rx: None,
            show_statement: false,
            stmt_cache: None,
            cases: CaseEditor::default(),
            new_title: String::new(),
            url_input: String::new(),
            lib_msg: String::new(),
            fetching: false,
            stress: StressState { rounds_input: "100".into(), gen_count_input: "20".into(), ..Default::default() },
            quick: QuickState::default(),
            ai: AiState::default(),
            prob_edit: ProblemEditor::default(),
            env: None,
            pending_retest: false,
            confirm_delete: None,
            key_src: store::secrets::KeySource::None,
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
        self.key_src = store::secrets::key_source(&self.cfg.ai.api_key);
    }

    /// AI 是否可用（按钮置灰判断用缓存的 key 来源，不每帧查钥匙串）
    pub fn ai_ready(&self) -> bool {
        self.key_src != store::secrets::KeySource::None
    }

    pub fn current_problem(&self) -> Option<&store::ProblemDir> {
        let cur = self.state.current_problem.as_ref()?;
        self.problems.iter().find(|p| &p.meta.id.to_string() == cur)
    }

    /// 当前题题面（带缓存）。换题/编辑器保存后按 id 失效。
    pub fn statement_cached(&mut self) -> &str {
        let (pid, text) = match self.current_problem() {
            Some(pd) => {
                let pid = pd.meta.id.to_string();
                let hit = self.stmt_cache.as_ref().is_some_and(|(id, _)| id == &pid);
                (pid, if hit { None } else { Some(pd.read_statement().unwrap_or_default()) })
            }
            None => return "",
        };
        if let Some(t) = text {
            self.stmt_cache = Some((pid, t));
        }
        self.stmt_cache.as_ref().map(|(_, t)| t.as_str()).unwrap_or("")
    }

    pub fn select_problem(&mut self, id: String) {
        self.state.current_problem = Some(id.clone());
        self.save_state();
        self.reload_cases();
        // 换题时清空旧路径并重新探测（路径属于题目目录）
        self.stress.gen_path.clear();
        self.stress.oracle_path.clear();
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
                self.cases.cases =
                    cases.into_iter().map(|c| (c.name, c.input, c.expected.unwrap_or_default())).collect();
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
        let name = if let Some(n) = &self.cases.editing { n.clone() } else { pd.next_case_name() };
        match pd.write_case(&name, input, Some(output), shuati_core::CaseOrigin::Sample) {
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
        if self.stress.gen_path.is_empty()
            && let Some(p) = shuati_judge::find_script(&gen_dir, &["gen"])
        {
            self.stress.gen_path = p.display().to_string();
        }
        if self.stress.oracle_path.is_empty()
            && let Some(p) = shuati_judge::find_script(&sol_dir, &["brute", "oracle", "std"])
        {
            self.stress.oracle_path = p.display().to_string();
        }
    }

    /// 用户解路径：watch 文件 > 题目 solutions/ 下的 main.*
    pub fn user_solution(&self) -> Option<PathBuf> {
        self.state.watch_file.clone().or_else(|| self.current_problem().and_then(|pd| pd.find_solution()))
    }

    /// 设置被监视文件（拖入或恢复）。识别源文件头部的 `// shuati:` 绑定注释：
    /// 绑定的题目存在时自动切换过去，防"拿上个题的代码测当前题"。
    pub fn set_watch_file(&mut self, path: PathBuf) {
        match shuati_judge::watch_file(path.clone()) {
            Ok((w, rx)) => {
                self._watcher = Some(w);
                self.watch_rx = Some(rx);
                self.state.watch_file = Some(path.clone());
                // shuati: 绑定注释 → 切到绑定的题目
                if let Some(bound) = shuati_judge::binding_comment(&path) {
                    let bid = bound.to_string();
                    if self.state.current_problem.as_deref() != Some(bid.as_str())
                        && self.problems.iter().any(|p| p.meta.id.to_string() == bid)
                    {
                        self.lib_msg = format!("源文件绑定 {bid}（shuati: 注释），已切换题目");
                        self.select_problem(bid);
                    }
                }
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
        let (Some(pd), Some(src)) = (self.current_problem().map(|p| p.dir.clone()), self.user_solution()) else {
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

        self.bridge.request(Task::RunTest { problem_dir: pd, problem_id, src, limits, checker, compiler_cfg, cancel });
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
        let seed =
            self.stress.seed_input.trim().parse::<u64>().unwrap_or((shuati_store::now_ts() as u64) % 1_000_000 + 1);

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

    /// 打开题目设置编辑器（预填当前值）
    pub fn open_problem_editor(&mut self) {
        let Some(pd) = self.current_problem() else { return };
        let (title, cpu, mem, stmt, checker) = (
            pd.meta.title.clone(),
            pd.meta.limits.cpu_ms,
            pd.meta.limits.memory_mb,
            pd.read_statement().unwrap_or_default(),
            pd.meta.checker.clone(),
        );
        self.prob_edit.title = title;
        self.prob_edit.cpu_ms = cpu.to_string();
        self.prob_edit.mem_mb = mem.to_string();
        self.prob_edit.statement = stmt;
        let (ck, eps) = match &checker {
            shuati_core::CheckerSpec::Tokens => ("tokens", String::new()),
            shuati_core::CheckerSpec::Exact => ("exact", String::new()),
            shuati_core::CheckerSpec::Float { eps } => ("float", eps.to_string()),
            shuati_core::CheckerSpec::Spj { .. } => ("spj", String::new()),
        };
        self.prob_edit.checker = ck.to_string();
        self.prob_edit.eps = eps;
        self.prob_edit.edit_statement = false;
        self.prob_edit.msg.clear();
        self.prob_edit.show = true;
    }

    /// 保存题目设置
    pub fn save_problem_editor(&mut self) {
        let Some(pd) = self.current_problem() else { return };
        let mut meta = pd.meta.clone();
        meta.title = self.prob_edit.title.trim().to_string();
        if let Ok(v) = self.prob_edit.cpu_ms.trim().parse::<u64>() {
            meta.limits.cpu_ms = v;
            meta.limits.wall_ms = (v * 3).max(2000);
        }
        if let Ok(v) = self.prob_edit.mem_mb.trim().parse::<u64>() {
            meta.limits.memory_mb = v;
        }
        meta.checker = match self.prob_edit.checker.as_str() {
            "exact" => shuati_core::CheckerSpec::Exact,
            "float" => shuati_core::CheckerSpec::Float { eps: self.prob_edit.eps.trim().parse().unwrap_or(1e-6) },
            _ => shuati_core::CheckerSpec::Tokens,
        };
        let dir = pd.dir.clone();
        let pd2 = match store::ProblemDir::open(&dir) {
            Ok(p) => p,
            Err(e) => {
                self.prob_edit.msg = format!("打开失败: {e}");
                return;
            }
        };
        let mut pd2 = pd2;
        pd2.meta = meta;
        if let Err(e) = pd2.save_meta() {
            self.prob_edit.msg = format!("保存失败: {e}");
            return;
        }
        if self.prob_edit.edit_statement
            && let Err(e) = pd2.write_statement(&self.prob_edit.statement)
        {
            self.prob_edit.msg = format!("题面保存失败: {e}");
            return;
        }
        self.stmt_cache = None; // 题面可能刚被编辑器改过
        self.reload_problems();
        self.prob_edit.show = false;
    }

    /// 删除题目（整个目录）
    pub fn delete_problem(&mut self, id: &str) {
        // id 形如 "luogu:P1001"，目录名 "luogu__P1001"
        let dir = self.ws.problems_dir().join(id.replace(':', "__"));
        match std::fs::remove_dir_all(&dir) {
            Ok(()) => {
                if self.state.current_problem.as_deref() == Some(id) {
                    self.state.current_problem = None;
                }
                self.state.last_verdicts.remove(id);
                self.save_state();
                self.reload_problems();
                self.reload_cases();
                self.lib_msg = format!("已删除 {id}");
            }
            Err(e) => self.lib_msg = format!("删除失败: {e}"),
        }
    }

    /// 触发批量造例
    pub fn trigger_gen_cases(&mut self) {
        let Some(pd) = self.current_problem() else {
            self.stress.msg = "先选题目".into();
            return;
        };
        let problem_dir = pd.dir.clone();
        let limits = pd.meta.limits;
        if self.stress.gen_path.is_empty() || self.stress.oracle_path.is_empty() {
            self.autodetect_stress_paths();
        }
        let (gp, op) = (self.stress.gen_path.trim(), self.stress.oracle_path.trim());
        if gp.is_empty() || op.is_empty() {
            self.stress.msg = "需要生成器和标程路径".into();
            return;
        }
        let mk = |p: &str| -> Option<shuati_core::SourceFile> {
            let path = PathBuf::from(p);
            let lang = shuati_core::Lang::from_path(&path)?;
            Some(shuati_core::SourceFile { path, lang })
        };
        let (Some(g), Some(o)) = (mk(gp), mk(op)) else {
            self.stress.msg = "文件类型不支持（.cpp/.py）".into();
            return;
        };
        let count = self.stress.gen_count_input.trim().parse::<u32>().unwrap_or(20).max(1);
        self.stress.genning = true;
        self.stress.msg = format!("生成 {count} 组用例中…");
        self.bridge.request(Task::GenCases {
            problem_dir,
            gen_src: g,
            oracle_src: o,
            count,
            seed_start: (shuati_store::now_ts() as u64) % 1_000_000 + 1,
            limits,
            cpp_cfg: self.cfg.compiler_cpp.clone(),
            py_cfg: self.cfg.compiler_python.clone(),
        });
    }

    /// 把某条用例送进快速运行页（调试单组数据）
    pub fn send_case_to_quick(&mut self, input: String, expected: String) {
        self.quick.input = input;
        self.quick.show_expected = !expected.trim().is_empty();
        self.quick.expected = expected;
        self.tab = Tab::Quick;
        // 有源文件就直接跑
        if self.state.watch_file.is_some() && !self.quick.running {
            self.trigger_quick_run();
        }
    }

    /// 快速运行：编译当前源文件，贴入输入跑一次
    pub fn trigger_quick_run(&mut self) {
        let Some(src_path) = self.state.watch_file.clone() else {
            self.quick.msg = "先拖入或选择源文件（.cpp/.py）".into();
            return;
        };
        let lang = match Lang::from_path(&src_path) {
            Some(l) => l,
            None => {
                self.quick.msg = "文件类型不支持（.cpp/.py）".into();
                return;
            }
        };
        let compiler_cfg = match lang {
            Lang::Cpp => self.cfg.compiler_cpp.clone(),
            Lang::Python => self.cfg.compiler_python.clone(),
        };
        self.quick.running = true;
        self.quick.result = None;
        self.quick.msg = "编译运行中…".into();
        let limits = self.current_problem().map(|p| p.meta.limits).unwrap_or(shuati_core::Limits {
            cpu_ms: 5000,
            wall_ms: 15000,
            memory_mb: 512,
            output_mb: 16,
        });
        let cancel = Arc::new(AtomicBool::new(false));
        self.bridge.request(Task::QuickRun {
            src: SourceFile { path: src_path, lang },
            input: self.quick.input.clone(),
            limits,
            compiler_cfg,
            cancel,
        });
    }

    /// 把快速运行的输入/输出存为当前题的一个用例
    pub fn save_quick_as_case(&mut self) {
        let Some(pd) = self.current_problem() else {
            self.quick.msg = "先在题库选一道题才能存用例".into();
            return;
        };
        if self.quick.input.trim().is_empty() {
            self.quick.msg = "输入为空".into();
            return;
        }
        // 期望输出：优先用户填的，否则用程序实际输出（造用例的常见用法）
        let expected = if self.quick.expected.trim().is_empty() {
            self.quick.result.as_ref().map(|r| r.stdout.clone()).unwrap_or_default()
        } else {
            self.quick.expected.clone()
        };
        if expected.trim().is_empty() {
            self.quick.msg = "没有期望输出——先跑一次或手填".into();
            return;
        }
        let name = pd.next_case_name();
        match pd.write_case(&name, &self.quick.input, Some(&expected), shuati_core::CaseOrigin::Sample) {
            Ok(()) => {
                self.reload_cases();
                self.quick.msg = format!("已存为用例 {name}");
            }
            Err(e) => self.quick.msg = format!("存用例失败: {e}"),
        }
    }

    /// 实际生效的 AI key（SHUATI_API_KEY > 系统钥匙串 > config.toml）
    pub fn ai_key(&self) -> Option<String> {
        self.cfg.ai.resolved_key()
    }

    /// 测试 AI 连接（输入框优先，空则用已存的 key）
    pub fn trigger_ai_ping(&mut self) {
        let key =
            if self.cfg_api_key.trim().is_empty() { self.ai_key() } else { Some(self.cfg_api_key.trim().to_string()) };
        let Some(key) = key else {
            self.cfg_msg = "先填 API Key".into();
            return;
        };
        self.ai_pinging = true;
        self.cfg_msg = "测试中…".into();
        self.bridge.request(Task::AiPing {
            api_base: self.cfg_api_base.trim().to_string(),
            api_key: key,
            model: self.cfg_model.trim().to_string(),
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
            api_key: self.ai_key().unwrap_or_default(),
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
        // 脚本落 gen/ 和 solutions/ 与自动探测位置一致
        let (gen_dir, oracle_dir) = (pd.gen_dir(), pd.solutions_dir());
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
            api_key: self.ai_key().unwrap_or_default(),
            model: self.cfg.ai.model.clone(),
            statement,
            lang,
            gen_dir,
            oracle_dir,
            cpp_cfg: self.cfg.compiler_cpp.clone(),
            py_cfg: self.cfg.compiler_python.clone(),
        });
    }

    /// 处理后台事件（每帧调用）
    fn drain_events(&mut self, ctx: &egui::Context) {
        // watch 事件 → 自动重测；运行中收到保存 → 排队，跑完补测最新版
        if self.state.watch_enabled {
            let mut saved = false;
            if let Some(rx) = &self.watch_rx {
                while rx.try_recv().is_ok() {
                    saved = true;
                }
            }
            if saved {
                if self.run.running {
                    self.pending_retest = true;
                } else {
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
                    self.state.last_verdicts.insert(report.problem.to_string(), report.verdict.to_string());
                    self.save_state();
                    self.run.report = Some(*report);
                    // 跑的时候又保存过 → 立刻补测最新版本
                    if self.pending_retest && self.state.watch_enabled {
                        self.pending_retest = false;
                        self.trigger_test();
                    }
                }
                Event::Failed(msg) => {
                    self.run.running = false;
                    self.run.compiling = false;
                    self.run.error = Some(msg);
                    if self.pending_retest && self.state.watch_enabled {
                        self.pending_retest = false;
                        self.trigger_test();
                    }
                }
                Event::StressFailed(msg) => {
                    self.stress.running = false;
                    self.stress.msg = format!("失败: {msg}");
                }
                Event::GenFailed(msg) => {
                    self.stress.genning = false;
                    self.stress.msg = format!("生成失败: {msg}");
                }
                Event::FetchedOk(id) => {
                    self.fetching = false;
                    self.reload_problems();
                    self.select_problem(id.clone());
                    let n = self.current_problem().and_then(|p| p.load_cases().ok()).map(|c| c.len()).unwrap_or(0);
                    self.lib_msg = if n > 0 {
                        format!("已拉取 {id} · {n} 组样例")
                    } else {
                        format!("已拉取 {id}，但没抓到样例——评测页下方可手动粘贴")
                    };
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
                Event::AiDelta(d) => {
                    // 首个增量到达：清掉"思考中"占位，之后逐段追加
                    if !self.ai.streaming {
                        self.ai.text.clear();
                        self.ai.streaming = true;
                    }
                    self.ai.text.push_str(&d);
                    self.ai.show = true;
                }
                Event::AiText(t) => {
                    self.ai.busy = false;
                    self.ai.streaming = false;
                    self.ai.text = t;
                    self.ai.show = true;
                }
                Event::ScriptsReady(g, o) => {
                    self.ai.busy = false;
                    self.ai.text =
                        format!("脚本已生成并验证通过:\n  gen    = {}\n  oracle = {}", g.display(), o.display());
                    self.ai.show = true;
                    self.stress.gen_path = g.display().to_string();
                    self.stress.oracle_path = o.display().to_string();
                }
                Event::GenDone(n, failed) => {
                    self.stress.genning = false;
                    self.stress.msg = if failed.is_empty() {
                        format!("已生成 {n} 组用例 → 评测页可见")
                    } else {
                        format!("已生成 {n} 组（{} 个 seed 失败: {}）", failed.len(), failed.join("; "))
                    };
                    self.reload_cases();
                }
                Event::AiPong(Ok(s)) => {
                    self.ai_pinging = false;
                    self.cfg_msg = format!("连接正常: {s}");
                }
                Event::AiPong(Err(e)) => {
                    self.ai_pinging = false;
                    self.cfg_msg = format!("连接失败: {e}");
                }
                Event::QuickDone(qr) => {
                    self.quick.running = false;
                    self.quick.result = Some(*qr);
                    self.quick.msg.clear();
                }
                Event::QuickFailed(e) => {
                    self.quick.running = false;
                    self.quick.result = None;
                    self.quick.msg = e;
                }
            }
        }
        if self.run.running
            || self.stress.running
            || self.state.watch_enabled
            || self.fetching
            || self.ai.busy
            || self.stress.genning
            || self.ai_pinging
            || self.quick.running
        {
            ctx.request_repaint_after(std::time::Duration::from_millis(100));
        }
    }
}

impl eframe::App for ShuatiApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.drain_events(ctx);

        // F5 = 运行（Dev-C++ 的 F9 肌肉记忆）：按当前页派发
        if ctx.input(|i| i.key_pressed(egui::Key::F5)) {
            match self.tab {
                Tab::Quick => {
                    if !self.quick.running {
                        self.trigger_quick_run();
                    }
                }
                Tab::Test => {
                    if !self.run.running {
                        self.trigger_test();
                    }
                }
                Tab::Stress if !self.stress.running => {
                    self.trigger_stress();
                }
                _ => {}
            }
        }

        // 拖拽处理：.cpp/.py → 源文件；.in → 追加用例
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        for f in &dropped {
            let Some(p) = &f.path else { continue };
            if shuati_core::Lang::from_path(p).is_some() {
                self.set_watch_file(p.clone());
                // 拖入即跑：在快速运行页或没选题时 → 直接单次运行；
                // 有题在评测页 → 走完整评测
                if self.tab == Tab::Quick || self.current_problem().is_none() {
                    if !self.quick.running {
                        self.trigger_quick_run();
                    }
                } else if !self.run.running {
                    self.trigger_test();
                }
            } else if p.extension().is_some_and(|e| e == "in") {
                // 拖入 .in → 加到用例（无 .out 则按无答案存）
                if let (Ok(input), Some(pd)) =
                    (std::fs::read_to_string(p), self.current_problem().map(|x| x.dir.clone()))
                {
                    let out_p = p.with_extension("out");
                    let output = std::fs::read_to_string(&out_p).ok();
                    let pd2 = shuati_store::ProblemDir::open(&pd);
                    if let Ok(pd2) = pd2 {
                        let name = pd2.next_case_name();
                        let _ = pd2.write_case(&name, &input, output.as_deref(), shuati_core::CaseOrigin::Sample);
                        self.reload_cases();
                        self.cases.msg = if output.is_none() {
                            format!("拖入用例 {name}（无 .out 文件，按无答案运行）")
                        } else {
                            format!("拖入用例 {name}")
                        };
                    }
                }
            }
        }

        crate::views::render(self, ctx);
    }
}
