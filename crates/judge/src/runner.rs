//! 进程运行器：在资源限制下执行 Program，返回结构化结果。
//! Unix: pre_exec 设进程组 + rlimit；wait4 精确取 cpu/内存；超时 killpg 整组。

use crate::{Io, JudgeError, Program, Result};
use shuati_core::Limits;
use std::path::PathBuf;
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RunStatus {
    Ok,
    TimeLimit,
    MemoryLimit,
    RuntimeError,
    InternalError,
}

/// 运行一个 Program：argv 数组直传内核，无 shell。
pub async fn run_program(prog: &Program, io: &Io, limits: &Limits) -> Result<RunOutcome> {
    let prog = prog.clone();
    let io = io.clone();
    let limits = *limits;
    tokio::task::spawn_blocking(move || run_blocking(&prog, &io, &limits))
        .await
        .map_err(|e| JudgeError::Sandbox(format!("join: {e}")))?
}

#[cfg(unix)]
fn run_blocking(prog: &Program, io: &Io, limits: &Limits) -> Result<RunOutcome> {
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
    cmd.args(&prog.argv[1..])
        .current_dir(&prog.workdir)
        .stdin(stdin)
        .stdout(stdout)
        .stderr(stderr);

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

    let child = cmd.spawn().map_err(|e| {
        JudgeError::Sandbox(format!("spawn {}: {e}", prog.argv[0]))
    })?;
    let pid = child.id() as i32;

    let start = Instant::now();
    let wall_limit = Duration::from_millis(limits.wall_ms.max(500));
    let mut timed_out = false;

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
        if start.elapsed() > wall_limit {
            timed_out = true;
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
    let stderr_text = std::fs::read_to_string(&io.stderr).unwrap_or_default();

    let status_out = if timed_out {
        RunStatus::TimeLimit
    } else if cpu_ms > limits.cpu_ms {
        // rusage 精判 CPU TLE（wall 没超但 cpu 超了）
        RunStatus::TimeLimit
    } else if peak_mem_kb > limits.memory_mb.saturating_mul(1024) {
        // rusage 事后判 MLE（macOS 无 RLIMIT_AS 硬限，靠这个兜）
        RunStatus::MemoryLimit
    } else if libc::WIFSIGNALED(status) {
        let sig = libc::WTERMSIG(status);
        match sig {
            libc::SIGXCPU | libc::SIGXFSZ => RunStatus::TimeLimit,
            libc::SIGSEGV | libc::SIGBUS | libc::SIGKILL => {
                if peak_mem_kb >= limits.memory_mb.saturating_sub(8) * 1024
                    || stderr_text.contains("bad_alloc")
                {
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
    if unsafe { libc::setrlimit(res, &rl) } != 0 {
        Err(std::io::Error::last_os_error())
    } else {
        Ok(())
    }
}

/// Windows 初版：spawn + wall 超时杀进程树；Job Object 细化留 TODO。
#[cfg(windows)]
fn run_blocking(prog: &Program, io: &Io, limits: &Limits) -> Result<RunOutcome> {
    use std::process::{Command, Stdio};

    let stdin = match &io.stdin {
        Some(p) => Stdio::from(std::fs::File::open(p)?),
        None => Stdio::null(),
    };
    let stdout = Stdio::from(std::fs::File::create(&io.stdout)?);
    let stderr = Stdio::from(std::fs::File::create(&io.stderr)?);

    let mut cmd = Command::new(&prog.argv[0]);
    cmd.args(&prog.argv[1..])
        .current_dir(&prog.workdir)
        .stdin(stdin)
        .stdout(stdout)
        .stderr(stderr);

    let mut child = cmd.spawn().map_err(|e| JudgeError::Sandbox(format!("spawn: {e}")))?;
    let start = Instant::now();
    let wall_limit = Duration::from_millis(limits.wall_ms.max(500));

    loop {
        match child.try_wait()? {
            Some(st) => {
                return Ok(RunOutcome {
                    status: if st.success() { RunStatus::Ok } else { RunStatus::RuntimeError },
                    exit_code: st.code().unwrap_or(-1),
                    cpu_ms: start.elapsed().as_millis() as u64,
                    wall_ms: start.elapsed().as_millis() as u64,
                    peak_mem_kb: 0,
                    stdout_path: io.stdout.clone(),
                    stderr_path: io.stderr.clone(),
                    internal: String::new(),
                });
            }
            None => {
                if start.elapsed() > wall_limit {
                    let _ = child.kill();
                    let _ = child.wait();
                    return Ok(RunOutcome {
                        status: RunStatus::TimeLimit,
                        exit_code: -1,
                        cpu_ms: limits.wall_ms,
                        wall_ms: start.elapsed().as_millis() as u64,
                        peak_mem_kb: 0,
                        stdout_path: io.stdout.clone(),
                        stderr_path: io.stderr.clone(),
                        internal: String::new(),
                    });
                }
                std::thread::sleep(Duration::from_millis(3));
            }
        }
    }
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
        let out = run_program(&prog(&["/bin/echo", "hello"]), &io, &Limits::default()).await.unwrap();
        assert_eq!(out.status, RunStatus::Ok);
        assert_eq!(std::fs::read_to_string(&out.stdout_path).unwrap().trim(), "hello");
    }

    #[tokio::test]
    async fn sleeper_tle() {
        let (io, _d) = io_paths("sleep");
        let mut l = Limits::default();
        l.wall_ms = 200;
        let out = run_program(&prog(&["/bin/sleep", "10"]), &io, &l).await.unwrap();
        assert_eq!(out.status, RunStatus::TimeLimit);
    }

    #[tokio::test]
    async fn exit_nonzero_re() {
        let (io, _d) = io_paths("false");
        let out = run_program(&prog(&["/usr/bin/false"]), &io, &Limits::default()).await.unwrap();
        assert_eq!(out.status, RunStatus::RuntimeError);
    }
}
