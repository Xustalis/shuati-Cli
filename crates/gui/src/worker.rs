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
    /// 评测任务失败（编译/环境）
    Failed(String),
    /// 对拍任务失败——错误显示在对拍页，不动评测页状态
    StressFailed(String),
    /// 批量造例失败——复位 genning 并提示
    GenFailed(String),
    /// 拉题完成：(题目id, 抓到样例数, 新写入数)
    FetchedOk(String, usize, usize),
    /// 拉题失败
    FetchFailed(String),
    /// 对拍：单轮进度（done, matched, hacks, gen_fail, oracle_fail）
    StressTick(u32, u32, u32, u32, u32),
    /// 对拍：发现反例
    StressHack(Box<judge::HackCase>),
    /// 对拍完成
    StressDone(Box<judge::StressReport>),
    /// AI 文本增量（SSE 流式，逐段追加到面板）
    AiDelta(String),
    /// AI 文本响应（hint/explain 的完整文本/错误）
    AiText(String),
    /// AI 脚本就绪：gen 路径、oracle 路径
    ScriptsReady(PathBuf, PathBuf),
    /// 批量造例完成（生成组数, 失败原因列表）
    GenDone(usize, Vec<String>),
    /// AI 连接测试结果
    AiPong(Result<String, String>),
    /// 快速运行完成
    QuickDone(Box<QuickResult>),
    /// 快速运行失败（编译/环境）
    QuickFailed(String),
}

/// 快速运行结果：单次"跑一下看输出"
#[derive(Debug)]
pub struct QuickResult {
    pub stdout: String,
    pub stderr: String,
    /// None = 正常退出；Some = TLE/MLE/RE/SE
    pub status: Option<Verdict>,
    pub cpu_ms: u64,
    pub mem_kb: u64,
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
    /// 拉题
    FetchProblem { url: String },
    /// 对拍
    RunStress {
        problem_dir: PathBuf,
        gen_src: SourceFile,
        user_src: SourceFile,
        oracle_src: SourceFile,
        rounds: u32,
        seed_start: u64,
        jobs: usize,
        limits: shuati_core::Limits,
        checker: shuati_core::CheckerSpec,
        cpp_cfg: String,
        py_cfg: String,
        stop_on_fail: bool,
        cancel: Arc<AtomicBool>,
    },
    /// AI 提示/诊断
    AiAsk {
        api_base: String,
        api_key: String,
        model: String,
        statement: String,
        code: String,
        verdict_info: String,
        explain: bool,
    },
    /// AI 生成对拍脚本（自动 dry-run 验证）
    AiScripts {
        api_base: String,
        api_key: String,
        model: String,
        statement: String,
        lang: shuati_core::Lang,
        /// gen 落点（题目 gen/ 目录）
        gen_dir: PathBuf,
        /// oracle 落点（题目 solutions/ 目录）
        oracle_dir: PathBuf,
        cpp_cfg: String,
        py_cfg: String,
    },
    /// 快速运行：贴输入直接跑，不需要题目/用例
    QuickRun {
        src: SourceFile,
        input: String,
        limits: shuati_core::Limits,
        compiler_cfg: String,
        cancel: Arc<AtomicBool>,
    },
    /// AI 连接测试（最小请求）
    AiPing { api_base: String, api_key: String, model: String },
    /// 批量生成用例 → tests/
    GenCases {
        problem_dir: PathBuf,
        gen_src: SourceFile,
        oracle_src: SourceFile,
        count: u32,
        seed_start: u64,
        limits: shuati_core::Limits,
        cpp_cfg: String,
        py_cfg: String,
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
    pub fn spawn(ws_root: PathBuf) -> Self {
        let (task_tx, mut task_rx) = mpsc::unbounded_channel::<Task>();
        let (ev_tx, ev_rx) = std::sync::mpsc::channel::<Event>();
        let cancel_slot: Arc<Mutex<Option<Arc<AtomicBool>>>> = Arc::new(Mutex::new(None));

        let slot = cancel_slot.clone();
        let root = ws_root.clone();
        std::thread::spawn(move || {
            let rt = tokio::runtime::Runtime::new().expect("tokio runtime");
            rt.block_on(async move {
                // 编译缓存：源文件没变就跳过 g++，重复运行秒出
                let cache_dir = root.join(".cache").join("build");
                while let Some(task) = task_rx.recv().await {
                    match task {
                        Task::RunTest { problem_dir, problem_id, src, limits, checker, compiler_cfg, cancel } => {
                            *slot.lock().unwrap() = Some(cancel.clone());
                            let ev = ev_tx.clone();
                            let cd = cache_dir.clone();
                            tokio::spawn(async move {
                                let job = TestJob {
                                    problem_dir,
                                    problem_id,
                                    src,
                                    limits,
                                    checker,
                                    compiler_cfg,
                                    cache_dir: cd,
                                };
                                run_test(job, cancel, ev).await;
                            });
                        }
                        Task::FetchProblem { url } => {
                            let ev = ev_tx.clone();
                            let root = root.clone();
                            tokio::spawn(async move {
                                let ws = match store::Workspace::open(&root) {
                                    Ok(w) => w,
                                    Err(e) => {
                                        let _ = ev.send(Event::FetchFailed(format!("工作区: {e}")));
                                        return;
                                    }
                                };
                                match shuati_fetch::fetch_to_dir(&url, &ws).await {
                                    Ok(out) => {
                                        let _ = ev.send(Event::FetchedOk(
                                            out.pd.meta.id.to_string(),
                                            out.samples,
                                            out.added,
                                        ));
                                    }
                                    Err(e) => {
                                        let _ = ev.send(Event::FetchFailed(e.to_string()));
                                    }
                                }
                            });
                        }
                        Task::RunStress {
                            problem_dir,
                            gen_src,
                            user_src,
                            oracle_src,
                            rounds,
                            seed_start,
                            jobs,
                            limits,
                            checker,
                            cpp_cfg,
                            py_cfg,
                            stop_on_fail,
                            cancel,
                        } => {
                            *slot.lock().unwrap() = Some(cancel.clone());
                            let ev = ev_tx.clone();
                            let cd = cache_dir.clone();
                            tokio::spawn(async move {
                                run_stress(
                                    problem_dir,
                                    gen_src,
                                    user_src,
                                    oracle_src,
                                    rounds,
                                    seed_start,
                                    jobs,
                                    limits,
                                    checker,
                                    cpp_cfg,
                                    py_cfg,
                                    stop_on_fail,
                                    cancel,
                                    ev,
                                    cd,
                                )
                                .await;
                            });
                        }
                        Task::AiAsk { api_base, api_key, model, statement, code, verdict_info, explain } => {
                            let ev = ev_tx.clone();
                            tokio::spawn(async move {
                                let r = shuati_ai::AiClient::new(&api_base, &api_key, &model);
                                match r {
                                    Err(e) => {
                                        let _ = ev.send(Event::AiText(format!("AI 未配置: {e}")));
                                    }
                                    Ok(client) => {
                                        // SSE 流式：delta 实时进面板；流挂掉才退非流式整段
                                        let ev2 = ev.clone();
                                        let mut on_delta = move |d: &str| {
                                            let _ = ev2.send(Event::AiDelta(d.to_string()));
                                        };
                                        let out = if explain {
                                            client
                                                .explain_stream(&statement, &code, &verdict_info, "", &mut on_delta)
                                                .await
                                        } else {
                                            client.hint_stream(&statement, &code, &verdict_info, &mut on_delta).await
                                        };
                                        let out = match out {
                                            Ok(t) => Ok(t),
                                            Err(_) => {
                                                if explain {
                                                    client.explain(&statement, &code, &verdict_info, "").await
                                                } else {
                                                    client.hint(&statement, &code, &verdict_info).await
                                                }
                                            }
                                        };
                                        match out {
                                            Ok(t) => {
                                                let _ = ev.send(Event::AiText(t));
                                            }
                                            Err(e) => {
                                                let _ = ev.send(Event::AiText(format!("AI 错误: {e}")));
                                            }
                                        }
                                    }
                                }
                            });
                        }
                        Task::QuickRun { src, input, limits, compiler_cfg, cancel } => {
                            *slot.lock().unwrap() = Some(cancel.clone());
                            let ev = ev_tx.clone();
                            let cd = cache_dir.clone();
                            tokio::spawn(async move {
                                match quick_run(src, input, limits, compiler_cfg, cancel, cd).await {
                                    Ok(qr) => {
                                        let _ = ev.send(Event::QuickDone(Box::new(qr)));
                                    }
                                    Err(e) => {
                                        let _ = ev.send(Event::QuickFailed(e));
                                    }
                                }
                            });
                        }
                        Task::AiPing { api_base, api_key, model } => {
                            let ev = ev_tx.clone();
                            tokio::spawn(async move {
                                let out = async {
                                    let client = shuati_ai::AiClient::new(&api_base, &api_key, &model)
                                        .map_err(|e| e.to_string())?;
                                    client
                                        .chat("You are a connectivity probe. Reply with exactly: ok", "ping")
                                        .await
                                        .map(|r| format!("{} · {}", model, r.chars().take(60).collect::<String>()))
                                        .map_err(|e| e.to_string())
                                }
                                .await;
                                let _ = ev.send(Event::AiPong(out));
                            });
                        }
                        Task::GenCases {
                            problem_dir,
                            gen_src,
                            oracle_src,
                            count,
                            seed_start,
                            limits,
                            cpp_cfg,
                            py_cfg,
                        } => {
                            let ev = ev_tx.clone();
                            let cd = cache_dir.clone();
                            tokio::spawn(async move {
                                run_gen_cases(
                                    problem_dir,
                                    gen_src,
                                    oracle_src,
                                    count,
                                    seed_start,
                                    limits,
                                    cpp_cfg,
                                    py_cfg,
                                    ev,
                                    cd,
                                )
                                .await;
                            });
                        }
                        Task::AiScripts {
                            api_base,
                            api_key,
                            model,
                            statement,
                            lang,
                            gen_dir,
                            oracle_dir,
                            cpp_cfg,
                            py_cfg,
                        } => {
                            let ev = ev_tx.clone();
                            tokio::spawn(async move {
                                let r = shuati_ai::AiClient::new(&api_base, &api_key, &model);
                                match r {
                                    Err(e) => {
                                        let _ = ev.send(Event::AiText(format!("AI 未配置: {e}")));
                                    }
                                    Ok(client) => {
                                        let dirs = shuati_ai::ScriptDirs { generator: &gen_dir, oracle: &oracle_dir };
                                        match shuati_ai::produce_scripts(
                                            &client,
                                            &statement,
                                            lang,
                                            dirs,
                                            &cpp_cfg,
                                            &py_cfg,
                                            |_| {},
                                        )
                                        .await
                                        {
                                            Ok((g, o)) => {
                                                let _ = ev.send(Event::ScriptsReady(g, o));
                                            }
                                            Err(e) => {
                                                let _ = ev.send(Event::AiText(format!("脚本生成失败: {e}")));
                                            }
                                        }
                                    }
                                }
                            });
                        }
                    }
                }
            });
        });

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

/// 快速运行：编译一次 → 贴入输入跑一次 → 读回 stdout/stderr
async fn quick_run(
    src: SourceFile,
    input: String,
    limits: shuati_core::Limits,
    compiler_cfg: String,
    cancel: Arc<AtomicBool>,
    cache_dir: PathBuf,
) -> std::result::Result<QuickResult, String> {
    let tc = judge::Toolchain::detect(src.lang, &compiler_cfg).map_err(|e| e.to_string())?;
    let prog = judge::compile_cached(&src, &tc, &cache_dir).await.map_err(|e| format!("编译失败:\n{e}"))?;
    let tmp = tempfile::tempdir().map_err(|e| e.to_string())?;
    let stdin_path = tmp.path().join("stdin.txt");
    std::fs::write(&stdin_path, &input).map_err(|e| e.to_string())?;
    let io = judge::Io {
        stdin: Some(stdin_path),
        stdout: tmp.path().join("stdout.txt"),
        stderr: tmp.path().join("stderr.txt"),
    };
    let out = judge::run_program(&prog, &io, &limits, Some(cancel)).await.map_err(|e| e.to_string());
    judge::cleanup_program(&prog);
    let out = out?;
    let stdout = std::fs::read_to_string(&out.stdout_path).unwrap_or_default();
    let stderr = std::fs::read_to_string(&out.stderr_path).unwrap_or_default();
    let status = match out.status {
        judge::RunStatus::Ok => None,
        judge::RunStatus::TimeLimit => Some(Verdict::Tle),
        judge::RunStatus::MemoryLimit => Some(Verdict::Mle),
        judge::RunStatus::OutputLimit => Some(Verdict::Ole),
        judge::RunStatus::Cancelled => Some(Verdict::Se),
        judge::RunStatus::RuntimeError => Some(Verdict::Re),
        judge::RunStatus::InternalError => Some(Verdict::Se),
    };
    Ok(QuickResult { stdout, stderr, status, cpu_ms: out.cpu_ms, mem_kb: out.peak_mem_kb })
}

/// 一次评测的全部上下文（Task::RunTest 解包后原样打包）
struct TestJob {
    problem_dir: PathBuf,
    problem_id: shuati_core::ProblemId,
    src: PathBuf,
    limits: shuati_core::Limits,
    checker: shuati_core::CheckerSpec,
    compiler_cfg: String,
    cache_dir: PathBuf,
}

async fn run_test(job: TestJob, cancel: Arc<AtomicBool>, ev: std::sync::mpsc::Sender<Event>) {
    let TestJob { problem_dir, problem_id, src, limits, checker, compiler_cfg, cache_dir } = job;
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
            let _ = ev.send(Event::Failed("tests/ 下没有用例——去评测页下方加一个".into()));
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
    let engine = judge::TestEngine::new(tc).with_cache(cache_dir);
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

/// 批量造例：编译 gen+oracle → gen_cases → write_case 落盘
#[allow(clippy::too_many_arguments)]
async fn run_gen_cases(
    problem_dir: PathBuf,
    gen_src: SourceFile,
    oracle_src: SourceFile,
    count: u32,
    seed_start: u64,
    limits: shuati_core::Limits,
    cpp_cfg: String,
    py_cfg: String,
    ev: std::sync::mpsc::Sender<Event>,
    cache_dir: PathBuf,
) {
    let pd = match store::ProblemDir::open(&problem_dir) {
        Ok(p) => p,
        Err(e) => {
            let _ = ev.send(Event::GenFailed(format!("题目目录错误: {e}")));
            return;
        }
    };
    let comp = match gen_src.lang {
        Lang::Cpp => &cpp_cfg,
        Lang::Python => &py_cfg,
    };
    let ocomp = match oracle_src.lang {
        Lang::Cpp => &cpp_cfg,
        Lang::Python => &py_cfg,
    };
    let gen_tc = match judge::Toolchain::detect(gen_src.lang, comp) {
        Ok(t) => t,
        Err(e) => {
            let _ = ev.send(Event::GenFailed(e.to_string()));
            return;
        }
    };
    let oracle_tc = match judge::Toolchain::detect(oracle_src.lang, ocomp) {
        Ok(t) => t,
        Err(e) => {
            let _ = ev.send(Event::GenFailed(e.to_string()));
            return;
        }
    };
    let gen_prog = match judge::compile_cached(&gen_src, &gen_tc, &cache_dir).await {
        Ok(p) => p,
        Err(e) => {
            let _ = ev.send(Event::GenFailed(format!("gen 编译失败: {e}")));
            return;
        }
    };
    let oracle_prog = match judge::compile_cached(&oracle_src, &oracle_tc, &cache_dir).await {
        Ok(p) => p,
        Err(e) => {
            judge::cleanup_program(&gen_prog);
            let _ = ev.send(Event::GenFailed(format!("oracle 编译失败: {e}")));
            return;
        }
    };

    let glim = shuati_core::Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 64 };
    let batch = judge::gen_cases(&gen_prog, &oracle_prog, seed_start..seed_start + count as u64, &limits, &glim).await;
    judge::cleanup_program(&gen_prog);
    judge::cleanup_program(&oracle_prog);

    if batch.cases.is_empty() {
        let first = batch.failed.first().map(|(_, r)| r.clone()).unwrap_or_default();
        let _ = ev.send(Event::GenFailed(format!("没有产出生效例（gen/oracle 可能运行失败）{first}")));
        return;
    }
    let mut n = 0;
    for (_s, input, output) in &batch.cases {
        let name = pd.next_case_name();
        if pd.write_case(&name, input, Some(output), shuati_core::CaseOrigin::Generated).is_ok() {
            n += 1;
        }
    }
    let failed_msgs: Vec<String> = batch.failed.iter().map(|(s, r)| format!("seed {s}: {r}")).collect();
    let _ = ev.send(Event::GenDone(n, failed_msgs));
}

#[allow(clippy::too_many_arguments)]
async fn run_stress(
    problem_dir: PathBuf,
    gen_src: SourceFile,
    user_src: SourceFile,
    oracle_src: SourceFile,
    rounds: u32,
    seed_start: u64,
    jobs: usize,
    limits: shuati_core::Limits,
    checker: shuati_core::CheckerSpec,
    cpp_cfg: String,
    py_cfg: String,
    stop_on_fail: bool,
    cancel: Arc<AtomicBool>,
    ev: std::sync::mpsc::Sender<Event>,
    cache_dir: PathBuf,
) {
    let pd = match store::ProblemDir::open(&problem_dir) {
        Ok(p) => p,
        Err(e) => {
            let _ = ev.send(Event::StressFailed(format!("题目目录错误: {e}")));
            return;
        }
    };

    let counts = Arc::new(Mutex::new((0u32, 0u32, 0u32, 0u32, 0u32)));
    let counts2 = counts.clone();
    let ev2 = ev.clone();
    let progress = Arc::new(move |e: judge::StressEvent| {
        use judge::StressEvent::*;
        let mut c = counts2.lock().unwrap();
        c.0 += 1;
        match &e {
            Match { .. } => c.1 += 1,
            Hack { case, .. } => {
                c.2 += 1;
                let _ = ev2.send(Event::StressHack(case.clone()));
            }
            GenFail { .. } => c.3 += 1,
            OracleFail { .. } => c.4 += 1,
        }
        let _ = ev2.send(Event::StressTick(c.0, c.1, c.2, c.3, c.4));
    });

    let gen_limits = shuati_core::Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 64 };
    let params = judge::StressParams {
        gen_src: &gen_src,
        user_src: &user_src,
        oracle_src: &oracle_src,
        rounds,
        seed_start,
        jobs,
        limits_run: &limits,
        limits_gen: &gen_limits,
        checker: &checker,
        stop_on_fail,
        progress: Some(progress),
        cancel: Some(cancel),
    };

    let engine = match judge::StressEngine::new(&params, &cpp_cfg, &py_cfg).map(|e| e.with_cache(cache_dir.clone())) {
        Ok(e) => e,
        Err(e) => {
            let _ = ev.send(Event::StressFailed(e.to_string()));
            return;
        }
    };
    match engine.run(&params).await {
        Ok(report) => {
            // hack 落盘
            for h in &report.hacks {
                let _ =
                    pd.save_hack(h.seed, &h.input, &h.user_out, &h.oracle_out, &format!("{}: {}", h.kind, h.detail));
            }
            let _ = ev.send(Event::StressDone(Box::new(report)));
        }
        Err(judge::JudgeError::Compile(msg)) => {
            let _ = ev.send(Event::StressFailed(format!("编译失败:\n{msg}")));
        }
        Err(e) => {
            let _ = ev.send(Event::StressFailed(e.to_string()));
        }
    }
}
