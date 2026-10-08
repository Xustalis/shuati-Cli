//! 对拍引擎：seed 驱动的差分测试。
//! 循环: gen(seed) → input → (user ∥ oracle) → checker 比对
//! mismatch 即 hack，三方归责：gen 坏/标程坏/找到反例。

use crate::{Io, Program, Result, RunStatus, Toolchain, check_async, cleanup_program, compile_source, run_program};
use shuati_core::{CheckerSpec, Lang, Limits, SourceFile, Verdict};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};

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
    cache_dir: Option<std::path::PathBuf>,
}

impl StressEngine {
    /// 按三个源文件语言建 toolchain
    pub fn new(p: &StressParams<'_>, cfg_cpp: &str, cfg_py: &str) -> Result<Self> {
        Ok(Self {
            gen_tc: Toolchain::detect(p.gen_src.lang, pick(p.gen_src.lang, cfg_cpp, cfg_py))?,
            user_tc: Toolchain::detect(p.user_src.lang, pick(p.user_src.lang, cfg_cpp, cfg_py))?,
            oracle_tc: Toolchain::detect(p.oracle_src.lang, pick(p.oracle_src.lang, cfg_cpp, cfg_py))?,
            cache_dir: None,
        })
    }

    /// 启用编译缓存
    pub fn with_cache(mut self, dir: std::path::PathBuf) -> Self {
        self.cache_dir = Some(dir);
        self
    }

    pub async fn run(&self, p: &StressParams<'_>) -> Result<StressReport> {
        // 编译三方（并行；有缓存目录时按指纹复用）
        let (gen_r, user_r, oracle_r) = match &self.cache_dir {
            Some(d) => tokio::join!(
                crate::compile_cached(p.gen_src, &self.gen_tc, d),
                crate::compile_cached(p.user_src, &self.user_tc, d),
                crate::compile_cached(p.oracle_src, &self.oracle_tc, d),
            ),
            None => tokio::join!(
                compile_source(p.gen_src, &self.gen_tc),
                compile_source(p.user_src, &self.user_tc),
                compile_source(p.oracle_src, &self.oracle_tc),
            ),
        };
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
        let fail_count = Arc::new(AtomicU32::new(0));
        let report = Arc::new(tokio::sync::Mutex::new(StressReport::default()));

        let jobs = p.jobs.max(1);
        let mut handles = Vec::new();

        for _ in 0..jobs {
            let next_round = next_round.clone();
            let fail_count = fail_count.clone();
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
                    // stop_on_fail = 任何一类失败（hack/gen 异常/标程异常）即停
                    if stop_on_fail && fail_count.load(Ordering::Relaxed) > 0 {
                        break;
                    }
                    let r = next_round.fetch_add(1, Ordering::Relaxed);
                    if r >= rounds {
                        break;
                    }
                    let seed = seed_start + r as u64;
                    let ctx = RoundCtx {
                        generator: &gen_prog,
                        user: &user_prog,
                        oracle: &oracle_prog,
                        limits_run: &limits_run,
                        limits_gen: &limits_gen,
                        checker: &checker,
                    };
                    let outcome = run_one_round(seed, r, &ctx, cancel.clone()).await;

                    {
                        let mut rep = report.lock().await;
                        rep.rounds_done += 1;
                        match &outcome {
                            StressEvent::Match { .. } => rep.matched += 1,
                            StressEvent::Hack { case, .. } => {
                                rep.hacks.push((**case).clone());
                                fail_count.fetch_add(1, Ordering::Relaxed);
                            }
                            StressEvent::GenFail { .. } => {
                                rep.gen_failures += 1;
                                fail_count.fetch_add(1, Ordering::Relaxed);
                            }
                            StressEvent::OracleFail { .. } => {
                                rep.oracle_failures += 1;
                                fail_count.fetch_add(1, Ordering::Relaxed);
                            }
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

/// 一轮对拍用到的只读上下文（程序组 + 时限 + 判题器，轮间共享）
struct RoundCtx<'a> {
    generator: &'a Program,
    user: &'a Program,
    oracle: &'a Program,
    limits_run: &'a Limits,
    limits_gen: &'a Limits,
    checker: &'a CheckerSpec,
}

/// 单轮：gen(seed) → input → user & oracle → 比对
async fn run_one_round(seed: u64, round: u32, ctx: &RoundCtx<'_>, cancel: Option<Arc<AtomicBool>>) -> StressEvent {
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
    let gen_p = with_extra_arg(ctx.generator, &seed.to_string());
    let g = match run_program(&gen_p, &gen_io, ctx.limits_gen, cancel.clone()).await {
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
    let user_io = Io { stdin: Some(in_f.clone()), stdout: td.join("user.out"), stderr: td.join("user.err") };
    let oracle_io = Io { stdin: Some(in_f), stdout: td.join("oracle.out"), stderr: td.join("oracle.err") };
    let (u, o) = tokio::join!(
        run_program(ctx.user, &user_io, ctx.limits_run, cancel.clone()),
        run_program(ctx.oracle, &oracle_io, ctx.limits_run, cancel)
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

    if u.status == RunStatus::Cancelled || o.status == RunStatus::Cancelled {
        return StressEvent::GenFail { seed, round, msg: "已取消".into() };
    }

    if o.status != RunStatus::Ok {
        let e = std::fs::read_to_string(&oracle_io.stderr).unwrap_or_default();
        return StressEvent::OracleFail { seed, round, msg: format!("标程 {:?}: {}", o.status, truncate(&e, 300)) };
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
            seed,
            round,
            case: Box::new(HackCase {
                seed,
                input,
                user_out,
                oracle_out,
                kind: kind.into(),
                detail: if e.is_empty() { format!("{:?}", u.status) } else { truncate(&e, 300) },
            }),
        };
    }

    // 4. 比对
    let check = shuati_core::CheckCtx { input: &input, actual: &user_out, expected: &oracle_out };
    match check_async(ctx.checker, &check, &td).await {
        Ok(Verdict::Ac) => StressEvent::Match { seed, round, cpu_ms: u.cpu_ms },
        Ok(_) => StressEvent::Hack {
            seed,
            round,
            case: Box::new(HackCase {
                seed,
                input,
                user_out,
                oracle_out,
                kind: "wa".into(),
                detail: "output differs".into(),
            }),
        },
        Err(e) => StressEvent::OracleFail { seed, round, msg: format!("checker: {e}") },
    }
}

fn truncate(s: &str, n: usize) -> String {
    if s.len() <= n { s.to_string() } else { format!("{}…", s.chars().take(n).collect::<String>()) }
}

/// gen_cases 的批次结果：产出的用例 + 失败明细（不静默吞错）
#[derive(Debug, Default)]
pub struct GenBatch {
    /// (seed, input, output)
    pub cases: Vec<(u64, String, String)>,
    /// (seed, 失败原因)
    pub failed: Vec<(u64, String)>,
}

/// 批量测试集生成：gen(seed) → oracle → 返回用例对与失败明细（调用方落盘/展示）
pub async fn gen_cases(
    gen_prog: &Program,
    oracle_prog: &Program,
    seeds: impl Iterator<Item = u64>,
    limits_run: &Limits,
    limits_gen: &Limits,
) -> GenBatch {
    let mut batch = GenBatch::default();
    for seed in seeds {
        let r = gen_one_case(gen_prog, oracle_prog, seed, limits_run, limits_gen).await;
        match r {
            Ok((input, output)) => batch.cases.push((seed, input, output)),
            Err(e) => batch.failed.push((seed, e)),
        }
    }
    batch
}

async fn gen_one_case(
    gen_prog: &Program,
    oracle_prog: &Program,
    seed: u64,
    limits_run: &Limits,
    limits_gen: &Limits,
) -> std::result::Result<(String, String), String> {
    let tmp = tempfile::tempdir().map_err(|e| e.to_string())?;
    let td = tmp.path().to_path_buf();
    let gen_out = td.join("g.out");
    let gen_err = td.join("g.err");
    let gen_io = Io { stdin: None, stdout: gen_out.clone(), stderr: gen_err.clone() };
    let g = run_program(&with_extra_arg(gen_prog, &seed.to_string()), &gen_io, limits_gen, None)
        .await
        .map_err(|e| e.to_string())?;
    if g.status != RunStatus::Ok {
        let e = std::fs::read_to_string(&gen_err).unwrap_or_default();
        return Err(format!("gen {:?}: {}", g.status, truncate(&e, 120)));
    }
    let input = std::fs::read_to_string(&gen_out).unwrap_or_default();
    if input.trim().is_empty() {
        return Err("gen 输出为空".into());
    }
    let in_f = td.join("c.in");
    std::fs::write(&in_f, &input).map_err(|e| e.to_string())?;
    let o_err = td.join("o.err");
    let io = Io { stdin: Some(in_f), stdout: td.join("o.out"), stderr: o_err.clone() };
    let o = run_program(oracle_prog, &io, limits_run, None).await.map_err(|e| e.to_string())?;
    if o.status != RunStatus::Ok {
        let e = std::fs::read_to_string(&o_err).unwrap_or_default();
        return Err(format!("oracle {:?}: {}", o.status, truncate(&e, 120)));
    }
    let output = std::fs::read_to_string(&io.stdout).unwrap_or_default();
    Ok((input, output))
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
