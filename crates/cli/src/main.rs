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
    },
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
        Some(Cmd::Stress { id, generator, oracle, file, rounds, seed, jobs, keep_going }) => {
            cmd_stress(&ws, &cfg, id, generator, oracle, file, rounds, seed, jobs, keep_going).await?
        }
    }
    Ok(())
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
    let gen_path = match generator {
        Some(g) => g,
        None => judge::find_script(&pd.gen_dir(), &["gen"])
            .context("找不到生成器（放 gen/gen.cpp|py 或 --gen 指定）")?,
    };
    let oracle_path = match oracle {
        Some(o) => o,
        None => judge::find_script(&pd.solutions_dir(), &["brute", "oracle", "std"])
            .context("找不到标程（放 solutions/brute.cpp|py 或 --oracle 指定）")?,
    };

    let mk_src = |p: &PathBuf| -> Result<SourceFile> {
        let lang = Lang::from_path(p).context(format!("不支持类型: {}", p.display()))?;
        Ok(SourceFile { path: p.clone(), lang })
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

    let engine = judge::StressEngine::new(&params, &cfg.compiler_cpp, &cfg.compiler_python)?;
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
        Some(p) => println!("  C++ 编译器 : {} ✓", p.display()),
        None => println!("  C++ 编译器 : ✗ 未找到（安装 MinGW/Dev-C++/Xcode CLT）"),
    }
    match &rep.python {
        Some(p) => println!("  Python    : {} ✓", p.display()),
        None => println!("  Python    : ✗ 未找到"),
    }
    println!("  AI        : {}", if cfg.ai.enabled() { "已配置 ✓" } else { "未配置（可选）" });
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
    let engine = judge::TestEngine::new(toolchain);

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
