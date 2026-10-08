# shuati v2

给 Dev-C++ 萌新的本地评测小助手。**把你的 .cpp 给我，我告诉你对不对。**

- 拉题（洛谷/Codeforces）→ 拖入源文件 → 保存即自动判题 → 红绿结果 + diff
- 真对拍：seed 驱动循环，抓到反例自动存盘、可精确重放
- AI 提示/诊断 + AI 自动生成对拍脚本（生成后自动验证能跑才交付）
- 单 exe GUI（egui），三平台；同引擎精简 CLI

## 30 秒上手（GUI）

```bash
cargo build --release -p shuati-gui
./target/release/shuati-gui
```

**最快路径（不用建题）**：【快速运行】页 —— 拖入 `main.cpp` → 左边贴输入 → 「▶ 运行」→ 右边看输出，和 Dev-C++ 按 F9 一样。

**完整判题**：

1. 【题库】粘贴 `https://www.luogu.com.cn/problem/P1001` → 获取
2. 【评测】把 `main.cpp` 拖进窗口 → 自动判题
3. 勾「保存后自动重测」→ Dev-C++ 里 Ctrl+S 即触发

## CLI

```bash
shuati doctor                          # 环境自检（含 Dev-C++ g++ 探测）
shuati run main.cpp                    # 快速运行（echo "1 2" | shuati run main.cpp）
shuati add <url>                       # 拉题
shuati test <id> [--file main.cpp]     # 评测
shuati stress <id> -n 200              # 对拍：gen(gen.*) × brute(solutions/brute.*)
shuati stress <id> --seed 42 -n 1      # 精确重放某个反例
shuati stress <id> --auto -n 100       # AI 自动生成 gen+oracle（先验证再跑）
shuati gen <id> -n 50                  # 批量生成用例 → tests/
shuati gen <id> -n 50 --auto           # AI 写脚本再造用例
shuati hint <id> [--explain]           # AI 提示/诊断
shuati new <标题> / shuati list        # 本地题
shuati setup                           # Dev-C++ 集成（Windows 自动写 tools.ini，其余打手动步骤）
```

源文件首行写 `// shuati: luogu P1001` 可绑定题目——`test`/`stress` 拖错文件也不判错题。

## 题目目录

```
problems/<src>__<id>/
├── problem.toml      元数据+时限+checker
├── statement.md      题面
├── tests/NNNN.in|.out + manifest.toml
├── solutions/        main.cpp, brute.cpp（标程）
├── gen/gen.cpp|py    生成器（argv[1]=seed）
├── stress/hack_<seed>.{in,out,ans,meta.json}
└── reports/<ts>.json
```

## 配置

`<题库>/config.toml`：`compiler_cpp`、`compiler_python`、`ai.api_base/model`（默认 DeepSeek）。GUI 设置页可视化编辑。

**API key 按优先级**：环境变量 `SHUATI_API_KEY` > 系统钥匙串（GUI 保存时自动写入）> `config.toml` 明文（旧配置兼容）。

## 注意

- Codeforces 有 Cloudflare 防护：能拿到标题/难度/标签，题面样例被拦时需手动粘贴（GUI 评测页下方可加用例）
- 沙箱：macOS/Linux 用 rlimit+rusage；Windows 用 Job Object（CPU/内存硬限 + 进程树终止），无法创建 Job 时降级为 wall 超时 + kill
