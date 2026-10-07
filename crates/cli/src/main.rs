//! shuati CLI：引擎的薄壳。
//! 无参数 → 提示（GUI 由 shuati-gui 提供）；有参数 → 子命令。

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use shuati_core::{Lang, ProblemId, SourceFile};
use shuati_judge as judge;
use shuati_store as store;
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "shuati", about = "本地评测小助手 · shuati v2", version)]
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
        /// 起始 seed
        #[arg(long, default_value = "1")]
        seed: u64,
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
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .with_writer(std::io::stderr)
        .init();

    let cli = Cli::parse();
    let root = cli.root.unwrap_or_else(store::Workspace::default_dir);
    let ws = store::Workspace::open(&root)?;
    let cfg = store::Config::load(&ws.config_path());

    match cli.cmd {
        None => {
            eprintln!("shuati v2 — 本地评测小助手");
            eprintln!("用法: shuati <命令>（--help 查看全部）");
            eprintln!("GUI: 运行 shuati-gui");
        }
        Some(Cmd::Doctor) => cmd_doctor(&cfg),
        Some(Cmd::List) => cmd_list(&ws)?,
        Some(Cmd::New { title, slug }) => cmd_new(&ws, &title, slug)?,
        Some(Cmd::Test { id, file, jobs }) => cmd_test(&ws, &cfg, id, file, jobs).await?,
        Some(Cmd::Add { url }) => cmd_add(&ws, &url).await?,
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

/// 快速运行：编译 file → stdin 透传 → stdout/stderr 直接输出
async fn cmd_run(ws: &store::Workspace, cfg: &store::Config, file: &PathBuf) -> Result<()> {
    let lang = Lang::from_path(file).context("不支持的文件类型（.cpp/.py）")?;
    let tc_cfg = pick_tc(lang, cfg);
    let tc = judge::Toolchain::detect(lang, tc_cfg)?;
    println!("[*] 编译 {} …", file.display());
    let prog = judge::compile_cached(&SourceFile { path: file.clone(), lang }, &tc, &ws.cache_dir()).await?;

    let tmp = tempfile::tempdir()?;
    // 管道输入 → 读到临时文件喂给程序；TTY（交互终端）→ 直接给 EOF
    let stdin = {
        use std::io::IsTerminal;
        if std::io::stdin().is_terminal() {
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
    let io = judge::Io {
        stdin,
        stdout: tmp.path().join("stdout"),
        stderr: tmp.path().join("stderr"),
    };
    let limits = shuati_core::Limits { cpu_ms: 5000, wall_ms: 15000, memory_mb: 512, output_mb: 16 };
    let out = judge::run_program(&prog, &io, &limits).await;
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
    let client = shuati_ai::AiClient::new(&ai.api_base, &ai.api_key, &ai.model)?;
    let statement = pd.read_statement().unwrap_or_default();
    let lang = match lang_hint.unwrap_or(&cfg.default_lang) {
        "python" | "py" => Lang::Python,
        _ => Lang::Cpp,
    };
    println!("[*] AI 正在生成对拍脚本（{lang:?}），自动 dry-run 验证…");
    let (gen_path, oracle_path) = shuati_ai::produce_scripts(
        &client,
        &statement,
        lang,
        &pd.dir,
        &cfg.compiler_cpp,
        &cfg.compiler_python,
        |s| println!("    {s}"),
    )
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
    seed: u64,
    auto: bool,
) -> Result<()> {
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目"),
    };

    let (gen_path, oracle_path) = if auto {
        auto_produce(&pd, cfg, None).await?
    } else {
        let g = generator.or_else(|| judge::find_script(&pd.gen_dir(), &["gen"]));
        let o = oracle.or_else(|| judge::find_script(&pd.solutions_dir(), &["brute", "oracle", "std"]));
        (g.ok_or_else(|| anyhow::anyhow!("找不到生成器（放 gen/gen.* 或用 --auto 让 AI 写）"))?,
         o.ok_or_else(|| anyhow::anyhow!("找不到标程（放 solutions/brute.* 或用 --auto）"))?)
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

    println!("[*] 生成 {} 组用例（seed {}..{}）…", count, seed, seed + count as u64 - 1);
    let glim = shuati_core::Limits { cpu_ms: 10000, wall_ms: 15000, memory_mb: 512, output_mb: 64 };
    let seeds = seed..seed + count as u64;
    let made = judge::gen_cases(&gen_prog, &oracle_prog, seeds, &pd.meta.limits, &glim).await;
    judge::cleanup_program(&gen_prog);
    judge::cleanup_program(&oracle_prog);

    let existing = pd.load_cases()?.len() as u32;
    for (i, (s, input, output)) in made.iter().enumerate() {
        let name = format!("{:04}", existing + i as u32 + 1);
        pd.write_case(&name, input, output, shuati_core::CaseOrigin::Generated)?;
        eprint!("\r  {}/{} (seed={})", i + 1, made.len(), s);
    }
    eprintln!();
    println!("[+] 生成 {} 组 → {}/tests/", made.len(), pd.dir.display());
    Ok(())
}

/// 相对路径优先按题目目录解析；不存在则按 CWD 原样使用
fn resolve_rel(problem_dir: &std::path::Path, p: &PathBuf) -> PathBuf {
    if p.is_absolute() {
        return p.clone();
    }
    let cand = problem_dir.join(p);
    if cand.exists() {
        cand
    } else {
        p.clone()
    }
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
    if !ai.enabled() {
        anyhow::bail!("AI 未配置：在设置页或 config.toml 填 ai.api_key（DeepSeek 兼容接口）");
    }
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
            let mut paths: Vec<PathBuf> = entries.filter_map(|e| e.ok().map(|x| x.path())).filter(|p| p.extension().is_some_and(|x| x == "json")).collect();
            paths.sort();
            if let Some(p) = paths.last() {
                if let Ok(t) = std::fs::read_to_string(p) {
                    latest = serde_json::from_str(&t).ok();
                }
            }
        }
        match latest {
            Some(r) => {
                let fails: Vec<String> = r.cases.iter()
                    .filter(|c| c.verdict != shuati_core::Verdict::Ac)
                    .map(|c| format!("{}: {} （期望 {} 实际 {}）", c.name, c.verdict,
                        c.expected.lines().next().unwrap_or(""),
                        c.actual.lines().next().unwrap_or("")))
                    .take(3)
                    .collect();
                format!("总判定 {}，{}/{}。失败点:\n{}", r.verdict, r.pass, r.total, fails.join("\n"))
            }
            None => "（尚未评测过）".into(),
        }
    };

    let client = shuati_ai::AiClient::new(&ai.api_base, &ai.api_key, &ai.model)?;
    println!("[*] AI 思考中…");
    let text = if explain {
        client.explain(&statement, &code, &verdict_info, "").await?
    } else {
        client.hint(&statement, &code, &verdict_info).await?
    };
    println!("{text}");
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
    println!("  1. Dev-C++ → Tools → Configure Tools → Add");
    println!("     Title:   shuati 评测");
    println!("     Program: <shuati.exe 路径>");
    println!("     Working: <PROJECTPATH>");
    println!("     Params:  test --file <SOURCEPATH>");
    println!("  2. 这样工具菜单里就有一键评测当前文件");
    println!();
    println!("或者：GUI 里打开『保存即评测』，Dev-C++ 里 Ctrl+S 即触发判题（推荐萌新）");
}

#[cfg(target_os = "windows")]
fn setup_windows() -> Result<Vec<String>> {
    // Dev-C++ tools 存在 tools.ini / 注册表；稳妥做法只给提示不写注册表
    Ok(vec!["Windows 下自动写 Dev-C++ 工具项尚未实现，请按下方手动步骤".to_string()])
}

async fn cmd_add(ws: &store::Workspace, url: &str) -> Result<()> {
    println!("[*] 正在拉取 {url} …");
    let pd = shuati_fetch::fetch_to_dir(url, ws).await?;
    println!("[+] {}  {}", pd.meta.id, pd.meta.title);
    let cases = pd.load_cases()?;
    println!("    样例 {} 组 → {}", cases.len(), pd.dir.display());
    // 设为当前题目
    let mut state = store::AppState::load(&ws.state_path());
    state.current_problem = Some(pd.meta.id.to_string());
    state.save(&ws.state_path())?;
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
        auto_produce(&pd, cfg, auto_lang.as_deref()).await?
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

    println!("[*] 对拍 {} · {} 轮", pd.meta.id, rounds);
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
        seed_start: seed.unwrap_or(1),
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
        let path = pd.save_hack(hack.seed, &hack.input, &hack.user_out, &hack.oracle_out, &format!("{}: {}", hack.kind, hack.detail))?;
        println!("  hack 已保存 → {}", path.display());
    }

    println!(
        "[结果] {} 轮 · ✓{} 一致 · hack {} · gen异常 {} · 标程异常 {}",
        report.rounds_done, report.matched, report.hacks.len(), report.gen_failures, report.oracle_failures
    );
    if !report.hacks.is_empty() {
        std::process::exit(1);
    }
    Ok(())
}

fn cmd_doctor(cfg: &store::Config) {
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
    println!("  AI:     {}", if cfg.ai.enabled() { "已配置 ✓" } else { "未配置（hint/自动脚本不可用）" });
    println!("  题库:   {}", store::Workspace::default_dir().display());
}

fn cmd_list(ws: &store::Workspace) -> Result<()> {
    let problems = store::load_all_problems(ws);
    if problems.is_empty() {
        println!("（题库为空）使用 `shuati new <标题>` 创建本地题，或 GUI 题库页拉取");
        return Ok(());
    }
    for p in &problems {
        println!("  {:22} {}", p.meta.id, p.meta.title);
    }
    Ok(())
}

fn cmd_new(ws: &store::Workspace, title: &str, slug: Option<String>) -> Result<()> {
    let slug = slug.unwrap_or_else(|| {
        title.chars().filter(|c| c.is_alphanumeric()).collect::<String>()
    });
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
    // 解析题目：参数 > state.current > 唯一题目
    let problems = store::load_all_problems(ws);
    let pd = match resolve_problem(&problems, id.as_deref(), ws)? {
        Some(p) => p,
        None => anyhow::bail!("找不到题目（用 `shuati list` 查看，或先 `shuati new`）"),
    };

    let src_path = match file {
        Some(f) => f,
        None => pd.find_solution().context("solutions/ 下没有源文件")?,
    };
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

    for c in &report.cases {
        let mark = if c.verdict == shuati_core::Verdict::Ac { "✓" } else { "✗" };
        println!("  {} {:12} {:3} {:>6}ms {:>8}KB", mark, c.name, c.verdict, c.cpu_ms, c.memory_kb);
        if c.verdict != shuati_core::Verdict::Ac && !c.message.is_empty() {
            let first = c.message.lines().next().unwrap_or("");
            println!("      {}", first.chars().take(80).collect::<String>());
        }
    }
    println!("[结果] {} · {}/{}", report.verdict, report.pass, report.total);

    let path = pd.save_report(&report)?;
    println!("报告: {}", path.display());

    // 记录最近结果
    let mut state = store::AppState::load(&ws.state_path());
    state
        .last_verdicts
        .insert(pd.meta.id.to_string(), report.verdict.to_string());
    state.save(&ws.state_path())?;

    if report.verdict != shuati_core::Verdict::Ac {
        std::process::exit(1);
    }
    Ok(())
}

fn cfg_cpp_for<'a>(cfg: &'a store::Config, lang: Lang) -> &'a str {
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
            full == needle
                || p.meta.id.slug.to_lowercase() == needle
                || p.meta.id.dir_name().to_lowercase() == needle
        });
        return Ok(hit);
    }
    // state.json 记录的当前题目
    let state = store::AppState::load(&ws.state_path());
    if let Some(cur) = &state.current_problem {
        if let Some(p) = problems.iter().find(|p| &p.meta.id.to_string() == cur) {
            return Ok(Some(p));
        }
    }
    // 只有一道题时直接用
    if problems.len() == 1 {
        return Ok(problems.first());
    }
    Ok(None)
}
