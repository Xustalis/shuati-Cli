//! TestEngine：编译 → 逐用例跑 → checker → 聚合报告。
//! GUI/CLI/对拍共用这一条管线。

use crate::{check_async, compile_source, run_program, JudgeError, Result, RunStatus, Toolchain};
use shuati_core::{CaseResult, CheckerSpec, Limits, ProblemId, RunKind, SourceFile, TestCase, TestReport, Verdict};
use std::path::PathBuf;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

/// 评测进度回调（GUI 实时刷新用）
pub type ProgressFn = Arc<dyn Fn(usize, usize, &CaseResult) + Send + Sync>;

pub struct TestEngine {
    pub toolchain: Toolchain,
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
        Self { toolchain }
    }

    /// 完整评测：编译一次 → 跑全部用例 → 报告
    pub async fn run(&self, p: &RunParams<'_>) -> Result<TestReport> {
        // 1. 编译
        let prog = compile_source(p.solution, &self.toolchain).await?;

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
                    let r = run_one(&prog, &cases[i], &limits, &checker, &tmp, i).await;
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

        let mut indexed = results.lock().await.clone();
        indexed.sort_by_key(|(i, _)| *i);
        let cases_res: Vec<CaseResult> = indexed.into_iter().map(|(_, r)| r).collect();

        let pass = cases_res.iter().filter(|c| c.verdict == Verdict::Ac).count();
        let verdict = cases_res
            .iter()
            .find(|c| c.verdict != Verdict::Ac)
            .map(|c| c.verdict)
            .unwrap_or(Verdict::Ac);

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
    tmpdir: &PathBuf,
    idx: usize,
) -> CaseResult {
    let base = tmpdir.join(format!("case_{idx}"));
    let in_f = base.with_extension("in");
    let out_f = base.with_extension("out");
    let err_f = base.with_extension("err");
    if let Err(e) = std::fs::write(&in_f, &tc.input) {
        return err_result(&tc.name, format!("write input: {e}"));
    }

    let io = crate::Io { stdin: Some(in_f.clone()), stdout: out_f.clone(), stderr: err_f.clone() };
    let outcome = match run_program(prog, &io, limits).await {
        Ok(o) => o,
        Err(e) => return err_result(&tc.name, e.to_string()),
    };

    let actual = std::fs::read_to_string(&out_f).unwrap_or_default();
    let stderr_text = std::fs::read_to_string(&err_f).unwrap_or_default();

    let (verdict, message) = match outcome.status {
        RunStatus::Ok => {
            let expected = tc.expected.clone().unwrap_or_default();
            let ctx = shuati_core::CheckCtx { input: &tc.input, actual: &actual, expected: &expected };
            match check_async(checker, &ctx, tmpdir).await {
                Ok(v) => (v, String::new()),
                Err(e) => (Verdict::Se, e.to_string()),
            }
        }
        RunStatus::TimeLimit => (Verdict::Tle, String::new()),
        RunStatus::MemoryLimit => (Verdict::Mle, String::new()),
        RunStatus::RuntimeError => (
            Verdict::Re,
            if stderr_text.is_empty() { format!("exit code {}", outcome.exit_code) } else { stderr_text },
        ),
        RunStatus::InternalError => (Verdict::Se, outcome.internal.clone()),
    };

    CaseResult {
        name: tc.name.clone(),
        verdict,
        cpu_ms: outcome.cpu_ms,
        memory_kb: outcome.peak_mem_kb,
        message,
        input: tc.input.clone(),
        actual,
        expected: tc.expected.clone().unwrap_or_default(),
    }
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

/// 清理编译产物（cpp exe；python 不删源文件）
pub fn cleanup_program(prog: &crate::Program) {
    if prog.lang == shuati_core::Lang::Cpp && prog.artifact.exists() {
        let _ = std::fs::remove_file(&prog.artifact);
    }
}
