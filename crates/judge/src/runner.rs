//! 进程运行器：在资源限制下执行 Program，返回结构化结果。
//! Unix: pre_exec 设进程组 + rlimit；wait4 精确取 cpu/内存；超时 killpg 整组。

use crate::{Io, JudgeError, Program, Result};
use shuati_core::Limits;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::atomic::AtomicBool;
use std::time::{Duration, Instant};

/// 一次运行的结构化结果
#[derive(Debug, Clone)]
pub struct RunOutcome {
    pub status: RunStatus,
    pub exit_code: i32,
    /// CPU 时间（user+sys，毫秒）
    pub cpu_ms: u64,
    /// 墙钟时间
    pub wall_ms: u64,
    /// 峰值内存（KB；0 = 平台不支持）
    pub peak_mem_kb: u64,
    pub stdout_path: PathBuf,
    pub stderr_path: PathBuf,
    /// 内部错误信息（沙箱自身故障）
    pub internal: String,
}

/// stderr 探针只需头部：本地化报错/bad_alloc 都在开头，整读可能被刷屏撑爆内存
pub(crate) const STDERR_HEAD_CAP: u64 = 64 * 1024;

pub(crate) fn read_head(path: &std::path::Path, cap: u64) -> String {
    use std::io::Read;
    let mut buf = Vec::new();
    if let Ok(f) = std::fs::File::open(path) {
        let _ = f.take(cap).read_to_end(&mut buf);
    }
    String::from_utf8_lossy(&buf).into_owned()
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RunStatus {
    Ok,
    TimeLimit,
    MemoryLimit,
    RuntimeError,
    /// 输出文件超过 output_mb 上限
    OutputLimit,
    /// 被外部取消（用户按停止/切任务）
    Cancelled,
    InternalError,
}

/// 运行一个 Program：argv 数组直传内核，无 shell。
/// `cancel` 置 true 后尽快杀掉进程并返回 Cancelled。
pub async fn run_program(
    prog: &Program,
    io: &Io,
    limits: &Limits,
    cancel: Option<Arc<AtomicBool>>,
) -> Result<RunOutcome> {
    let prog = prog.clone();
    let io = io.clone();
    let limits = *limits;
    tokio::task::spawn_blocking(move || run_blocking(&prog, &io, &limits, cancel))
        .await
        .map_err(|e| JudgeError::Sandbox(format!("join: {e}")))?
}

#[cfg(unix)]
fn run_blocking(prog: &Program, io: &Io, limits: &Limits, cancel: Option<Arc<AtomicBool>>) -> Result<RunOutcome> {
    use std::os::unix::process::CommandExt;
    use std::process::{Command, Stdio};

    let stdin = match &io.stdin {
        Some(p) => Stdio::from(std::fs::File::open(p)?),
        None => Stdio::null(),
    };
    let stdout = Stdio::from(std::fs::File::create(&io.stdout)?);
    let stderr = Stdio::from(std::fs::File::create(&io.stderr)?);

    let mem_bytes = limits.memory_mb.saturating_mul(1024 * 1024);
    let fsize_bytes = limits.output_mb.saturating_mul(1024 * 1024);
    // RLIMIT_CPU 粒度是秒，向上取整再 +1s 余量（精确计时靠 wall + rusage）
    let cpu_secs = limits.cpu_ms.div_ceil(1000) + 1;

    let mut cmd = Command::new(&prog.argv[0]);
    cmd.args(&prog.argv[1..]).current_dir(&prog.workdir).stdin(stdin).stdout(stdout).stderr(stderr);

    unsafe {
        cmd.pre_exec(move || {
            // 新进程组，便于超时整组 kill
            if libc::setpgid(0, 0) != 0 {
                return Err(std::io::Error::last_os_error());
            }
            // rlimit 尽力而为：macOS 不支持 RLIMIT_AS（EINVAL），
            // 内存超限由 rusage 事后判定；Linux 上仍是硬限制。
            let _ = set_rlimit(libc::RLIMIT_AS, mem_bytes);
            let _ = set_rlimit(libc::RLIMIT_FSIZE, fsize_bytes);
            let _ = set_rlimit(libc::RLIMIT_CPU, cpu_secs);
            Ok(())
        });
    }

    let child = cmd.spawn().map_err(|e| JudgeError::Sandbox(format!("spawn {}: {e}", prog.argv[0])))?;
    let pid = child.id() as i32;

    let start = Instant::now();
    let wall_limit = Duration::from_millis(limits.wall_ms.max(500));
    let mut timed_out = false;
    let mut cancelled = false;

    // wait4 轮询：拿到 per-child rusage
    let (status, usage) = loop {
        let mut st: libc::c_int = 0;
        let mut ru: libc::rusage = unsafe { std::mem::zeroed() };
        let r = unsafe { libc::wait4(pid, &mut st, libc::WNOHANG, &mut ru) };
        if r == pid {
            break (st, ru);
        }
        if r < 0 {
            let err = std::io::Error::last_os_error();
            if err.raw_os_error() == Some(libc::ECHILD) {
                return Err(JudgeError::Sandbox("child reaped unexpectedly".into()));
            }
            return Err(JudgeError::Io(err));
        }
        if start.elapsed() > wall_limit || cancel.as_ref().is_some_and(|c| c.load(std::sync::atomic::Ordering::Relaxed))
        {
            timed_out = start.elapsed() > wall_limit;
            cancelled = !timed_out;
            unsafe {
                libc::kill(-pid, libc::SIGKILL);
            }
            // reap
            let mut st2: libc::c_int = 0;
            let mut ru2: libc::rusage = unsafe { std::mem::zeroed() };
            unsafe { libc::wait4(pid, &mut st2, 0, &mut ru2) };
            break (st2, ru2);
        }
        std::thread::sleep(Duration::from_millis(3));
    };

    let wall_ms = start.elapsed().as_millis() as u64;
    let cpu_ms = (usage.ru_utime.tv_sec as u64 * 1000 + usage.ru_utime.tv_usec as u64 / 1000)
        + (usage.ru_stime.tv_sec as u64 * 1000 + usage.ru_stime.tv_usec as u64 / 1000);
    // ru_maxrss: Linux=KB, macOS=bytes
    #[cfg(target_os = "macos")]
    let peak_mem_kb = usage.ru_maxrss as u64 / 1024;
    #[cfg(not(target_os = "macos"))]
    let peak_mem_kb = usage.ru_maxrss as u64;

    let exit_code = if libc::WIFEXITED(status) { libc::WEXITSTATUS(status) } else { -1 };
    let stderr_text = read_head(&io.stderr, STDERR_HEAD_CAP);
    // 输出超限判定（各平台通用兜底，rlimit 未覆盖的平台也生效）。
    // stdout+stderr 合并计：RLIMIT_FSIZE 是单文件限，往 stderr 刷能绕过只看 stdout 的判定
    let out_bytes = std::fs::metadata(&io.stdout).map(|m| m.len()).unwrap_or(0);
    let err_bytes = std::fs::metadata(&io.stderr).map(|m| m.len()).unwrap_or(0);
    let over_output = out_bytes.saturating_add(err_bytes) > fsize_bytes;

    let status_out = if cancelled {
        RunStatus::Cancelled
    } else if timed_out {
        RunStatus::TimeLimit
    } else if cpu_ms > limits.cpu_ms {
        // rusage 精判 CPU TLE（wall 没超但 cpu 超了）
        RunStatus::TimeLimit
    } else if over_output {
        RunStatus::OutputLimit
    } else if peak_mem_kb > limits.memory_mb.saturating_mul(1024) {
        // rusage 事后判 MLE（macOS 无 RLIMIT_AS 硬限，靠这个兜）
        RunStatus::MemoryLimit
    } else if libc::WIFSIGNALED(status) {
        let sig = libc::WTERMSIG(status);
        match sig {
            libc::SIGXCPU => RunStatus::TimeLimit,
            libc::SIGXFSZ => RunStatus::OutputLimit,
            libc::SIGSEGV | libc::SIGBUS | libc::SIGKILL => {
                if peak_mem_kb >= limits.memory_mb.saturating_sub(8) * 1024 || stderr_text.contains("bad_alloc") {
                    RunStatus::MemoryLimit
                } else {
                    RunStatus::RuntimeError
                }
            }
            _ => RunStatus::RuntimeError,
        }
    } else if exit_code != 0 {
        if stderr_text.contains("bad_alloc") || stderr_text.contains("MemoryError") {
            RunStatus::MemoryLimit
        } else {
            RunStatus::RuntimeError
        }
    } else {
        RunStatus::Ok
    };

    // 防止 Child drop 时重复回收（已经 wait4 过）——直接忘掉它
    std::mem::forget(child);

    Ok(RunOutcome {
        status: status_out,
        exit_code,
        cpu_ms,
        wall_ms,
        peak_mem_kb,
        stdout_path: io.stdout.clone(),
        stderr_path: io.stderr.clone(),
        internal: String::new(),
    })
}

#[cfg(unix)]
fn set_rlimit(res: libc::c_int, val: u64) -> std::io::Result<()> {
    let rl = libc::rlimit { rlim_cur: val, rlim_max: val };
    if unsafe { libc::setrlimit(res, &rl) } != 0 { Err(std::io::Error::last_os_error()) } else { Ok(()) }
}

/// Windows：Job Object 沙箱。
/// KILL_ON_JOB_CLOSE + PROCESS_MEMORY 硬限 + PROCESS_TIME CPU 限；
/// 轮询里读 IoInfo.WriteTransferCount 实时掐输出、查取消；
/// TerminateJobObject 杀整棵进程树（child.kill 只杀根进程）。
#[cfg(windows)]
mod win_job {
    use super::*;
    use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
    use windows_sys::Win32::System::JobObjects::*;

    pub struct Job(HANDLE);

    impl Job {
        /// 按 limits 建 Job；失败（旧系统/权限）返回 None → 走基础路径
        pub fn new(limits: &Limits) -> Option<Self> {
            unsafe {
                let h = CreateJobObjectW(std::ptr::null(), std::ptr::null());
                if h.is_null() {
                    return None;
                }
                let mut info: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std::mem::zeroed();
                info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
                    | JOB_OBJECT_LIMIT_PROCESS_MEMORY
                    | JOB_OBJECT_LIMIT_PROCESS_TIME;
                // PerProcessUserTimeLimit 单位是 100ns，只计 user 态——放宽 2 倍再 +2s 余量，
                // 精确 TLE 判定靠退出后读 TotalUserTime+TotalKernelTime
                let cpu_100ns = limits.cpu_ms.saturating_mul(2).saturating_add(2000).saturating_mul(10_000) as i64;
                info.BasicLimitInformation.PerProcessUserTimeLimit = cpu_100ns;
                info.ProcessMemoryLimit = limits.memory_mb.saturating_mul(1024 * 1024) as usize;
                if SetInformationJobObject(
                    h,
                    JobObjectExtendedLimitInformation,
                    &info as *const _ as *const _,
                    std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32,
                ) == 0
                {
                    CloseHandle(h);
                    return None;
                }
                Some(Self(h))
            }
        }

        pub fn assign(&self, process: HANDLE) -> std::io::Result<()> {
            unsafe {
                if AssignProcessToJobObject(self.0, process) == 0 {
                    return Err(std::io::Error::last_os_error());
                }
                Ok(())
            }
        }

        pub fn kill(&self) {
            unsafe {
                TerminateJobObject(self.0, 1);
            }
        }

        /// (cpu_ms, 累计写出字节)
        pub fn accounting(&self) -> Option<(u64, u64)> {
            unsafe {
                let mut acc: JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION = std::mem::zeroed();
                if QueryInformationJobObject(
                    self.0,
                    JobObjectBasicAndIoAccountingInformation,
                    &mut acc as *mut _ as *mut _,
                    std::mem::size_of::<JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION>() as u32,
                    std::ptr::null_mut(),
                ) == 0
                {
                    return None;
                }
                let cpu_100ns = acc.BasicInfo.TotalUserTime + acc.BasicInfo.TotalKernelTime;
                Some(((cpu_100ns / 10_000).max(0) as u64, acc.IoInfo.WriteTransferCount))
            }
        }

        /// 峰值内存（KB，整个 job 累计峰值）
        pub fn peak_mem_kb(&self) -> u64 {
            unsafe {
                let mut info: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std::mem::zeroed();
                if QueryInformationJobObject(
                    self.0,
                    JobObjectExtendedLimitInformation,
                    &mut info as *mut _ as *mut _,
                    std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32,
                    std::ptr::null_mut(),
                ) == 0
                {
                    return 0;
                }
                (info.PeakJobMemoryUsed / 1024) as u64
            }
        }
    }

    impl Drop for Job {
        fn drop(&mut self) {
            unsafe {
                CloseHandle(self.0);
            }
        }
    }
}

#[cfg(windows)]
fn run_blocking(prog: &Program, io: &Io, limits: &Limits, cancel: Option<Arc<AtomicBool>>) -> Result<RunOutcome> {
    use std::process::{Command, Stdio};

    let stdin = match &io.stdin {
        Some(p) => Stdio::from(std::fs::File::open(p)?),
        None => Stdio::null(),
    };
    let stdout = Stdio::from(std::fs::File::create(&io.stdout)?);
    let stderr = Stdio::from(std::fs::File::create(&io.stderr)?);

    let mut cmd = Command::new(&prog.argv[0]);
    cmd.args(&prog.argv[1..]).current_dir(&prog.workdir).stdin(stdin).stdout(stdout).stderr(stderr);

    let mut child = cmd.spawn().map_err(|e| JudgeError::Sandbox(format!("spawn: {e}")))?;
    let start = Instant::now();
    let wall_limit = Duration::from_millis(limits.wall_ms.max(500));
    let fsize_bytes = limits.output_mb.saturating_mul(1024 * 1024);

    // Job Object：assign 失败（比如父进程已在不兼容的 job 里）→ 退回无沙箱路径
    use std::os::windows::io::AsRawHandle;
    let job = win_job::Job::new(limits).and_then(|j| match j.assign(child.as_raw_handle() as _) {
        Ok(()) => Some(j),
        Err(e) => {
            tracing::warn!("assign job failed, sandbox off: {e}");
            None
        }
    });

    let (mut status, exit_code) = loop {
        match child.try_wait()? {
            Some(st) => {
                break (if st.success() { RunStatus::Ok } else { RunStatus::RuntimeError }, st.code().unwrap_or(-1));
            }
            None => {
                let mut kill_reason = None;
                if start.elapsed() > wall_limit {
                    kill_reason = Some(RunStatus::TimeLimit);
                } else if cancel.as_ref().is_some_and(|c| c.load(std::sync::atomic::Ordering::Relaxed)) {
                    kill_reason = Some(RunStatus::Cancelled);
                } else if let Some(j) = &job {
                    // 实时输出计数（含 stderr/文件写，偏保守）
                    if let Some((_, written)) = j.accounting() {
                        if written > fsize_bytes {
                            kill_reason = Some(RunStatus::OutputLimit);
                        }
                    }
                }
                if let Some(st) = kill_reason {
                    match &job {
                        Some(j) => j.kill(),
                        None => {
                            let _ = child.kill();
                        }
                    }
                    let _ = child.wait();
                    break (st, -1);
                }
                std::thread::sleep(Duration::from_millis(3));
            }
        }
    };

    let elapsed = start.elapsed().as_millis() as u64;
    let (cpu_ms, peak_kb) = job
        .as_ref()
        .map(|j| (j.accounting().map(|(c, _)| c).unwrap_or(elapsed), j.peak_mem_kb()))
        .unwrap_or((elapsed, 0));

    // 兜底判定：job 的 CPU/内存硬限触发时进程被杀死 → try_wait 拿到非 0 退出码（RE），
    // 这里用 job 记账把误报的 RE 修回 TLE/MLE
    if matches!(status, RunStatus::Ok | RunStatus::RuntimeError) {
        if cpu_ms > limits.cpu_ms {
            status = RunStatus::TimeLimit;
        } else if peak_kb > limits.memory_mb.saturating_mul(1024)
            || read_head(&io.stderr, STDERR_HEAD_CAP).contains("bad_alloc")
        {
            status = RunStatus::MemoryLimit;
        } else if std::fs::metadata(&io.stdout)
            .map(|m| m.len())
            .unwrap_or(0)
            .saturating_add(std::fs::metadata(&io.stderr).map(|m| m.len()).unwrap_or(0))
            > fsize_bytes
        {
            status = RunStatus::OutputLimit;
        }
    }

    Ok(RunOutcome {
        status,
        exit_code,
        cpu_ms,
        wall_ms: elapsed,
        peak_mem_kb: peak_kb,
        stdout_path: io.stdout.clone(),
        stderr_path: io.stderr.clone(),
        internal: String::new(),
    })
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use shuati_core::Lang;

    fn prog(argv: &[&str]) -> Program {
        Program {
            argv: argv.iter().map(|s| s.to_string()).collect(),
            lang: Lang::Cpp,
            artifact: PathBuf::from(argv[0]),
            workdir: std::env::temp_dir(),
            cached: false,
        }
    }

    fn io_paths(tag: &str) -> (Io, tempfile::TempDir) {
        let d = tempfile::tempdir().unwrap();
        let io = Io {
            stdin: None,
            stdout: d.path().join(format!("{tag}.out")),
            stderr: d.path().join(format!("{tag}.err")),
        };
        (io, d)
    }

    #[tokio::test]
    async fn echo_ok() {
        let (io, _d) = io_paths("echo");
        let out = run_program(&prog(&["/bin/echo", "hello"]), &io, &Limits::default(), None).await.unwrap();
        assert_eq!(out.status, RunStatus::Ok);
        assert_eq!(std::fs::read_to_string(&out.stdout_path).unwrap().trim(), "hello");
    }

    #[tokio::test]
    async fn sleeper_tle() {
        let (io, _d) = io_paths("sleep");
        let l = Limits { wall_ms: 200, ..Default::default() };
        let out = run_program(&prog(&["/bin/sleep", "10"]), &io, &l, None).await.unwrap();
        assert_eq!(out.status, RunStatus::TimeLimit);
    }

    #[tokio::test]
    async fn exit_nonzero_re() {
        let (io, _d) = io_paths("false");
        let out = run_program(&prog(&["/usr/bin/false"]), &io, &Limits::default(), None).await.unwrap();
        assert_eq!(out.status, RunStatus::RuntimeError);
    }

    #[tokio::test]
    async fn cancel_kills_child() {
        let (io, _d) = io_paths("cancel");
        let cancel = Arc::new(AtomicBool::new(false));
        let c2 = cancel.clone();
        let p = prog(&["/bin/sleep", "30"]);
        let l = Limits::default();
        let h = tokio::spawn(async move { run_program(&p, &io, &l, Some(c2)).await });
        std::thread::sleep(Duration::from_millis(100));
        cancel.store(true, std::sync::atomic::Ordering::Relaxed);
        let out = h.await.unwrap().unwrap();
        assert_eq!(out.status, RunStatus::Cancelled);
    }

    #[tokio::test]
    async fn output_limit() {
        let (io, _d) = io_paths("ole");
        let l = Limits { cpu_ms: 5000, wall_ms: 10000, memory_mb: 256, output_mb: 0 };
        // yes 输出无限流，应被判 OutputLimit
        let out = run_program(&prog(&["/usr/bin/yes"]), &io, &l, None).await.unwrap();
        assert_eq!(out.status, RunStatus::OutputLimit);
    }
}
