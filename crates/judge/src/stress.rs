//! 对拍引擎：seed 驱动的差分测试。
//! 循环: gen(seed) → input → (user ∥ oracle) → checker 比对
//! mismatch 即 hack，三方归责：gen 坏/标程坏/找到反例。

use crate::{check_async, compile_source, cleanup_program, run_program, Io, Program, Result, RunStatus, Toolchain};
use shuati_core::{CheckerSpec, Lang, Limits, SourceFile, Verdict};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::Arc;

/// 对拍参数
pub struct StressParams<'a> {
    /// 生成器源文件（约定：argv[1] 收 seed）
    pub gen_src: &'a SourceFile,
    /// 用户解源文件
    pub user_src: &'a SourceFile,
    /// 标程源文件
    pub oracle_src: &'a SourceFile,
    /// 轮数
    pub rounds: u32,
    /// 起始 seed（默认 1）；replay 时传固定 seed + rounds=1
    pub seed_start: u64,
    /// 并行轮数
    pub jobs: usize,
    /// user/oracle 运行限制
    pub limits_run: &'a Limits,
    /// gen 运行限制（通常更宽松）
    pub limits_gen: &'a Limits,
    pub checker: &'a CheckerSpec,
    /// 发现 hack 即停？
    pub stop_on_fail: bool,
    /// 进度回调：每轮结束触发
    pub progress: Option<Arc<dyn Fn(StressEvent) + Send + Sync>>,
    pub cancel: Option<Arc<AtomicBool>>,
}

/// 单轮结果
#[derive(Debug)]
pub enum StressEvent {
    /// 一致
    Match { seed: u64, round: u32, cpu_ms: u64 },
    /// 找到反例
    Hack { seed: u64, round: u32, case: Box<HackCase> },
    /// 生成器异常
    GenFail { seed: u64, round: u32, msg: String },
    /// 标程异常
    OracleFail { seed: u64, round: u32, msg: String },
}

#[derive(Debug, Clone)]
pub struct HackCase {
    pub seed: u64,
    pub input: String,
    pub user_out: String,
    pub oracle_out: String,
    /// hack 类型：wa / user_re / user_tle
    pub kind: String,
    pub detail: String,
}

/// 汇总报告
#[derive(Debug, Default, Clone)]
pub struct StressReport {
    pub rounds_done: u32,
    pub matched: u32,
    pub hacks: Vec<HackCase>,
    pub gen_failures: u32,
    pub oracle_failures: u32,
    pub stopped_early: bool,
}

impl StressReport {
    pub fn is_clean(&self) -> bool {
        self.hacks.is_empty() && self.gen_failures == 0 && self.oracle_failures == 0
    }
}

/// 对拍引擎：编译三方一次，跑 N 轮
pub struct StressEngine {
    pub gen_tc: Toolchain,
    pub user_tc: Toolchain,
    pub oracle_tc: Toolchain,
}

impl StressEngine {
    /// 按三个源文件语言建 toolchain
    pub fn new(p: &StressParams<'_>, cfg_cpp: &str, cfg_py: &str) -> Result<Self> {
        Ok(Self {
            gen_tc: Toolchain::detect(p.gen_src.lang, pick(p.gen_src.lang, cfg_cpp, cfg_py))?,
            user_tc: Toolchain::detect(p.user_src.lang, pick(p.user_src.lang, cfg_cpp, cfg_py))?,
            oracle_tc: Toolchain::detect(p.oracle_src.lang, pick(p.oracle_src.lang, cfg_cpp, cfg_py))?,
        })
    }

    pub async fn run(&self, p: &StressParams<'_>) -> Result<StressReport> {
        // 编译三方（并行）
        let (gen_r, user_r, oracle_r) = tokio::join!(
            compile_source(p.gen_src, &self.gen_tc),
            compile_source(p.user_src, &self.user_tc),
            compile_source(p.oracle_src, &self.oracle_tc),
        );
        let gen_prog = gen_r?;
        let user_prog = user_r?;
        let oracle_prog = oracle_r?;

        let report = self.run_rounds(p, &gen_prog, &user_prog, &oracle_prog).await;

        cleanup_program(&gen_prog);
        cleanup_program(&user_prog);
        cleanup_program(&oracle_prog);
        report
    }

    async fn run_rounds(
        &self,
        p: &StressParams<'_>,
        gen_prog: &Program,
        user_prog: &Program,
        oracle_prog: &Program,
    ) -> Result<StressReport> {
        let next_round = Arc::new(AtomicU32::new(0));
        let hack_count = Arc::new(AtomicU32::new(0));
        let report = Arc::new(tokio::sync::Mutex::new(StressReport::default()));

        let jobs = p.jobs.max(1) as usize;
        let mut handles = Vec::new();

        for _ in 0..jobs {
            let next_round = next_round.clone();
            let hack_count = hack_count.clone();
            let report = report.clone();
            let gen_prog = gen_prog.clone();
            let user_prog = user_prog.clone();
            let oracle_prog = oracle_prog.clone();
            let progress = p.progress.clone();
            let cancel = p.cancel.clone();
            let stop_on_fail = p.stop_on_fail;
            let rounds = p.rounds;
            let seed_start = p.seed_start;
            let limits_run = *p.limits_run;
            let limits_gen = *p.limits_gen;
            let checker = p.checker.clone();

            handles.push(tokio::spawn(async move {
                loop {
                    if cancel.as_ref().is_some_and(|c| c.load(Ordering::Relaxed)) {
                        break;
                    }
                    if stop_on_fail && hack_count.load(Ordering::Relaxed) > 0 {
                        break;
                    }
                    let r = next_round.fetch_add(1, Ordering::Relaxed);
                    if r >= rounds {
                        break;
                    }
                    let seed = seed_start + r as u64;
                    let outcome = run_one_round(
                        seed, &gen_prog, &user_prog, &oracle_prog,
                        &limits_run, &limits_gen, &checker,
                    )
                    .await;

                    {
                        let mut rep = report.lock().await;
                        rep.rounds_done += 1;
                        match &outcome {
                            StressEvent::Match { .. } => rep.matched += 1,
                            StressEvent::Hack { case, .. } => {
                                rep.hacks.push((**case).clone());
                                hack_count.fetch_add(1, Ordering::Relaxed);
                            }
                            StressEvent::GenFail { .. } => rep.gen_failures += 1,
                            StressEvent::OracleFail { .. } => rep.oracle_failures += 1,
                        }
                    }
                    if let Some(cb) = &progress {
                        cb(outcome);
                    }
                }
            }));
        }
        for h in handles {
            let _ = h.await;
        }

        let mut rep = report.lock().await.clone();
        rep.stopped_early = rep.rounds_done < p.rounds;
        rep.hacks.sort_by_key(|h| h.seed);
        Ok(rep)
    }
}

fn pick<'a>(lang: Lang, cpp: &'a str, py: &'a str) -> &'a str {
    match lang {
        Lang::Cpp => cpp,
        Lang::Python => py,
    }
}

/// Program 追加参数（gen 用 argv[1]=seed）
fn with_extra_arg(prog: &Program, arg: &str) -> Program {
    let mut p = prog.clone();
    p.argv.push(arg.to_string());
    p
}

/// 单轮：gen(seed) → input → user & oracle → 比对
async fn run_one_round(
    seed: u64,
    gen_prog: &Program,
    user_prog: &Program,
    oracle_prog: &Program,
    limits_run: &Limits,
    limits_gen: &Limits,
    checker: &CheckerSpec,
) -> StressEvent {
    let round = (seed & 0xFFFF_FFFF) as u32;
    let tmp = match tempfile::Builder::new().prefix("shuati_stress_").tempdir() {
        Ok(t) => t,
        Err(e) => {
            return StressEvent::GenFail { seed, round, msg: format!("tempdir: {e}") };
        }
    };
    let td = tmp.path().to_path_buf();

    // 1. 生成器 → input（stdout 即测试输入）
    let gen_out = td.join("gen.out");
    let gen_err = td.join("gen.err");
    let gen_io = Io { stdin: None, stdout: gen_out.clone(), stderr: gen_err.clone() };
    let gen_p = with_extra_arg(gen_prog, &seed.to_string());
    let g = match run_program(&gen_p, &gen_io, limits_gen).await {
        Ok(o) => o,
        Err(e) => return StressEvent::GenFail { seed, round, msg: e.to_string() },
    };
    if g.status != RunStatus::Ok {
        let e = std::fs::read_to_string(&gen_err).unwrap_or_default();
        let e = if e.is_empty() { format!("gen exit {:?}", g.status) } else { e };
        return StressEvent::GenFail { seed, round, msg: truncate(&e, 400) };
    }
    let input = std::fs::read_to_string(&gen_out).unwrap_or_default();
    if input.trim().is_empty() {
        return StressEvent::GenFail { seed, round, msg: "生成器输出为空".into() };
    }

    // 2. user & oracle 并发跑同一 input
    let in_f = td.join("case.in");
    if let Err(e) = std::fs::write(&in_f, &input) {
        return StressEvent::GenFail { seed, round, msg: format!("write input: {e}") };
    }
    let user_io = Io {
        stdin: Some(in_f.clone()),
        stdout: td.join("user.out"),
        stderr: td.join("user.err"),
    };
    let oracle_io = Io {
        stdin: Some(in_f),
        stdout: td.join("oracle.out"),
        stderr: td.join("oracle.err"),
    };
    let (u, o) = tokio::join!(
        run_program(user_prog, &user_io, limits_run),
        run_program(oracle_prog, &oracle_io, limits_run)
    );
    let u = match u {
        Ok(x) => x,
        Err(e) => return StressEvent::GenFail { seed, round, msg: format!("runner: {e}") },
    };
    let o = match o {
        Ok(x) => x,
        Err(e) => return StressEvent::GenFail { seed, round, msg: format!("runner: {e}") },
    };

    // 3. 归责
    let user_out = std::fs::read_to_string(&user_io.stdout).unwrap_or_default();
    let oracle_out = std::fs::read_to_string(&oracle_io.stdout).unwrap_or_default();

    if o.status != RunStatus::Ok {
        let e = std::fs::read_to_string(&oracle_io.stderr).unwrap_or_default();
        return StressEvent::OracleFail {
            seed, round,
            msg: format!("标程 {:?}: {}", o.status, truncate(&e, 300)),
        };
    }
    if u.status != RunStatus::Ok {
        // 用户程序挂也是"反例"（不是答案不等，是程序错了）
        let e = std::fs::read_to_string(&user_io.stderr).unwrap_or_default();
        let kind = match u.status {
            RunStatus::TimeLimit => "user_tle",
            RunStatus::MemoryLimit => "user_mle",
            _ => "user_re",
        };
        return StressEvent::Hack {
            seed, round,
            case: Box::new(HackCase {
                seed, input, user_out,
                oracle_out,
                kind: kind.into(),
                detail: if e.is_empty() { format!("{:?}", u.status) } else { truncate(&e, 300) },
            }),
        };
    }

    // 4. 比对
    let ctx = shuati_core::CheckCtx { input: &input, actual: &user_out, expected: &oracle_out };
    match check_async(checker, &ctx, &td).await {
        Ok(Verdict::Ac) => StressEvent::Match { seed, round, cpu_ms: u.cpu_ms },
        Ok(_) => StressEvent::Hack {
            seed, round,
            case: Box::new(HackCase {
                seed, input, user_out, oracle_out,
                kind: "wa".into(),
                detail: "output differs".into(),
            }),
        },
        Err(e) => StressEvent::OracleFail { seed, round, msg: format!("checker: {e}") },
    }
}

fn truncate(s: &str, n: usize) -> String {
    if s.len() <= n { s.to_string() } else { format!("{}…", &s[..n]) }
}

/// 批量测试集生成：gen(seed) → oracle → 返回用例对（调用方落盘）
pub async fn gen_cases(
    gen_prog: &Program,
    oracle_prog: &Program,
    seeds: impl Iterator<Item = u64>,
    limits_run: &Limits,
    limits_gen: &Limits,
) -> Vec<(u64, String, String)> {
    let mut out = Vec::new();
    for seed in seeds {
        let tmp = match tempfile::tempdir() {
            Ok(t) => t,
            Err(_) => continue,
        };
        let td = tmp.path().to_path_buf();
        let gen_out = td.join("g.out");
        let gen_io = Io { stdin: None, stdout: gen_out.clone(), stderr: td.join("g.err") };
        let g = run_program(&with_extra_arg(gen_prog, &seed.to_string()), &gen_io, limits_gen).await;
        let Ok(g) = g else { continue };
        if g.status != RunStatus::Ok { continue; }
        let input = std::fs::read_to_string(&gen_out).unwrap_or_default();
        if input.trim().is_empty() { continue; }
        let in_f = td.join("c.in");
        if std::fs::write(&in_f, &input).is_err() { continue; }
        let io = Io { stdin: Some(in_f), stdout: td.join("o.out"), stderr: td.join("o.err") };
        let o = run_program(oracle_prog, &io, limits_run).await;
        let Ok(o) = o else { continue };
        if o.status != RunStatus::Ok { continue; }
        let output = std::fs::read_to_string(&io.stdout).unwrap_or_default();
        out.push((seed, input, output));
    }
    out
}

/// 在题目目录里找 gen/brute 默认文件
pub fn find_script(dir: &Path, names: &[&str]) -> Option<PathBuf> {
    for n in names {
        for ext in ["cpp", "py"] {
            let p = dir.join(format!("{n}.{ext}"));
            if p.exists() {
                return Some(p);
            }
        }
    }
    None
}
