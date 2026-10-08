//! shuati-cli：引擎的薄壳。
//! 无参数 → 拉起同目录的 shuati（GUI 主入口）；有参数 → 子命令。

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use shuati_core::{Lang, ProblemId, SourceFile};
use shuati_judge as judge;
use shuati_store as store;
use std::path::{Path, PathBuf};

#[derive(Parser)]
#[command(name = "shuati-cli", about = "本地评测小助手 · shuati v2", version)]
struct Cli {
    /// 题库根目录（默认：exe 同级 shuati-data/）
    #[arg(long, global = true)]
    root: Option<PathBuf>,
    #[command(subcommand)]
    cmd: Option<Cmd>,
}

#[derive(Subcommand)]
enum Cmd {
    /// 环境自检：编译器/解释器探测
    Doctor,
    /// 评测题目
    Test {
        /// 题目 id（如 luogu:P1001），省略用当前题目
        id: Option<String>,
        /// 指定源文件（默认 solutions/main.*）
        #[arg(long)]
        file: Option<PathBuf>,
        /// 并行度
        #[arg(short = 'j', long, default_value = "4")]
        jobs: usize,
    },
    /// 列出题目
    List,
    /// 创建本地题目
    New {
        /// 标题
        title: String,
        /// slug（默认自动生成）
        #[arg(long)]
        slug: Option<String>,
    },
    /// 从 URL 拉题（洛谷/Codeforces）
    Add {
        /// 题目链接
        url: String,
    },
    /// 给题目加用例：shuati case luogu:P1001 --in 1.in --out 1.out
    Case {
        /// 题目 id（可省略用当前题目）
        id: Option<String>,
        /// 输入文件
        #[arg(long = "in")]
        input: PathBuf,
        /// 期望输出文件（缺省则按可运行性 smoke）
        #[arg(long = "out")]
        output: Option<PathBuf>,
        /// 用例名（默认自动编号）
        #[arg(long)]
        name: Option<String>,
    },
    /// 在文件管理器中打开题目目录（改 tests/、看 stress/hack_*）
    Open {
        /// 题目 id（省略用当前题目）
        id: Option<String>,
    },
    /// 快速运行：编译源文件，stdin 透传跑一次（不需要题目）
    Run {
        /// 源文件（.cpp/.py）
        file: PathBuf,
    },
    /// 对拍：gen 生成器 × user 解 × oracle 标程
    Stress {
        /// 题目 id
        id: Option<String>,
        /// 生成器源文件（默认 gen/gen.*）
        #[arg(long = "gen")]
        generator: Option<PathBuf>,
        /// 标程源文件（默认 solutions/brute.*）
        #[arg(long)]
        oracle: Option<PathBuf>,
        /// 用户解（默认 solutions/main.*）
        #[arg(long)]
        file: Option<PathBuf>,
        /// 轮数
        #[arg(short = 'n', long, default_value = "100")]
        rounds: u32,
        /// 指定起始 seed（配合 -n 1 即 replay）
        #[arg(long)]
        seed: Option<u64>,
        /// 并行轮数
        #[arg(short = 'j', long, default_value = "4")]
        jobs: usize,
        /// 发现 hack 后继续跑
        #[arg(long)]
        keep_going: bool,
        /// AI 自动生成 gen+oracle（需配置 api_key），生成后自验再对拍
        #[arg(long)]
        auto: bool,
        /// AI 脚本语言（默认跟用户解一致）
        #[arg(long)]
        auto_lang: Option<String>,
    },
    /// 批量生成测试用例（gen + oracle → tests/）
    Gen {
        /// 题目 id
        id: Option<String>,
        /// 生成器源文件（默认 gen/gen.*）
        #[arg(long = "gen")]
        generator: Option<PathBuf>,
        /// 标程源文件（默认 solutions/brute.*）
        #[arg(long)]
        oracle: Option<PathBuf>,
        /// 生成几组
        #[arg(short = 'n', long, default_value = "20")]
        count: u32,
        /// 起始 seed（默认随机）
        #[arg(long)]
        seed: Option<u64>,
        /// AI 自动生成 gen+oracle
        #[arg(long)]
        auto: bool,
    },
    /// AI 提示/诊断（需配置 api_key）
    Hint {
        /// 题目 id
        id: Option<String>,
        /// 源文件（默认 solutions/main.* 或 watch 文件）
        #[arg(long)]
        file: Option<PathBuf>,
        /// 完整诊断（默认只给提示）
        #[arg(long)]
        explain: bool,
    },
    /// 环境初始化：写 Dev-C++ 自定义工具 / 打印集成说明
    Setup,
}

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();
    let root = cli.root.unwrap_or_else(store::Workspace::default_dir);
    let ws = store::Workspace::open(&root)?;
    init_logging(&ws);
    let cfg = store::Config::load(&ws.config_path());

    match cli.cmd {
        None => launch_gui_or_help(&root),
        Some(Cmd::Doctor) => cmd_doctor(&ws, &cfg),
        Some(Cmd::List) => cmd_list(&ws)?,
        Some(Cmd::New { title, slug }) => cmd_new(&ws, &title, slug)?,
        Some(Cmd::Test { id, file, jobs }) => cmd_test(&ws, &cfg, id, file, jobs).await?,
        Some(Cmd::Add { url }) => cmd_add(&ws, &url).await?,
        Some(Cmd::Case { id, input, output, name }) => cmd_case(&ws, id, input, output, name)?,
        Some(Cmd::Open { id }) => cmd_open(&ws, id)?,
        Some(Cmd::Run { file }) => cmd_run(&ws, &cfg, &file).await?,
        Some(Cmd::Stress { id, generator, oracle, file, rounds, seed, jobs, keep_going, auto, auto_lang }) => {
            cmd_stress(&ws, &cfg, id, generator, oracle, file, rounds, seed, jobs, keep_going, auto, auto_lang).await?
        }
        Some(Cmd::Gen { id, generator, oracle, count, seed, auto }) => {
            cmd_gen(&ws, &cfg, id, generator, oracle, count, seed, auto).await?
        }
        Some(Cmd::Hint { id, file, explain }) => cmd_hint(&ws, &cfg, id, file, explain).await?,
        Some(Cmd::Setup) => cmd_setup(),
    }
    Ok(())
}

/// 无参启动：拉起 exe 同目录的 shuati（GUI 主入口）；找不到或拉起失败才打帮助。
/// spawn 后立刻返回（GUI 是独立进程），stdio 置空让终端立刻可用；--root 透传。
fn launch_gui_or_help(root: &std::path::Path) {
    let gui = std::env::current_exe()
        .ok()
        .and_then(|e| e.parent().map(|d| d.join(format!("shuati{}", std::env::consts::EXE_SUFFIX))))
        .filter(|p| p.is_file());
    let launched = gui.is_some_and(|g| {
        let mut c = std::process::Command::new(&g);
        c.args(["--root".as_ref(), root.as_os_str()])
            .stdin(std::process::Stdio::null())
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null());
        c.spawn().is_ok()
    });
    if !launched {
        eprintln!("shuati v2 — 本地评测小助手");
        eprintln!("用法: shuati-cli <命令>（--help 查看全部）");
        eprintln!("GUI: 运行 shuati");
    }
}

/// stderr + 滚动文件双写日志（<root>/logs/shuati.log）
fn init_logging(ws: &store::Workspace) {
    use tracing_subscriber::fmt::writer::MakeWriterExt;
    let appender = tracing_appender::rolling::daily(ws.root.join("logs"), "shuati.log");
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .with_writer(std::io::stderr.and(appender))
        .with_ansi(false)
        .init();
}

/// 快速运行：编译 file → stdin 透传 → stdout/stderr 直接输出
async fn cmd_run(ws: &store::Workspace, cfg: &store::Config, file: &Path) -> Result<()> {
    let lang = Lang::from_path(file).context("不支持的文件类型（.cpp/.py）")?;
    let tc_cfg = pick_tc(lang, cfg);
    let tc = judge::Toolchain::detect(lang, tc_cfg)?;
    println!("[*] 编译 {} …", file.display());
    let prog = judge::compile_cached(&SourceFile { path: file.to_path_buf(), lang }, &tc, &ws.cache_dir()).await?;

    let tmp = tempfile::tempdir()?;
    // 管道输入 → 读到临时文件喂给程序；TTY（交互终端）→ 直接给 EOF（并提示）
    let stdin = {
        use std::io::IsTerminal;
        if std::io::stdin().is_terminal() {
            eprintln!("提示: 程序拿不到键盘输入（stdin=EOF）。要喂数据用管道：`shuati run --file x.cpp < data.in`");
            None
        } else {
            use std::io::Read;
            let mut buf = String::new();
            std::io::stdin().read_to_string(&mut buf)?;
            let p = tmp.path().join("stdin");
            std::fs::write(&p, buf)?;
            Some(p)
        }
    };
    let io = judge::Io { stdin, stdout: tmp.path().join("stdout"), stderr: tmp.path().join("stderr") };
    let limits = shuati_core::Limits { cpu_ms: 5000, wall_ms: 15000, memory_mb: 512, output_mb: 16 };
    let out = judge::run_program(&prog, &io, &limits, None).await;
    judge::cleanup_program(&prog);
    let out = out?;

    // stdout 原样输出到 stdout，stderr 到 stderr
    let stdout = std::fs::read_to_string(&out.stdout_path).unwrap_or_default();
    let stderr = std::fs::read_to_string(&out.stderr_path).unwrap_or_default();
    print!("{stdout}");
    if !stderr.is_empty() {
        eprintln!("--- stderr ---\n{stderr}");
    }
    let status = match out.status {
        judge::RunStatus::Ok => format!("运行结束 · {}ms · {}KB", out.cpu_ms, out.peak_mem_kb),
        judge::RunStatus::TimeLimit => format!("TLE · {}ms", out.cpu_ms),
        judge::RunStatus::MemoryLimit => format!("MLE · {}KB", out.peak_mem_kb),
        judge::RunStatus::OutputLimit => "OLE · 输出超限".into(),
        judge::RunStatus::Cancelled => "已取消".into(),
        judge::RunStatus::RuntimeError => format!("RE · exit {}", out.exit_code),
        judge::RunStatus::InternalError => format!("SE · {}", out.internal),
    };
    eprintln!("[{status}]");
    Ok(())
}

/// AI 自动管线：生成 gen+oracle 并验证，落盘到题目目录
async fn auto_produce(
    pd: &store::ProblemDir,
    cfg: &store::Config,
    lang_hint: Option<&str>,
) -> Result<(PathBuf, PathBuf)> {
    let ai = &cfg.ai;
    let key = ai.resolved_key().context("AI 未配置：设置页填 api_key，或设环境变量 SHUATI_API_KEY")?;
    let client = shuati_ai::AiClient::new(&ai.api_base, &key, &ai.model)?;
    let statement = pd.read_statement().unwrap_or_default();
    let lang = match lang_hint.unwrap_or(&cfg.default_lang) {
        "python" | "py" => Lang::Python,
        _ => Lang::Cpp,
    };
    println!("[*] AI 正在生成对拍脚本（{lang:?}），自动 dry-run 验证…");
    // 落 gen/ 和 solutions/——与 find_script 的探测目录一致，下次不再重生成
    let gen_dir = pd.gen_dir();
    let oracle_dir = pd.solutions_dir();
    let dirs = shuati_ai::ScriptDirs { generator: &gen_dir, oracle: &oracle_dir };
    let (gen_path, oracle_path) =
        shuati_ai::produce_scripts(&client, &statement, lang, dirs, &cfg.compiler_cpp, &cfg.compiler_python, |s| {
            println!("    {s}")
        })
        .await?;
    println!("    gen    → {}", gen_path.display());
    println!("    oracle → {}", oracle_path.display());
    Ok((gen_path, oracle_path))
}

#[allow(clippy::too_many_arguments)]
async fn cmd_gen(
    ws: &store::Workspace,
    cfg: &store::Config,
    id: Option<String>,
    generator: Option<PathBuf>,
    oracle: Option<PathBuf>,
    count: u32,
    seed: Option<u64>,
    auto: bool,
) -> Result<()> {
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目"),
    };

    let (gen_path, oracle_path) = if auto {
        auto_produce(pd, cfg, None).await?
    } else {
        let g = generator.or_else(|| judge::find_script(&pd.gen_dir(), &["gen"]));
        let o = oracle.or_else(|| judge::find_script(&pd.solutions_dir(), &["brute", "oracle", "std"]));
        (
            g.ok_or_else(|| anyhow::anyhow!("找不到生成器（放 gen/gen.* 或用 --auto 让 AI 写）"))?,
            o.ok_or_else(|| anyhow::anyhow!("找不到标程（放 solutions/brute.* 或用 --auto）"))?,
        )
    };

    let mk_src = |p: &PathBuf| -> Result<SourceFile> {
        let lang = Lang::from_path(p).context("不支持的类型")?;
        Ok(SourceFile { path: resolve_rel(&pd.dir, p), lang })
    };
    let gen_src = mk_src(&gen_path)?;
    let oracle_src = mk_src(&oracle_path)?;

    let gen_tc = judge::Toolchain::detect(gen_src.lang, pick_tc(gen_src.lang, cfg))?;
    let oracle_tc = judge::Toolchain::detect(oracle_src.lang, pick_tc(oracle_src.lang, cfg))?;
    let gen_prog = judge::compile_cached(&gen_src, &gen_tc, &ws.cache_dir()).await?;
    let oracle_prog = judge::compile_cached(&oracle_src, &oracle_tc, &ws.cache_dir()).await?;

    let seed = seed.unwrap_or_else(|| (store::now_ts() as u64 % 1_000_000) + 1);
    println!("[*] 生成 {} 组用例（seed {}..{}）…", count, seed, seed + count as u64 - 1);
    let glim = shuati_core::Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 64 };
    let seeds = seed..seed + count as u64;
    let batch = judge::gen_cases(&gen_prog, &oracle_prog, seeds, &pd.meta.limits, &glim).await;
    judge::cleanup_program(&gen_prog);
    judge::cleanup_program(&oracle_prog);

    // 失败明细不静默——报出来让用户知道哪几个 seed 没产出
    for (s, reason) in &batch.failed {
        eprintln!("  [seed {s} 失败] {reason}");
    }
    for (i, (s, input, output)) in batch.cases.iter().enumerate() {
        let name = pd.next_case_name();
        pd.write_case(&name, input, Some(output), shuati_core::CaseOrigin::Generated)?;
        eprint!("\r  {}/{} (seed={})", i + 1, batch.cases.len(), s);
    }
    eprintln!();
    println!(
        "[+] 生成 {} 组 → {}/tests/{}",
        batch.cases.len(),
        pd.dir.display(),
        if batch.failed.is_empty() { String::new() } else { format!("（{} 个 seed 失败）", batch.failed.len()) }
    );
    Ok(())
}

/// 相对路径优先按题目目录解析；不存在则按 CWD 原样使用
fn resolve_rel(problem_dir: &std::path::Path, p: &PathBuf) -> PathBuf {
    if p.is_absolute() {
        return p.clone();
    }
    let cand = problem_dir.join(p);
    if cand.exists() { cand } else { p.clone() }
}

fn pick_tc(lang: Lang, cfg: &store::Config) -> &str {
    match lang {
        Lang::Cpp => &cfg.compiler_cpp,
        Lang::Python => &cfg.compiler_python,
    }
}

async fn cmd_hint(
    ws: &store::Workspace,
    cfg: &store::Config,
    id: Option<String>,
    file: Option<PathBuf>,
    explain: bool,
) -> Result<()> {
    let ai = &cfg.ai;
    let Some(api_key) = ai.resolved_key() else {
        anyhow::bail!("AI 未配置：设置页填 api_key，或设环境变量 SHUATI_API_KEY");
    };
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目"),
    };
    let src = file.or_else(|| pd.find_solution());
    let Some(src) = src else { anyhow::bail!("没有源文件——用 --file 指定") };
    let code = std::fs::read_to_string(&src)?;
    let statement = pd.read_statement().unwrap_or_default();

    // 拿最近一次报告当判题上下文
    let verdict_info = {
        let reports_dir = pd.reports_dir();
        let mut latest: Option<shuati_core::TestReport> = None;
        if let Ok(entries) = std::fs::read_dir(&reports_dir) {
            let mut paths: Vec<PathBuf> = entries
                .filter_map(|e| e.ok().map(|x| x.path()))
                .filter(|p| p.extension().is_some_and(|x| x == "json"))
                .collect();
            paths.sort();
            if let Some(p) = paths.last()
                && let Ok(t) = std::fs::read_to_string(p)
            {
                latest = serde_json::from_str(&t).ok();
            }
        }
        match latest {
            Some(r) => {
                let fails: Vec<String> = r
                    .cases
                    .iter()
                    .filter(|c| c.verdict != shuati_core::Verdict::Ac)
                    .map(|c| {
                        format!(
                            "{}: {} （期望 {} 实际 {}）",
                            c.name,
                            c.verdict,
                            c.expected.lines().next().unwrap_or(""),
                            c.actual.lines().next().unwrap_or("")
                        )
                    })
                    .take(3)
                    .collect();
                // explain 需要失败用例的 IO 细节，取第一个失败点
                let io_detail = r
                    .cases
                    .iter()
                    .find(|c| c.verdict != shuati_core::Verdict::Ac)
                    .map(|c| {
                        format!("用例 {}:\n输入:\n{}\n期望:\n{}\n实际:\n{}", c.name, c.input, c.expected, c.actual)
                    })
                    .unwrap_or_default();
                (format!("总判定 {}，{}/{}。失败点:\n{}", r.verdict, r.pass, r.total, fails.join("\n")), io_detail)
            }
            None => ("（尚未评测过）".into(), String::new()),
        }
    };

    let client = shuati_ai::AiClient::new(&ai.api_base, &api_key, &ai.model)?;
    let (verdict_info, io_detail) = verdict_info;
    // SSE 流式：逐段打到 stderr（stdout 保持干净可管道）；服务端不吐流时退普通 chat
    use std::io::Write;
    let mut print_delta = |d: &str| {
        eprint!("{d}");
        let _ = std::io::stderr().flush();
    };
    eprintln!("[*] AI 思考中…\n");
    // 流式中途失败才回退：已打印的半截 + 完整回复一起呈现，不会没下文
    if explain {
        if client.explain_stream(&statement, &code, &verdict_info, &io_detail, &mut print_delta).await.is_err() {
            eprintln!("{}", client.explain(&statement, &code, &verdict_info, &io_detail).await?);
        }
    } else if client.hint_stream(&statement, &code, &verdict_info, &mut print_delta).await.is_err() {
        eprintln!("{}", client.hint(&statement, &code, &verdict_info).await?);
    }
    eprintln!();
    Ok(())
}

fn cmd_setup() {
    println!("== shuati 集成设置 ==");
    #[cfg(target_os = "windows")]
    {
        // 写 Dev-C++ Tools 配置 + 资源管理器右键
        match setup_windows() {
            Ok(msgs) => {
                for m in msgs {
                    println!("  {m}");
                }
            }
            Err(e) => println!("  自动配置失败: {e}\n请按下面手动配置:"),
        }
    }
    println!();
    println!("Dev-C++ 集成（手动方式，任何平台通用思路）:");
    println!("  1. Dev-C++ → 工具 → 配置工具 → 添加");
    println!("     标题:    shuati 评测");
    println!("     程序:    <Dev-C++ 安装目录>\\ConsolePauser.exe");
    println!("     工作目录: <SOURCEPATH>");
    println!("     参数:    \"<shuati.exe 路径>\" test --file \"<SOURCENAME>\"");
    println!("  2. 工具菜单里就会出现一键评测当前文件（ConsolePauser 让结果窗口停留）");
    println!();
    println!("或者：GUI 里打开『保存即评测』，Dev-C++ 里 Ctrl+S 即触发判题（推荐萌新）");
}

/// 一条 Dev-C++ 自定义工具项（对应 tools.ini 的 [ToolN] 段）
#[cfg(any(target_os = "windows", test))]
struct DevCppTool {
    title: String,
    program: String,
    workdir: String,
    params: String,
}

/// 往 Dev-C++ tools.ini 里 upsert shuati 工具项（纯函数，幂等）。
/// 格式（Embarcadero 6.x / Orwell 5.x 一致）：
///   [Tools] Count=N   [ToolK] Title=/Program=/WorkDir=/Params=
#[cfg(any(target_os = "windows", test))]
fn upsert_devcpp_tools(content: &str, tools: &[DevCppTool]) -> String {
    // 切成 (段名, 行列表)；段名来自 [header]，段外的行归 preamble
    let mut preamble: Vec<String> = Vec::new();
    let mut sections: Vec<(String, Vec<String>)> = Vec::new();
    for line in content.lines() {
        let t = line.trim();
        if t.starts_with('[') && t.ends_with(']') && t.len() > 2 {
            sections.push((t[1..t.len() - 1].to_string(), vec![line.to_string()]));
        } else if let Some(last) = sections.last_mut() {
            last.1.push(line.to_string());
        } else {
            preamble.push(line.to_string());
        }
    }

    // 现有 shuati 工具段名集合（Title/Params 含 shuati，大小写不敏感）
    let is_ours = |lines: &[String]| {
        lines.iter().any(|l| {
            let l = l.trim();
            (l.to_ascii_lowercase().starts_with("title=") || l.to_ascii_lowercase().starts_with("params="))
                && l.to_ascii_lowercase().contains("shuati")
        })
    };

    // Count = [Tools].Count 或 ToolN 段数兜底
    let mut count: usize = sections
        .iter()
        .find(|(n, _)| n.eq_ignore_ascii_case("tools"))
        .and_then(|(_, ls)| {
            ls.iter().find_map(|l| {
                l.trim()
                    .split_once('=')
                    .filter(|(k, _)| k.trim().eq_ignore_ascii_case("count"))
                    .and_then(|(_, v)| v.trim().parse().ok())
            })
        })
        .unwrap_or_else(|| {
            sections
                .iter()
                .filter(|(n, _)| n.to_ascii_lowercase().starts_with("tool") && !n.eq_ignore_ascii_case("tools"))
                .count()
        });

    // 已存在的 shuati 段：更新第一个，多余的删掉；不够就新开 Tool{count} 段
    let mut updated: Vec<String> = Vec::new();
    for tool in tools {
        let sec_name = sections
            .iter()
            .find(|(n, l)| {
                n.to_ascii_lowercase().starts_with("tool")
                    && !n.eq_ignore_ascii_case("tools")
                    && !updated.contains(n)
                    && is_ours(l)
            })
            .map(|(n, _)| n.clone())
            .unwrap_or_else(|| {
                let n = format!("Tool{count}");
                count += 1;
                n
            });
        let body = vec![
            format!("[{sec_name}]"),
            format!("Title={}", tool.title),
            format!("Program={}", tool.program),
            format!("WorkDir={}", tool.workdir),
            format!("Params={}", tool.params),
        ];
        if let Some(slot) = sections.iter_mut().find(|(n, _)| *n == sec_name) {
            slot.1 = body;
        } else {
            sections.push((sec_name.clone(), body));
        }
        updated.push(sec_name);
    }
    // 没被复用的旧 shuati 段（比如上次装过、这次工具变少）直接抹掉
    sections.retain(|(n, l)| {
        !(n.to_ascii_lowercase().starts_with("tool")
            && !n.eq_ignore_ascii_case("tools")
            && is_ours(l)
            && !updated.contains(n))
    });

    // Dev-C++ 按 Tool0..Count-1 连续读——删段后统一重编号，顺带压实用户手动删除留下的洞
    let mut idx = 0usize;
    for (name, lines) in sections.iter_mut() {
        let ln = name.to_ascii_lowercase();
        if ln.starts_with("tool") && ln.len() > 4 && ln[4..].bytes().all(|b| b.is_ascii_digit()) {
            let new_name = format!("Tool{idx}");
            if let Some(h) = lines.first_mut() {
                *h = format!("[{new_name}]");
            }
            *name = new_name;
            idx += 1;
        }
    }
    let count = idx;

    // 写回 [Tools] Count
    let count_line = format!("Count={count}");
    if let Some((_, ls)) = sections.iter_mut().find(|(n, _)| n.eq_ignore_ascii_case("tools")) {
        if let Some(slot) = ls
            .iter_mut()
            .find(|l| l.trim().split_once('=').is_some_and(|(k, _)| k.trim().eq_ignore_ascii_case("count")))
        {
            *slot = count_line;
        } else {
            ls.push(count_line);
        }
    } else {
        sections.insert(0, ("Tools".into(), vec!["[Tools]".into(), count_line]));
    }

    let mut out = String::new();
    for l in preamble.iter().chain(sections.iter().flat_map(|(_, ls)| ls.iter())) {
        out.push_str(l);
        out.push_str("\r\n");
    }
    out
}

#[cfg(target_os = "windows")]
fn setup_windows() -> Result<Vec<String>> {
    let exe = std::env::current_exe()?.to_string_lossy().into_owned();
    let gui_exe = std::env::current_exe()?
        .parent()
        .map(|p| p.join("shuati.exe"))
        .filter(|p| p.exists())
        .map(|p| p.to_string_lossy().into_owned());

    let mut tools = vec![
        // ConsolePauser 包一层：跑完暂停，否则控制台一闪就关
        DevCppTool {
            title: "shuati 评测".into(),
            program: "<EXECPATH>\\ConsolePauser.exe".into(),
            workdir: "<SOURCEPATH>".into(),
            params: format!("\"{exe}\" test --file \"<SOURCENAME>\""),
        },
        DevCppTool {
            title: "shuati 对拍".into(),
            program: "<EXECPATH>\\ConsolePauser.exe".into(),
            workdir: "<SOURCEPATH>".into(),
            params: format!("\"{exe}\" stress --file \"<SOURCENAME>\" -n 50"),
        },
    ];
    if let Some(g) = gui_exe {
        tools.push(DevCppTool {
            title: "shuati 面板".into(),
            program: g,
            workdir: String::new(),
            params: String::new(),
        });
    }

    let mut msgs = Vec::new();
    let mut wrote = false;
    for dir in devcpp_config_dirs() {
        let ini = dir.join("tools.ini");
        let old = std::fs::read_to_string(&ini).unwrap_or_default();
        let new = upsert_devcpp_tools(&old, &tools);
        store::atomic_write(&ini, new.as_bytes())?;
        msgs.push(format!("已写入 {}", ini.display()));
        wrote = true;
    }
    if !wrote {
        msgs.push("没找到 Dev-C++ 配置目录（%APPDATA%\\Dev-Cpp 等），请按下方手动步骤".into());
    }

    // §7.3：.cpp 右键菜单「用 shuati 评测」（HKCU 免管理员，可逆）
    println!();
    println!("资源管理器右键菜单：.cpp 上右键可直接「用 shuati 评测」");
    if ask_yes_no("要现在添加吗？[Y/n] ") {
        match add_context_menu(&exe, gui_exe.as_deref()) {
            Ok(()) => msgs.push("右键菜单已添加（HKCU\\…\\SystemFileAssociations\\.cpp\\shell\\shuati）".into()),
            Err(e) => msgs.push(format!("右键菜单写入失败: {e}")),
        }
    }
    Ok(msgs)
}

#[cfg(target_os = "windows")]
fn ask_yes_no(prompt: &str) -> bool {
    use std::io::Write;
    print!("{prompt}");
    let _ = std::io::stdout().flush();
    let mut line = String::new();
    if std::io::stdin().read_line(&mut line).is_err() {
        return false;
    }
    !matches!(line.trim().to_lowercase().as_str(), "n" | "no")
}

/// .cpp 右键 → `shuati-cli test --file "%1"`；cmd /c + pause 防结果窗一闪就关。
/// SystemFileAssociations 是官方兜底路径，跟 .cpp 是否已有 ProgID 关联无关。
#[cfg(target_os = "windows")]
fn add_context_menu(cli_exe: &str, gui_exe: Option<&str>) -> Result<()> {
    let icon = gui_exe.unwrap_or(cli_exe);
    let base = r"HKCU\Software\Classes\SystemFileAssociations\.cpp\shell\shuati";
    let run = format!("cmd.exe /c \"\"{cli_exe}\" test --file \"%1\" & pause\"");
    let steps: [(String, Vec<String>, String); 3] = [
        (base.into(), vec!["/ve".into()], "用 shuati 评测".into()),
        (base.into(), vec!["/v".into(), "Icon".into()], icon.into()),
        (format!("{base}\\command"), vec!["/ve".into()], run),
    ];
    for (key, flags, data) in &steps {
        let out =
            std::process::Command::new("reg").arg("add").arg(key).args(flags).arg("/d").arg(data).arg("/f").output()?;
        if !out.status.success() {
            anyhow::bail!("reg add {key}: {}", String::from_utf8_lossy(&out.stderr));
        }
    }
    Ok(())
}

/// Dev-C++ 各版本的配置目录候选（只返回真实存在的）
#[cfg(target_os = "windows")]
fn devcpp_config_dirs() -> Vec<PathBuf> {
    let mut dirs: Vec<PathBuf> = Vec::new();
    let mut push = |d: PathBuf| {
        if d.is_dir() && !dirs.contains(&d) {
            dirs.push(d);
        }
    };
    if let Some(a) = std::env::var_os("APPDATA") {
        let a = PathBuf::from(a);
        push(a.join("Embarcadero\\Dev-Cpp")); // Dev-C++ 6.x
        push(a.join("Dev-Cpp")); // Orwell 5.x
    }
    // 便携模式：<安装目录>\config
    for env in ["ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"] {
        if let Some(p) = std::env::var_os(env) {
            let p = PathBuf::from(p);
            push(p.join("Dev-Cpp\\config"));
            push(p.join("Embarcadero\\Dev-Cpp\\config"));
        }
    }
    push(PathBuf::from("C:\\Dev-Cpp\\config"));
    dirs
}

async fn cmd_add(ws: &store::Workspace, url: &str) -> Result<()> {
    println!("[*] 正在拉取 {url} …");
    let pd = shuati_fetch::fetch_to_dir(url, ws).await?;
    println!("[+] {}  {}", pd.meta.id, pd.meta.title);
    let cases = pd.load_cases()?;
    println!("    样例 {} 组 → {}", cases.len(), pd.dir.display());
    if cases.is_empty() {
        eprintln!("    注意：没拉到样例（题面抓取被拦？）。评测页下方/statement.md 可手动补。");
    }
    // 设为当前题目
    let mut state = store::AppState::load(&ws.state_path());
    state.current_problem = Some(pd.meta.id.to_string());
    state.save(&ws.state_path())?;
    Ok(())
}

/// `shuati case <id> --in f --out f`：把文件内容存成 tests/ 用例
fn cmd_case(
    ws: &store::Workspace,
    id: Option<String>,
    input: PathBuf,
    output: Option<PathBuf>,
    name: Option<String>,
) -> Result<()> {
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目（用 `shuati list` 查看，或先 `shuati new`）"),
    };
    let input = std::fs::read_to_string(&input).with_context(|| format!("读输入文件失败: {}", input.display()))?;
    let output = output
        .map(|p| std::fs::read_to_string(&p).with_context(|| format!("读输出文件失败: {}", p.display())))
        .transpose()?;
    let name = name.unwrap_or_else(|| pd.next_case_name());
    pd.write_case(&name, &input, output.as_deref(), shuati_core::CaseOrigin::Sample)?;
    println!("[+] 用例 {name} → {}", pd.dir.join("tests").display());
    Ok(())
}

/// `shuati open [id]`：文件管理器打开题目目录
fn cmd_open(ws: &store::Workspace, id: Option<String>) -> Result<()> {
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目"),
    };
    #[cfg(target_os = "macos")]
    let opener = "open";
    #[cfg(target_os = "windows")]
    let opener = "explorer";
    #[cfg(all(unix, not(target_os = "macos")))]
    let opener = "xdg-open";
    let _ = std::process::Command::new(opener).arg(&pd.dir).spawn();
    println!("{}", pd.dir.display());
    Ok(())
}

#[allow(clippy::too_many_arguments)]
async fn cmd_stress(
    ws: &store::Workspace,
    cfg: &store::Config,
    id: Option<String>,
    generator: Option<PathBuf>,
    oracle: Option<PathBuf>,
    file: Option<PathBuf>,
    rounds: u32,
    seed: Option<u64>,
    jobs: usize,
    keep_going: bool,
    auto: bool,
    auto_lang: Option<String>,
) -> Result<()> {
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目"),
    };

    let user_path = match file {
        Some(f) => f,
        None => pd.find_solution().context("solutions/ 下没有源文件")?,
    };
    let (gen_path, oracle_path) = if auto {
        auto_produce(pd, cfg, auto_lang.as_deref()).await?
    } else {
        let g = match generator {
            Some(g) => g,
            None => judge::find_script(&pd.gen_dir(), &["gen"])
                .context("找不到生成器（放 gen/gen.*、--gen 指定或 --auto 让 AI 写）")?,
        };
        let o = match oracle {
            Some(o) => o,
            None => judge::find_script(&pd.solutions_dir(), &["brute", "oracle", "std"])
                .context("找不到标程（放 solutions/brute.*、--oracle 指定或 --auto）")?,
        };
        (g, o)
    };

    let mk_src = |p: &PathBuf| -> Result<SourceFile> {
        let lang = Lang::from_path(p).context(format!("不支持类型: {}", p.display()))?;
        Ok(SourceFile { path: resolve_rel(&pd.dir, p), lang })
    };
    let gen_src = mk_src(&gen_path)?;
    let user_src = mk_src(&user_path)?;
    let oracle_src = mk_src(&oracle_path)?;

    // 没指定 seed 时随机给一个，否则每次对拍的都是同一批数据
    let seed_start = seed.unwrap_or_else(|| (store::now_ts() as u64 % 1_000_000) + 1);
    println!("[*] 对拍 {} · {} 轮 · seed={}", pd.meta.id, rounds, seed_start);
    println!("    gen    = {}", gen_path.display());
    println!("    user   = {}", user_path.display());
    println!("    oracle = {}", oracle_path.display());

    let ev_count = std::sync::Arc::new(std::sync::atomic::AtomicU32::new(0));
    let cnt = ev_count.clone();
    let progress = std::sync::Arc::new(move |ev: judge::StressEvent| {
        use judge::StressEvent::*;
        let n = cnt.fetch_add(1, std::sync::atomic::Ordering::Relaxed) + 1;
        match ev {
            Match { seed, .. } => eprint!("\r  round {n} seed={seed} ✓        "),
            Hack { seed, case, .. } => eprintln!("\n  [HACK] seed={seed} kind={}", case.kind),
            GenFail { seed, msg, .. } => eprintln!("\n  [gen 异常] seed={seed}: {msg}"),
            OracleFail { seed, msg, .. } => eprintln!("\n  [标程异常] seed={seed}: {msg}"),
        }
    });

    let gen_limits = shuati_core::Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 64 };
    let params = judge::StressParams {
        gen_src: &gen_src,
        user_src: &user_src,
        oracle_src: &oracle_src,
        rounds,
        seed_start,
        jobs,
        limits_run: &pd.meta.limits,
        limits_gen: &gen_limits,
        checker: &pd.meta.checker,
        stop_on_fail: !keep_going,
        progress: Some(progress),
        cancel: None,
    };

    let engine = judge::StressEngine::new(&params, &cfg.compiler_cpp, &cfg.compiler_python)?.with_cache(ws.cache_dir());
    let report = engine.run(&params).await?;
    eprintln!();

    // hack 落盘
    for hack in &report.hacks {
        let path = pd.save_hack(
            hack.seed,
            &hack.input,
            &hack.user_out,
            &hack.oracle_out,
            &format!("{}: {}", hack.kind, hack.detail),
        )?;
        println!("  hack 已保存 → {}", path.display());
    }

    println!(
        "[结果] {} 轮 · ✓{} 一致 · hack {} · gen异常 {} · 标程异常 {}",
        report.rounds_done,
        report.matched,
        report.hacks.len(),
        report.gen_failures,
        report.oracle_failures
    );
    if !report.hacks.is_empty() {
        std::process::exit(1);
    }
    Ok(())
}

fn cmd_doctor(ws: &store::Workspace, cfg: &store::Config) {
    let rep = judge::check_env(&cfg.compiler_cpp, &cfg.compiler_python);
    println!("== 环境自检 ==");
    match &rep.cpp {
        Some(p) => println!("  g++:    {} ✓", p.display()),
        None => println!("  g++:    未找到（装 Dev-C++/MinGW 后在设置页填路径）"),
    }
    match &rep.python {
        Some(p) => println!("  python: {} ✓", p.display()),
        None => println!("  python: 未找到（只用 C++ 可忽略）"),
    }
    let ai_note = match store::secrets::key_source(&cfg.ai.api_key) {
        store::secrets::KeySource::Env => "已配置（环境变量 SHUATI_API_KEY）✓",
        store::secrets::KeySource::Keyring => "已配置（系统钥匙串）✓",
        store::secrets::KeySource::File => "已配置（config.toml 明文）✓",
        store::secrets::KeySource::None => "未配置（hint/自动脚本不可用）",
    };
    println!("  AI:     {ai_note}");
    println!("  题库:   {}", ws.root.display());
    println!("  日志:   {}", ws.root.join("logs").display());
}

fn cmd_list(ws: &store::Workspace) -> Result<()> {
    let problems = store::load_all_problems(ws);
    if problems.is_empty() {
        println!("（题库为空）使用 `shuati new <标题>` 创建本地题，或 GUI 题库页拉取");
        return Ok(());
    }
    let state = store::AppState::load(&ws.state_path());
    for p in &problems {
        let id = p.meta.id.to_string();
        let n = p.count_cases();
        let last = state.last_verdicts.get(&id).map(|s| s.as_str()).unwrap_or("-");
        let cur = if state.current_problem.as_deref() == Some(id.as_str()) { "*" } else { " " };
        println!("{cur} {id:22} 上次:{last:4} {n:3}组  {}", p.meta.title);
    }
    Ok(())
}

fn cmd_new(ws: &store::Workspace, title: &str, slug: Option<String>) -> Result<()> {
    let slug = slug.unwrap_or_else(|| title.chars().filter(|c| c.is_alphanumeric()).collect::<String>());
    let slug = if slug.is_empty() { format!("{}", store::now_ts()) } else { slug };
    let id = ProblemId::new(shuati_core::Source::Local, &slug);
    let dir = ws.problem_dir(&id);
    let meta = shuati_core::Problem {
        id: id.clone(),
        title: title.to_string(),
        url: String::new(),
        difficulty: None,
        tags: vec![],
        limits: Default::default(),
        statement: PathBuf::from("statement.md"),
        checker: Default::default(),
        created_at: store::now_ts(),
    };
    store::ProblemDir::create(&dir, &meta)?;
    println!("[+] 已创建 {} → {}", id, dir.display());
    Ok(())
}

async fn cmd_test(
    ws: &store::Workspace,
    cfg: &store::Config,
    id: Option<String>,
    file: Option<PathBuf>,
    jobs: usize,
) -> Result<()> {
    // 解析题目：参数 > --file 绑定注释 > state.current > 唯一题目
    let problems = store::load_all_problems(ws);
    let mut bound_hit = None;
    if id.is_none()
        && let Some(f) = &file
        && let Some(bound) = judge::binding_comment(f)
    {
        bound_hit = problems.iter().find(|p| p.meta.id == bound);
        if bound_hit.is_some() {
            println!("[*] 源文件绑定 {bound}（`// shuati:` 注释）");
        }
    }
    let mut pd = match bound_hit.or(resolve_problem(&problems, id.as_deref(), ws)?) {
        Some(p) => p,
        None => anyhow::bail!("找不到题目（用 `shuati list` 查看，或先 `shuati new`）"),
    };

    let src_path = match file {
        Some(f) => f,
        None => pd.find_solution().context("solutions/ 下没有源文件")?,
    };
    // solutions/ 下发现的源文件也过一遍绑定注释（与已选不同时显式切题）
    if id.is_none()
        && let Some(bound) = judge::binding_comment(&src_path)
        && let Some(p) = problems.iter().find(|p| p.meta.id == bound)
        && p.meta.id != pd.meta.id
    {
        println!("[*] 源文件绑定 {}（`// shuati:` 注释）", bound);
        pd = p;
    }
    let lang = Lang::from_path(&src_path).context("不支持的文件类型（需 .cpp/.py）")?;

    let cases = pd.load_cases()?;
    if cases.is_empty() {
        anyhow::bail!("tests/ 下没有用例（放 *.in/*.out，或对拍生成）");
    }

    println!("[*] 题目   : {} {}", pd.meta.id, pd.meta.title);
    println!("[*] 源文件 : {}", src_path.display());
    println!("[*] 用例   : {} 个", cases.len());

    let toolchain = judge::Toolchain::detect(lang, cfg_cpp_for(cfg, lang))?;
    let engine = judge::TestEngine::new(toolchain).with_cache(ws.cache_dir());

    let src = SourceFile { path: src_path, lang };
    let progress: judge::ProgressFn = std::sync::Arc::new(|i, total, r| {
        eprint!("\r  [{}/{}] {} {} ({}ms)", i, total, r.name, r.verdict, r.cpu_ms);
    });
    let params = judge::RunParams {
        problem: &pd.meta.id,
        solution: &src,
        cases: &cases,
        limits: &pd.meta.limits,
        checker: &pd.meta.checker,
        jobs,
        progress: Some(progress),
        cancel: None,
    };

    let report = match engine.run(&params).await {
        Ok(r) => r,
        Err(judge::JudgeError::Compile(msg)) => {
            eprintln!("\n[Compile Error]\n{msg}");
            std::process::exit(2);
        }
        Err(e) => return Err(e.into()),
    };
    eprintln!();

    // WA 展示数量：多了刷屏，萌新只需要看第一组错在哪
    let mut shown_diff = 0;
    for c in &report.cases {
        let mark = if c.verdict == shuati_core::Verdict::Ac { "✓" } else { "✗" };
        println!("  {} {:12} {:3} {:>6}ms {:>8}KB", mark, c.name, c.verdict, c.cpu_ms, c.memory_kb);
        // 有 message 就打（AC 的 smoke 提示、WA/RE 的错误详情）
        if !c.message.is_empty() {
            let first = c.message.lines().next().unwrap_or("");
            println!("      {}", first.chars().take(80).collect::<String>());
        }
        if c.verdict == shuati_core::Verdict::Wa && shown_diff < 3 {
            shown_diff += 1;
            print_case_diff(c);
        }
    }
    if shown_diff == 3 && report.cases.iter().filter(|c| c.verdict == shuati_core::Verdict::Wa).count() > 3 {
        println!("      …（更多 WA 的 diff 见报告 json 或 GUI diff 弹窗）");
    }
    println!("[结果] {} · {}/{}", report.verdict, report.pass, report.total);

    let path = pd.save_report(&report)?;
    println!("报告: {}", path.display());

    // 记录最近结果
    let mut state = store::AppState::load(&ws.state_path());
    state.last_verdicts.insert(pd.meta.id.to_string(), report.verdict.to_string());
    state.save(&ws.state_path())?;

    if report.verdict != shuati_core::Verdict::Ac {
        std::process::exit(1);
    }
    Ok(())
}

/// WA 的期望/实际紧凑对比：单行内联，多行块式（各截断到 ~6 行/300 字符）
fn print_case_diff(c: &shuati_core::CaseResult) {
    fn clipped(s: &str) -> Vec<String> {
        const MAX_LINES: usize = 6;
        const MAX_CHARS: usize = 300;
        let over_chars = s.chars().count() > MAX_CHARS;
        let s: String = s.chars().take(MAX_CHARS).collect();
        let mut lines: Vec<String> = s.trim_end().lines().map(|l| l.to_string()).collect();
        let truncated = over_chars || lines.len() > MAX_LINES;
        lines.truncate(MAX_LINES);
        if truncated {
            lines.push("…".into());
        }
        lines
    }
    let (exp, act) = (c.expected.trim_end(), c.actual.trim_end());
    if !exp.contains('\n') && !act.contains('\n') && exp.len() <= 60 && act.len() <= 60 {
        println!("      期望: {exp} · 实际: {act}");
        return;
    }
    println!("      期望:");
    for l in clipped(exp) {
        println!("        {l}");
    }
    println!("      实际:");
    for l in clipped(act) {
        println!("        {l}");
    }
}

fn cfg_cpp_for(cfg: &store::Config, lang: Lang) -> &str {
    match lang {
        Lang::Cpp => &cfg.compiler_cpp,
        Lang::Python => &cfg.compiler_python,
    }
}

fn resolve_problem<'a>(
    problems: &'a [store::ProblemDir],
    id: Option<&str>,
    ws: &store::Workspace,
) -> Result<Option<&'a store::ProblemDir>> {
    if let Some(s) = id {
        let needle = s.to_lowercase();
        // 支持 "P1001" / "luogu:P1001" / 目录名 模糊匹配
        let hit = problems.iter().find(|p| {
            let full = p.meta.id.to_string().to_lowercase();
            full == needle || p.meta.id.slug.to_lowercase() == needle || p.meta.id.dir_name().to_lowercase() == needle
        });
        return Ok(hit);
    }
    // state.json 记录的当前题目
    let state = store::AppState::load(&ws.state_path());
    if let Some(cur) = &state.current_problem
        && let Some(p) = problems.iter().find(|p| &p.meta.id.to_string() == cur)
    {
        return Ok(Some(p));
    }
    // 只有一道题时直接用
    if problems.len() == 1 {
        return Ok(problems.first());
    }
    Ok(None)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tool(title: &str) -> DevCppTool {
        DevCppTool {
            title: title.into(),
            program: "<EXECPATH>\\ConsolePauser.exe".into(),
            workdir: "<SOURCEPATH>".into(),
            params: "\"C:\\bin\\shuati.exe\" test --file \"<SOURCENAME>\"".into(),
        }
    }

    #[test]
    fn upsert_empty_ini() {
        let out = upsert_devcpp_tools("", &[tool("shuati 评测")]);
        assert!(out.contains("[Tools]"), "{out}");
        assert!(out.contains("Count=1"), "{out}");
        assert!(out.contains("[Tool0]"), "{out}");
        assert!(out.contains("Title=shuati 评测"), "{out}");
    }

    #[test]
    fn upsert_preserves_other_tools() {
        let old = "[Tools]\r\nCount=1\r\n\r\n[Tool0]\r\nTitle=Existing\r\nProgram=C:\\x.exe\r\nWorkDir=\r\nParams=\r\n";
        let out = upsert_devcpp_tools(old, &[tool("shuati 评测")]);
        assert!(out.contains("Count=2"), "{out}");
        assert!(out.contains("Title=Existing"), "{out}");
        assert!(out.contains("[Tool1]"), "{out}");
        assert!(out.contains("Title=shuati 评测"), "{out}");
    }

    #[test]
    fn upsert_idempotent() {
        let tools = [tool("shuati 评测"), tool("shuati 对拍")];
        let once = upsert_devcpp_tools("", &tools);
        let twice = upsert_devcpp_tools(&once, &tools);
        // 第二次不产生新段：shuati 段数稳定
        assert_eq!(once.matches("Title=shuati").count(), 2);
        assert_eq!(twice.matches("Title=shuati").count(), 2);
        assert!(twice.contains("Count=2"), "{twice}");
    }

    #[test]
    fn upsert_removes_stale_shuati() {
        // 上次装过 3 个 shuati 工具，这次只装 2 个 → Tool2 该被删掉
        let old = "[Tools]\r\nCount=3\r\n[Tool0]\r\nTitle=shuati 评测\r\nProgram=old\r\n[Tool1]\r\nTitle=shuati 对拍\r\nProgram=old\r\n[Tool2]\r\nTitle=shuati 旧项\r\nProgram=old\r\n";
        let out = upsert_devcpp_tools(old, &[tool("shuati 评测"), tool("shuati 对拍")]);
        assert!(!out.contains("[Tool2]"), "{out}");
        assert!(!out.contains("shuati 旧项"), "{out}");
        assert!(out.contains("Count=2"), "{out}");
    }
}
