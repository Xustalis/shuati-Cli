//! 对拍引擎集成测试：确定性 seed、hack 发现、三方归责。
//! 全部用 python3 脚本（跳过编译，测试快）。

use shuati_core::{CheckerSpec, Lang, Limits, SourceFile};
use shuati_judge::{StressEngine, StressParams};
use std::io::Write;
use std::path::PathBuf;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

fn py_script(dir: &PathBuf, name: &str, code: &str) -> SourceFile {
    let p = dir.join(name);
    let mut f = std::fs::File::create(&p).unwrap();
    f.write_all(code.as_bytes()).unwrap();
    SourceFile { path: p, lang: Lang::Python }
}

fn limits() -> Limits {
    Limits { cpu_ms: 5000, wall_ms: 10000, memory_mb: 256, output_mb: 16 }
}

/// gen: 输出两个随机数
const GEN: &str = "import random,sys;random.seed(int(sys.argv[1]));print(random.randint(0,9),random.randint(0,9))";
/// oracle: a+b
const ORACLE: &str = "a,b=map(int,input().split());print(a+b)";
/// user: 正确解
const USER_OK: &str = "a,b=map(int,input().split());print(a+b)";
/// user: 当 a==9 时错
const USER_BUG: &str = "a,b=map(int,input().split());print(a-b if a==9 else a+b)";
/// gen: 坏生成器（不输出）
const GEN_BROKEN: &str = "import sys;sys.exit(1)";

fn mk_params<'a>(
    genf: &'a SourceFile, user: &'a SourceFile, oracle: &'a SourceFile,
    rounds: u32, seed: u64, lim: &'a Limits, glim: &'a Limits, chk: &'a CheckerSpec,
) -> StressParams<'a> {
    StressParams {
        gen_src: genf, user_src: user, oracle_src: oracle,
        rounds, seed_start: seed, jobs: 2,
        limits_run: lim, limits_gen: glim, checker: chk,
        stop_on_fail: true, progress: None, cancel: None,
    }
}

fn engine(p: &StressParams) -> StressEngine {
    StressEngine::new(p, "g++", "python3").unwrap()
}

#[tokio::test]
async fn stress_all_match() {
    let d = tempfile::tempdir().unwrap();
    let dir = d.path().to_path_buf();
    let genf = py_script(&dir, "g.py", GEN);
    let user = py_script(&dir, "u.py", USER_OK);
    let oracle = py_script(&dir, "o.py", ORACLE);
    let (lim, glim, chk) = (limits(), limits(), CheckerSpec::Tokens);

    let p = mk_params(&genf, &user, &oracle, 20, 1, &lim, &glim, &chk);
    let rep = engine(&p).run(&p).await.unwrap();
    assert_eq!(rep.matched, rep.rounds_done);
    assert!(rep.is_clean());
    assert!(rep.rounds_done > 0);
}

#[tokio::test]
async fn stress_finds_hack_and_replays() {
    let d = tempfile::tempdir().unwrap();
    let dir = d.path().to_path_buf();
    let genf = py_script(&dir, "g.py", GEN);
    let user = py_script(&dir, "u.py", USER_BUG);
    let oracle = py_script(&dir, "o.py", ORACLE);
    let (lim, glim, chk) = (limits(), limits(), CheckerSpec::Tokens);

    // 50 轮内必现 a==9
    let p = mk_params(&genf, &user, &oracle, 50, 1, &lim, &glim, &chk);
    let rep = engine(&p).run(&p).await.unwrap();
    assert!(!rep.hacks.is_empty(), "应找到反例");
    let seed = rep.hacks[0].seed;

    // replay：同 seed 单轮必然重现
    let p2 = mk_params(&genf, &user, &oracle, 1, seed, &lim, &glim, &chk);
    let rep2 = engine(&p2).run(&p2).await.unwrap();
    assert_eq!(rep2.hacks.len(), 1);
    assert_eq!(rep2.hacks[0].seed, seed);
    // 修复后用同 seed 应一致
    let user2 = py_script(&dir, "u2.py", USER_OK);
    let p3 = mk_params(&genf, &user2, &oracle, 1, seed, &lim, &glim, &chk);
    let rep3 = engine(&p3).run(&p3).await.unwrap();
    assert!(rep3.is_clean());
}

#[tokio::test]
async fn stress_gen_failure_attributed() {
    let d = tempfile::tempdir().unwrap();
    let dir = d.path().to_path_buf();
    let genf = py_script(&dir, "g.py", GEN_BROKEN);
    let user = py_script(&dir, "u.py", USER_OK);
    let oracle = py_script(&dir, "o.py", ORACLE);
    let (lim, glim, chk) = (limits(), limits(), CheckerSpec::Tokens);

    let p = mk_params(&genf, &user, &oracle, 5, 1, &lim, &glim, &chk);
    let rep = engine(&p).run(&p).await.unwrap();
    assert_eq!(rep.hacks.len(), 0);
    assert!(rep.gen_failures > 0, "gen 崩溃应归责生成器");
}

#[tokio::test]
async fn stress_cancel_stops_early() {
    let d = tempfile::tempdir().unwrap();
    let dir = d.path().to_path_buf();
    let genf = py_script(&dir, "g.py", GEN);
    let user = py_script(&dir, "u.py", USER_OK);
    let oracle = py_script(&dir, "o.py", ORACLE);
    let (lim, glim, chk) = (limits(), limits(), CheckerSpec::Tokens);

    let cancel = Arc::new(AtomicBool::new(true)); // 已取消
    let p = StressParams {
        gen_src: &genf, user_src: &user, oracle_src: &oracle,
        rounds: 1000, seed_start: 1, jobs: 1,
        limits_run: &lim, limits_gen: &glim, checker: &chk,
        stop_on_fail: false, progress: None, cancel: Some(cancel),
    };
    let rep = engine(&p).run(&p).await.unwrap();
    assert_eq!(rep.rounds_done, 0);
}
