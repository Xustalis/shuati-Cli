//! TestEngine：编译 → 逐用例跑 → checker → 聚合报告。
//! GUI/CLI/对拍共用这一条管线。

use crate::{JudgeError, Result, RunStatus, Toolchain, check_async, compile_source, run_program};
use shuati_core::{CaseResult, CheckerSpec, Limits, ProblemId, RunKind, SourceFile, TestCase, TestReport, Verdict};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};

/// 评测进度回调（GUI 实时刷新用）
pub type ProgressFn = Arc<dyn Fn(usize, usize, &CaseResult) + Send + Sync>;

pub struct TestEngine {
    pub toolchain: Toolchain,
    /// 编译缓存目录（None = 不缓存）
    cache_dir: Option<PathBuf>,
}

/// 引擎运行参数
pub struct RunParams<'a> {
    pub problem: &'a ProblemId,
    pub solution: &'a SourceFile,
    pub cases: &'a [TestCase],
    pub limits: &'a Limits,
    pub checker: &'a CheckerSpec,
    /// 并行度（1 = 串行）
    pub jobs: usize,
    /// 进度回调：完成第 i 个（1-based）时触发
    pub progress: Option<ProgressFn>,
    /// 取消信号：置 true 后尽快停止
    pub cancel: Option<Arc<std::sync::atomic::AtomicBool>>,
}

impl TestEngine {
    pub fn new(toolchain: Toolchain) -> Self {
        Self { toolchain, cache_dir: None }
    }

    /// 启用编译缓存：源未变时复用产物，跳过 g++ 调用
    pub fn with_cache(mut self, dir: PathBuf) -> Self {
        self.cache_dir = Some(dir);
        self
    }

    /// 完整评测：编译一次 → 跑全部用例 → 报告
    pub async fn run(&self, p: &RunParams<'_>) -> Result<TestReport> {
        // 1. 编译（有缓存目录时按 mtime+size 指纹复用）
        let prog = match &self.cache_dir {
            Some(d) => crate::compile_cached(p.solution, &self.toolchain, d).await?,
            None => compile_source(p.solution, &self.toolchain).await?,
        };

        let report = self.run_compiled(p, &prog).await;
        cleanup_program(&prog);
        report
    }

    /// 已编译产物跑用例（对拍复用：三个 Program 各自编译一次）
    pub async fn run_compiled(&self, p: &RunParams<'_>, prog: &crate::Program) -> Result<TestReport> {
        let total = p.cases.len();
        if total == 0 {
            return Err(JudgeError::NoCases);
        }

        let tmpdir = tempfile::Builder::new().prefix("shuati_run_").tempdir()?;
        let cases = Arc::new(p.cases.to_vec());
        let next = Arc::new(AtomicUsize::new(0));
        let results = Arc::new(tokio::sync::Mutex::new(Vec::<(usize, CaseResult)>::new()));
        let jobs = p.jobs.max(1).min(total);

        let mut handles = Vec::new();
        for _ in 0..jobs {
            let prog = prog.clone();
            let cases = cases.clone();
            let next = next.clone();
            let results = results.clone();
            let tmp = tmpdir.path().to_path_buf();
            let limits = *p.limits;
            let checker = p.checker.clone();
            let progress = p.progress.clone();
            let cancel = p.cancel.clone();

            handles.push(tokio::spawn(async move {
                loop {
                    if cancel.as_ref().is_some_and(|c| c.load(Ordering::Relaxed)) {
                        break;
                    }
                    let i = next.fetch_add(1, Ordering::Relaxed);
                    if i >= cases.len() {
                        break;
                    }
                    let r = run_one(&prog, &cases[i], &limits, &checker, &tmp, i, cancel.clone()).await;
                    if let Some(cb) = &progress {
                        cb(i + 1, cases.len(), &r);
                    }
                    results.lock().await.push((i, r));
                }
            }));
        }
        for h in handles {
            let _ = h.await;
        }

        let mut indexed = std::mem::take(&mut *results.lock().await);
        indexed.sort_by_key(|(i, _)| *i);
        let cases_res: Vec<CaseResult> = indexed.into_iter().map(|(_, r)| r).collect();

        let pass = cases_res.iter().filter(|c| c.verdict == Verdict::Ac).count();
        let verdict = cases_res.iter().find(|c| c.verdict != Verdict::Ac).map(|c| c.verdict).unwrap_or(Verdict::Ac);

        Ok(TestReport {
            problem: p.problem.clone(),
            timestamp: shuati_store::now_ts(),
            kind: RunKind::Test,
            verdict,
            pass,
            total,
            cases: cases_res,
        })
    }
}

/// 单个用例：run → check
async fn run_one(
    prog: &crate::Program,
    tc: &TestCase,
    limits: &Limits,
    checker: &CheckerSpec,
    tmpdir: &Path,
    idx: usize,
    cancel: Option<Arc<std::sync::atomic::AtomicBool>>,
) -> CaseResult {
    let base = tmpdir.join(format!("case_{idx}"));
    let in_f = base.with_extension("in");
    let out_f = base.with_extension("out");
    let err_f = base.with_extension("err");
    if let Err(e) = std::fs::write(&in_f, &tc.input) {
        return err_result(&tc.name, format!("write input: {e}"));
    }

    let io = crate::Io { stdin: Some(in_f.clone()), stdout: out_f.clone(), stderr: err_f.clone() };
    let outcome = match run_program(prog, &io, limits, cancel).await {
        Ok(o) => o,
        Err(e) => return err_result(&tc.name, e.to_string()),
    };

    let actual = std::fs::read_to_string(&out_f).unwrap_or_default();

    let (verdict, message) = match outcome.status {
        RunStatus::Ok => match &tc.expected {
            // 无 .out 的用例当 smoke run：能跑完即算过，标注明示
            None => (Verdict::Ac, "（无 .out 答案，仅验证可运行）".into()),
            Some(expected) => {
                let ctx = shuati_core::CheckCtx { input: &tc.input, actual: &actual, expected };
                match check_async(checker, &ctx, tmpdir).await {
                    Ok(v) => (v, String::new()),
                    Err(e) => (Verdict::Se, e.to_string()),
                }
            }
        },
        RunStatus::TimeLimit => (Verdict::Tle, String::new()),
        RunStatus::MemoryLimit => (Verdict::Mle, String::new()),
        RunStatus::OutputLimit => (Verdict::Ole, "输出超过 output_mb 限制".into()),
        RunStatus::Cancelled => (Verdict::Se, "已取消".into()),
        RunStatus::RuntimeError => {
            // stderr 只在 RE 时才读，且截头 64KB——AC 路径不该为不用的数据付 I/O
            let stderr_text = crate::read_head(&err_f, crate::STDERR_HEAD_CAP);
            (
                Verdict::Re,
                if stderr_text.is_empty() {
                    format!("exit code {}", outcome.exit_code)
                } else {
                    stderr_text.chars().take(2000).collect()
                },
            )
        }
        RunStatus::InternalError => (Verdict::Se, outcome.internal.clone()),
    };

    CaseResult {
        name: tc.name.clone(),
        verdict,
        cpu_ms: outcome.cpu_ms,
        memory_kb: outcome.peak_mem_kb,
        message,
        // 报告只为 diff 展示服务——完整数据在 tests/ 文件里，截断防报告膨胀
        input: cap_io(&tc.input),
        actual: cap_io(&actual),
        expected: cap_io(tc.expected.as_deref().unwrap_or("")),
    }
}

/// 报告里单个 IO 字段的上限：完整数据在 tests/ 和 stress/hack_* 里
const REPORT_IO_CAP: usize = 64 * 1024;

fn cap_io(s: &str) -> String {
    if s.len() <= REPORT_IO_CAP {
        return s.to_string();
    }
    // UTF-8 边界安全截断（直接按字节切会在多字节中间 panic）
    let mut end = 0;
    for (i, c) in s.char_indices() {
        if i > REPORT_IO_CAP {
            break;
        }
        end = i + c.len_utf8();
    }
    format!("{}…(truncated, 完整见 tests/)", &s[..end])
}

fn err_result(name: &str, msg: String) -> CaseResult {
    CaseResult {
        name: name.to_string(),
        verdict: Verdict::Se,
        cpu_ms: 0,
        memory_kb: 0,
        message: msg,
        input: String::new(),
        actual: String::new(),
        expected: String::new(),
    }
}

/// 清理编译产物（cpp exe；python 不删源文件；缓存产物保留复用）
pub fn cleanup_program(prog: &crate::Program) {
    if prog.cached {
        return;
    }
    if prog.lang == shuati_core::Lang::Cpp && prog.artifact.exists() {
        let _ = std::fs::remove_file(&prog.artifact);
    }
}
