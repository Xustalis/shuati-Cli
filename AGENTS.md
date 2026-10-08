# shuati-v2 开发手册

Dev-C++ 萌新的本地评测助手。Rust workspace，GUI 为主 CLI 为辅。

## 构建/测试

```bash
cargo build --workspace       # 全量构建（两个 bin：shuati, shuati-gui）
cargo test --workspace        # 全部测试
cargo run -p shuati-cli       # CLI
cargo run -p shuati-gui       # GUI
```

## 架构

```
crates/
├── core    纯领域模型，零 I/O（Problem/Limits/Verdict/CheckerSpec/TestCase/TestReport）
├── store   文件真相源：problem.toml + tests/*.in|.out + manifest.toml + reports/*.json
│           原子写（tmp+rename）。无数据库。
├── judge   判题引擎：Compiler→Program / Runner(wait4 rusage) / Checker(4种)
│           TestEngine(并行+进度+cancel) StressEngine(gen(seed)→user∥oracle→checker)
│           所有进程调用走 argv，绝不拼 shell
├── fetch   Provider trait + URL 路由。luogu=lentille-context JSON；
│           cf=API 元数据 + HTML 样例（Cloudflare 拦则降级仅元数据）
├── ai      OpenAI 兼容客户端（DeepSeek 默认）。hint/explain/gen_scripts+repair
│           autogen.rs = 生成→落盘→dry_run→失败回喂重试 ≤3 轮
├── cli     clap。命令：doctor/list/new/add/test/stress/gen/hint/setup
└── gui     egui 五屏（快速运行/评测/题库/对拍/设置）+ 保存自动重测(notify) + 拖放
            worker.rs = tokio runtime 线程 + mpsc 事件桥，禁止裸线程
```

## 硬规则

- 进程执行一律 `tokio::process::Command`/argv 数组，禁止 shell 拼接
- 对拍 seed 是唯一的确定性来源：`gen argv[1]=seed`
- 文件是唯一真相源，DB 不存在
- 编译产物缓存：`compile_cached(src, tc, ws.cache_dir())`，指纹 = 源文件 mtime+size+编译器路径；`Program.cached=true` 时 `cleanup_program` 不删
- Rust 2024：`gen` 是保留字，变量/字段用 `generator`/`genf`
- 改动后：`cargo fmt && cargo clippy --workspace --all-targets && cargo build --workspace && cargo test --workspace` 全绿才提交（CI 强制 fmt --check + clippy -D warnings）

## 冒烟清单

```bash
B=target/debug/shuati; W=/tmp/s2ws
$B --root $W add https://www.luogu.com.cn/problem/P1001
$B --root $W test luogu:P1001 --file <解>
$B --root $W stress luogu:P1001 -n 50        # gen/gen.* + solutions/brute.*
$B --root $W stress luogu:P1001 --seed <N> -n 1   # replay
$B --root $W gen luogu:P1001 -n 10
$B --root $W hint luogu:P1001 --explain      # 需 config.toml 配 ai.api_key
```
