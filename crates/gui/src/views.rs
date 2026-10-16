//! 界面视图：顶栏四屏 + 底栏环境状态 + diff 弹窗 + 用例查看弹窗。

use crate::app::{ShuatiApp, Tab};
use egui::{Color32, RichText, Ui};

pub fn render(app: &mut ShuatiApp, ctx: &egui::Context) {
    // ── 顶栏
    egui::TopBottomPanel::top("top").show(ctx, |ui| {
        ui.horizontal(|ui| {
            ui.heading("刷题助手");
            ui.separator();
            tab_btn(ui, app, Tab::Quick, "快速运行");
            tab_btn(ui, app, Tab::Test, "评测");
            tab_btn(ui, app, Tab::Library, "题库");
            tab_btn(ui, app, Tab::Stress, "对拍");
            tab_btn(ui, app, Tab::Settings, "设置");
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if app.state.watch_enabled && app.state.watch_file.is_some() {
                    ui.label(RichText::new("● 监视中").color(Color32::from_rgb(80, 200, 120)));
                }
            });
        });
    });

    // ── 底栏环境状态
    egui::TopBottomPanel::bottom("bottom").show(ctx, |ui| {
        ui.horizontal(|ui| {
            if let Some(env) = &app.env {
                env_item(ui, "g++", env.cpp.is_some(), env.cpp.as_ref().map(|p| p.display().to_string()));
                env_item(ui, "python", env.python.is_some(), env.python.as_ref().map(|p| p.display().to_string()));
            }
            let ai_tip = match app.key_src {
                shuati_store::secrets::KeySource::Env => "key 来自环境变量".to_string(),
                shuati_store::secrets::KeySource::Keyring => "key 在系统钥匙串".to_string(),
                shuati_store::secrets::KeySource::File => "key 在 config.toml".to_string(),
                shuati_store::secrets::KeySource::None => "未配置 api_key（设置页可配）".to_string(),
            };
            env_item(ui, "AI", app.ai_ready(), Some(ai_tip));
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label(RichText::new(app.ws.root.display().to_string()).weak().small());
            });
        });
    });

    // ── 主区
    egui::CentralPanel::default().show(ctx, |ui| match app.tab {
        Tab::Quick => quick_view(app, ui),
        Tab::Test => test_view(app, ui),
        Tab::Library => library_view(app, ui),
        Tab::Stress => stress_view(app, ui),
        Tab::Settings => settings_view(app, ui),
    });

    diff_window(app, ctx);
    case_view_window(app, ctx);
    ai_window(app, ctx);
    problem_edit_window(app, ctx);
}

/// 题目设置弹窗：标题/时限/内存/题面
fn problem_edit_window(app: &mut ShuatiApp, ctx: &egui::Context) {
    if !app.prob_edit.show {
        return;
    }
    let mut open = true;
    egui::Window::new("题目设置").open(&mut open).default_size([560.0, 420.0]).show(ctx, |ui| {
        egui::Grid::new("pe_grid").num_columns(2).spacing([12.0, 6.0]).show(ui, |ui| {
            ui.label("标题:");
            ui.text_edit_singleline(&mut app.prob_edit.title);
            ui.end_row();
            ui.label("时限 (ms):");
            ui.add(egui::TextEdit::singleline(&mut app.prob_edit.cpu_ms).desired_width(100.0));
            ui.end_row();
            ui.label("内存 (MB):");
            ui.add(egui::TextEdit::singleline(&mut app.prob_edit.mem_mb).desired_width(100.0));
            ui.end_row();
            ui.label("判题方式:");
            ui.horizontal(|ui| {
                egui::ComboBox::from_id_salt("checker_sel")
                    .selected_text(match app.prob_edit.checker.as_str() {
                        "exact" => "完全一致",
                        "float" => "浮点容差",
                        _ => "按 token 比较（默认）",
                    })
                    .show_ui(ui, |ui| {
                        ui.selectable_value(&mut app.prob_edit.checker, "tokens".into(), "按 token 比较");
                        ui.selectable_value(&mut app.prob_edit.checker, "exact".into(), "完全一致");
                        ui.selectable_value(&mut app.prob_edit.checker, "float".into(), "浮点容差");
                    });
                if app.prob_edit.checker == "float" {
                    ui.label("eps:");
                    ui.add(egui::TextEdit::singleline(&mut app.prob_edit.eps).desired_width(80.0).hint_text("1e-6"));
                }
            });
            ui.end_row();
        });
        ui.add_space(6.0);
        ui.horizontal(|ui| {
            if ui.button(if app.prob_edit.edit_statement { "收起题面编辑" } else { "编辑题面" }).clicked() {
                app.prob_edit.edit_statement = !app.prob_edit.edit_statement;
            }
            if ui.button("打开题目目录").on_hover_text("在文件管理器中打开（放 gen/brute/数据文件用）").clicked()
                && let Some(pd) = app.current_problem()
            {
                open_in_explorer(&pd.dir);
            }
        });
        if app.prob_edit.edit_statement {
            egui::ScrollArea::vertical().max_height(200.0).show(ui, |ui| {
                ui.add(
                    egui::TextEdit::multiline(&mut app.prob_edit.statement)
                        .desired_width(f32::INFINITY)
                        .desired_rows(10),
                );
            });
        }
        if !app.prob_edit.msg.is_empty() {
            ui.colored_label(Color32::from_rgb(230, 90, 90), &app.prob_edit.msg);
        }
        ui.add_space(6.0);
        if ui.button("保存").clicked() {
            app.save_problem_editor();
        }
    });
    if !open {
        app.prob_edit.show = false;
    }
}

/// AI 回答浮窗
fn ai_window(app: &mut ShuatiApp, ctx: &egui::Context) {
    if !app.ai.show {
        return;
    }
    let mut open = true;
    egui::Window::new("AI 助手").open(&mut open).default_size([560.0, 380.0]).show(ctx, |ui| {
        if app.ai.busy {
            ui.horizontal(|ui| {
                ui.spinner();
                ui.label("AI 工作中…");
            });
        }
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add(egui::Label::new(RichText::new(&app.ai.text)).wrap());
        });
    });
    if !open {
        app.ai.show = false;
    }
}

fn tab_btn(ui: &mut Ui, app: &mut ShuatiApp, tab: Tab, label: &str) {
    let sel = app.tab == tab;
    if ui.selectable_label(sel, label).clicked() {
        app.tab = tab;
    }
}

fn env_item(ui: &mut Ui, name: &str, ok: bool, tip: Option<String>) {
    let (icon, color) =
        if ok { ("✓", Color32::from_rgb(80, 200, 120)) } else { ("✗", Color32::from_rgb(230, 90, 90)) };
    let text = RichText::new(format!("{name} {icon}")).color(color).small();
    match tip {
        Some(t) => {
            ui.label(text).on_hover_text(t);
        }
        None => {
            ui.label(text);
        }
    }
}

fn verdict_color(v: shuati_core::Verdict) -> Color32 {
    use shuati_core::Verdict::*;
    match v {
        Ac => Color32::from_rgb(80, 200, 120),
        Wa => Color32::from_rgb(230, 90, 90),
        Tle => Color32::from_rgb(230, 180, 60),
        Mle => Color32::from_rgb(230, 140, 60),
        Ole => Color32::from_rgb(230, 160, 80),
        Re => Color32::from_rgb(220, 90, 160),
        Ce | Se => Color32::from_rgb(160, 160, 160),
    }
}

// ═══ 快速运行屏 ════════════════════════════════════════
// 贴合 Dev-C++ 习惯：拖入 cpp → 贴输入 → 跑 → 看输出，零前置。

fn quick_view(app: &mut ShuatiApp, ui: &mut Ui) {
    // ── 源文件行
    ui.horizontal(|ui| {
        ui.label("源文件:");
        if let Some(p) = app.state.watch_file.clone() {
            ui.monospace(p.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default())
                .on_hover_text(p.display().to_string());
        } else {
            ui.label(RichText::new("（未选择）").weak());
        }
        if ui.button("选择文件…").clicked()
            && let Some(p) = rfd::FileDialog::new().add_filter("源代码", &["cpp", "cc", "cxx", "c++", "py"]).pick_file()
        {
            app.set_watch_file(p);
        }
        if let Some(p) = app.state.watch_file.clone()
            && ui.small_button("编辑").on_hover_text("用设置里的编辑器打开（留空=系统默认程序）").clicked()
        {
            open_source_file(app, &p);
        }
        let can_run = app.state.watch_file.is_some() && !app.quick.running;
        if ui.add_enabled(can_run, egui::Button::new("▶ 运行").min_size(egui::vec2(76.0, 0.0))).clicked() {
            app.trigger_quick_run();
        }
        if app.quick.running {
            ui.spinner();
            ui.label("编译运行中…");
        }
    });
    if app.state.watch_file.is_none() {
        ui.add_space(16.0);
        ui.group(|ui| {
            ui.label(RichText::new("怎么用").strong());
            ui.label("1. 把 Dev-C++ 里写的 .cpp 拖进这个窗口（或点「选择文件…」）");
            ui.label("2. 左边贴上输入数据，点「▶ 运行」");
            ui.label("3. 右边就是你的程序输出——和按 F9 一样");
            ui.add_space(2.0);
            ui.label(RichText::new("要判对错就勾「对答案」贴上期望输出；做整组测试去「评测」页").weak().small());
        });
    }

    // ── 输入 / 期望 / 输出 栏（对答案时三栏）
    let avail = ui.available_size();
    let out_height = (avail.y - 96.0).max(120.0);
    let n_cols = if app.quick.show_expected { 3 } else { 2 };
    ui.columns(n_cols, |cols| {
        // 输入栏
        cols[0].label(RichText::new("输入 (stdin)").strong());
        let w = cols[0].available_width();
        cols[0].add_sized(
            [w, out_height],
            egui::TextEdit::multiline(&mut app.quick.input)
                .font(egui::TextStyle::Monospace)
                .hint_text("把输入数据贴在这里，例如:\n5\n1 2 3 4 5"),
        );
        // 期望栏（可选）
        let out_col = if app.quick.show_expected {
            cols[1].label(RichText::new("期望输出").strong());
            let w = cols[1].available_width();
            cols[1].add_sized(
                [w, out_height],
                egui::TextEdit::multiline(&mut app.quick.expected)
                    .font(egui::TextStyle::Monospace)
                    .hint_text("贴上正确答案"),
            );
            2
        } else {
            1
        };
        // 输出栏
        let col = &mut cols[out_col];
        col.horizontal(|ui| {
            ui.label(RichText::new("你的输出").strong());
            if let Some(r) = &app.quick.result {
                match r.status {
                    None => {
                        ui.label(RichText::new(format!("{}ms · {}KB", r.cpu_ms, r.mem_kb)).weak().small());
                    }
                    Some(v) => {
                        ui.label(
                            RichText::new(format!("{} · {}ms", v, r.cpu_ms)).color(verdict_color(v)).strong().small(),
                        );
                    }
                }
            }
        });
        let mut out_text = app.quick.result.as_ref().map(|r| r.stdout.clone()).unwrap_or_default();
        let w = col.available_width();
        let err = app.quick.result.as_ref().map(|r| r.stderr.clone()).unwrap_or_default();
        let out_h = if err.is_empty() { out_height } else { out_height - 60.0 };
        col.add_sized(
            [w, out_h.max(60.0)],
            egui::TextEdit::multiline(&mut out_text)
                .font(egui::TextStyle::Monospace)
                .interactive(false)
                .hint_text("程序输出会出现在这里"),
        );
        if !err.is_empty() {
            col.label(RichText::new("stderr").weak().small());
            let mut e = err;
            col.add_sized(
                [w, 52.0],
                egui::TextEdit::multiline(&mut e).font(egui::TextStyle::Monospace).interactive(false),
            );
        }
    });

    // ── 对答案开关 / 判定 / 存用例 / 消息
    ui.horizontal(|ui| {
        ui.checkbox(&mut app.quick.show_expected, "对答案");
        // 有结果+期望非空 → 判 AC/WA
        if app.quick.show_expected
            && let Some(r) = &app.quick.result
            && r.status.is_none()
            && !app.quick.expected.trim().is_empty()
        {
            let ctx =
                shuati_core::CheckCtx { input: &app.quick.input, actual: &r.stdout, expected: &app.quick.expected };
            let v = shuati_judge::check(&shuati_core::CheckerSpec::Tokens, &ctx).unwrap_or(shuati_core::Verdict::Wa);
            ui.label(RichText::new(v.to_string()).color(verdict_color(v)).strong().size(16.0));
        }
        ui.separator();
        let can_save = app.current_problem().is_some() && !app.quick.input.trim().is_empty();
        if ui
            .add_enabled(can_save, egui::Button::new("存为当前题用例"))
            .on_hover_text("输入+期望（空则用实际输出）写到当前题的 tests/")
            .clicked()
        {
            app.save_quick_as_case();
        }
        if app.current_problem().is_none() {
            ui.label(RichText::new("选题后可存用例").weak().small());
        }
        ui.label(RichText::new("F5 = 运行").weak().small());
    });
    if !app.quick.msg.is_empty() {
        let errish = ["失败", "先", "不支持", "没有"].iter().any(|k| app.quick.msg.contains(k));
        let color = if errish { Color32::from_rgb(230, 90, 90) } else { Color32::from_rgb(80, 200, 120) };
        ui.colored_label(color, &app.quick.msg);
    }
}

// ═══ 评测屏 ═══════════════════════════════════════════

fn test_view(app: &mut ShuatiApp, ui: &mut Ui) {
    egui::ScrollArea::vertical().show(ui, |ui| {
        // 空态引导：没有题目时给一条最短路径
        if app.problems.is_empty() {
            ui.add_space(20.0);
            ui.group(|ui| {
                ui.label(RichText::new("快速开始").strong().size(18.0));
                ui.label("1. 去【题库】页粘贴题目链接点「获取」，或新建本地题");
                ui.label("2. 回这里把你的 .cpp 拖进窗口（或点「选择文件…」）");
                ui.label("3. 勾上「保存后自动重测」——在 Dev-C++ 里 Ctrl+S 就会自动判题");
                ui.add_space(4.0);
                if ui.button("去题库").clicked() {
                    app.tab = Tab::Library;
                }
            });
            return;
        }
        // 题目行
        ui.horizontal(|ui| {
            ui.label("题目:");
            let cur = app.state.current_problem.clone().unwrap_or_else(|| "（未选择）".into());
            egui::ComboBox::from_id_salt("prob_sel").selected_text(&cur).show_ui(ui, |ui| {
                let items: Vec<(String, String)> =
                    app.problems.iter().map(|p| (p.meta.id.to_string(), p.meta.title.clone())).collect();
                for (id, title) in items {
                    let verdict = app.state.last_verdicts.get(&id).cloned().unwrap_or_default();
                    let label = format!(
                        "{id}  {title}{}",
                        if verdict.is_empty() { String::new() } else { format!("  [{verdict}]") }
                    );
                    if ui.selectable_label(cur == id, label).clicked() {
                        app.select_problem(id);
                    }
                }
            });
            if ui.button("题库").clicked() {
                app.tab = Tab::Library;
            }
            if let Some(pd) = app.current_problem() {
                let (title, cpu, mem) = (pd.meta.title.clone(), pd.meta.limits.cpu_ms, pd.meta.limits.memory_mb);
                if ui.button("题面").clicked() {
                    app.show_statement = !app.show_statement;
                }
                if ui.button("题目设置").on_hover_text("编辑标题/时限/内存/判题方式/题面").clicked() {
                    app.open_problem_editor();
                }
                ui.label(RichText::new(format!("{title}  ·  {cpu}ms/{mem}MB")).weak());
            }
        });

        // 题面（可折叠）。读盘结果进缓存——这里每帧都渲染
        if app.show_statement && app.current_problem().is_some() {
            let stmt = app.statement_cached();
            ui.group(|ui| {
                egui::ScrollArea::vertical().max_height(220.0).show(ui, |ui| {
                    ui.add(
                        egui::Label::new(RichText::new(if stmt.is_empty() { "（无题面）" } else { stmt }).monospace())
                            .wrap(),
                    );
                });
            });
        }

        ui.add_space(6.0);

        // 源文件区
        ui.group(|ui| {
            ui.horizontal(|ui| {
                ui.label("源文件:");
                match &app.state.watch_file {
                    Some(p) => {
                        ui.monospace(p.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default())
                            .on_hover_text(p.display().to_string());
                    }
                    None => {
                        ui.label(RichText::new("把 .cpp/.py 拖进窗口，或点右侧选择").weak());
                    }
                }
                if ui.button("选择文件…").clicked()
                    && let Some(p) =
                        rfd::FileDialog::new().add_filter("源代码", &["cpp", "cc", "cxx", "c++", "py"]).pick_file()
                {
                    app.set_watch_file(p);
                }
                if let Some(p) = app.state.watch_file.clone()
                    && ui.small_button("编辑").on_hover_text("用设置里的编辑器打开（留空=系统默认程序）").clicked()
                {
                    open_source_file(app, &p);
                }
                let mut watch = app.state.watch_enabled;
                if ui
                    .checkbox(&mut watch, "保存后自动重测")
                    .on_hover_text("在 Dev-C++ 里 Ctrl+S 保存，这里自动重新评测")
                    .changed()
                {
                    app.state.watch_enabled = watch;
                    app.save_state();
                }
            });
            ui.horizontal(|ui| {
                let can_run = app.current_problem().is_some() && app.user_solution().is_some() && !app.run.running;
                let btn = egui::Button::new(RichText::new("▶ 评测").size(15.0)).min_size(egui::vec2(80.0, 26.0));
                if ui.add_enabled(can_run, btn).clicked() {
                    app.trigger_test();
                }
                if app.run.running && ui.button("■ 停止").clicked() {
                    app.bridge.cancel_current();
                }
                ui.separator();
                let ai_ok = app.ai_ready() && !app.ai.busy;
                if ui.add_enabled(ai_ok, egui::Button::new("AI 提示")).clicked() {
                    app.trigger_ai(false);
                }
                if ui.add_enabled(ai_ok, egui::Button::new("AI 诊断")).clicked() {
                    app.trigger_ai(true);
                }
            });
        });

        // 错误（编译错误可能很长，要换行显示）
        if let Some(err) = app.run.error.clone() {
            ui.horizontal(|ui| {
                ui.add(
                    egui::Label::new(RichText::new(&err).color(Color32::from_rgb(230, 90, 90)).monospace().small())
                        .wrap(),
                );
                if ui.small_button("×").clicked() {
                    app.run.error = None;
                }
            });
        }

        // 运行状态
        if app.run.compiling {
            ui.label("编译中…");
        } else if app.run.running || app.run.report.is_some() {
            let done = app.run.done;
            let total = app.run.total.max(1);
            ui.add(egui::ProgressBar::new(done as f32 / total as f32).text(format!("{done}/{total}")));
        }

        // 用例结果：借用引用不 clone——diff_case 是别的字段，写完再赋
        let rows: Vec<(&str, shuati_core::Verdict)> = if !app.run.live_cases.is_empty() {
            app.run.live_cases.iter().map(|(n, v)| (n.as_str(), *v)).collect()
        } else if let Some(rep) = &app.run.report {
            rep.cases.iter().map(|c| (c.name.as_str(), c.verdict)).collect()
        } else {
            Vec::new()
        };
        let summary = app.run.report.as_ref().map(|r| (r.verdict, r.pass, r.total));

        let mut open_diff = None;
        for (name, v) in &rows {
            ui.horizontal(|ui| {
                ui.label(RichText::new(format!("{name:12}")).monospace());
                ui.label(RichText::new(v.to_string()).color(verdict_color(*v)).strong().monospace());
                // 按用例名存——live_cases 是完成顺序，跟报告里的顺序不一样
                if app.run.report.is_some() && ui.small_button("diff").clicked() {
                    open_diff = Some(name.to_string());
                }
            });
        }
        if open_diff.is_some() {
            app.run.diff_case = open_diff;
        }
        if let Some((v, pass, total)) = summary {
            ui.label(RichText::new(format!("结果 {v} · {pass}/{total}")).color(verdict_color(v)).size(16.0));
        }

        ui.add_space(10.0);
        ui.separator();
        ui.add_space(6.0);

        // ── 用例管理 ──
        case_manager(app, ui);
    });
}

/// 用例管理：列表 + 增删 + 查看
fn case_manager(app: &mut ShuatiApp, ui: &mut Ui) {
    if app.current_problem().is_none() {
        ui.label(RichText::new("选好题目后这里管理它的测试用例").weak());
        return;
    }
    ui.horizontal(|ui| {
        ui.label(RichText::new("测试用例").strong());
        ui.label(format!("{} 组", app.cases.cases.len()));
        if ui.button("刷新").clicked() {
            app.reload_cases();
        }
        if ui.button(if app.cases.show_add { "收起" } else { "＋ 添加" }).clicked() {
            app.cases.show_add = !app.cases.show_add;
        }
    });
    if !app.cases.msg.is_empty() {
        ui.label(RichText::new(&app.cases.msg).weak().small());
    }

    // 列表
    let mut to_delete: Option<String> = None;
    let mut to_view: Option<usize> = None;
    let mut to_edit: Option<(String, String, String)> = None;
    let mut to_debug: Option<(String, String)> = None;
    egui::Grid::new("case_grid").striped(true).show(ui, |ui| {
        for (i, (name, input, output)) in app.cases.cases.iter().enumerate() {
            ui.monospace(name);
            ui.label(RichText::new(input_preview(input)).weak().small().monospace());
            ui.label(RichText::new("→").weak());
            ui.label(RichText::new(input_preview(output)).weak().small().monospace());
            if ui.small_button("调试").on_hover_text("把这组数据送去快速运行页单跑").clicked() {
                to_debug = Some((input.clone(), output.clone()));
            }
            if ui.small_button("看").clicked() {
                to_view = Some(i);
            }
            if ui.small_button("改").clicked() {
                to_edit = Some((name.clone(), input.clone(), output.clone()));
            }
            if ui.small_button("删").clicked() {
                to_delete = Some(name.clone());
            }
            ui.end_row();
        }
    });
    if let Some(i) = to_view {
        app.cases.view_case = Some(i);
    }
    if let Some((inp, outp)) = to_debug {
        app.send_case_to_quick(inp, outp);
    }
    if let Some((n, inp, outp)) = to_edit {
        app.cases.editing = Some(n);
        app.cases.new_in = inp;
        app.cases.new_out = outp;
        app.cases.show_add = true;
    }
    if let Some(n) = to_delete {
        app.delete_case(&n);
    }

    // 添加/编辑表单
    if app.cases.show_add {
        ui.group(|ui| {
            let title = app
                .cases
                .editing
                .as_ref()
                .map(|n| format!("编辑用例 {n}"))
                .unwrap_or_else(|| "输入 / 期望输出（粘贴样例）:".into());
            ui.label(title);
            ui.horizontal(|ui| {
                ui.add(
                    egui::TextEdit::multiline(&mut app.cases.new_in)
                        .desired_width(280.0)
                        .desired_rows(4)
                        .hint_text("输入…"),
                );
                ui.add(
                    egui::TextEdit::multiline(&mut app.cases.new_out)
                        .desired_width(280.0)
                        .desired_rows(4)
                        .hint_text("期望输出…"),
                );
            });
            ui.horizontal(|ui| {
                let label = if app.cases.editing.is_some() { "保存" } else { "添加用例" };
                if ui.button(label).clicked() {
                    if app.cases.new_in.trim().is_empty() {
                        app.cases.msg = "输入为空".into();
                    } else {
                        let (i, o) = (app.cases.new_in.clone(), app.cases.new_out.clone());
                        app.add_case(&i, &o);
                        app.cases.new_in.clear();
                        app.cases.new_out.clear();
                    }
                }
                if app.cases.editing.is_some() && ui.button("取消").clicked() {
                    app.cases.editing = None;
                    app.cases.new_in.clear();
                    app.cases.new_out.clear();
                }
            });
        });
    }
}

fn input_preview(s: &str) -> String {
    let one = s.lines().next().unwrap_or("");
    if one.chars().count() > 40 { format!("{}…", one.chars().take(40).collect::<String>()) } else { one.to_string() }
}

/// 用例内容查看弹窗
fn case_view_window(app: &mut ShuatiApp, ctx: &egui::Context) {
    let Some(idx) = app.cases.view_case else { return };
    let Some((name, input, output)) = app.cases.cases.get(idx).cloned() else {
        app.cases.view_case = None;
        return;
    };
    let mut open = true;
    egui::Window::new(format!("用例 {name}")).open(&mut open).default_size([640.0, 360.0]).show(ctx, |ui| {
        ui.columns(2, |cols| {
            cols[0].label(RichText::new("输入").strong());
            cols[1].label(RichText::new("期望输出").strong());
        });
        ui.separator();
        egui::ScrollArea::both().show(ui, |ui| {
            ui.columns(2, |cols| {
                cols[0].add(egui::Label::new(RichText::new(&input).monospace()).wrap());
                cols[1].add(egui::Label::new(RichText::new(&output).monospace()).wrap());
            });
        });
    });
    if !open {
        app.cases.view_case = None;
    }
}

// ═══ diff 弹窗 ═════════════════════════════════════════

fn diff_window(app: &mut ShuatiApp, ctx: &egui::Context) {
    let Some(name) = app.run.diff_case.clone() else { return };
    let Some(rep) = &app.run.report else {
        app.run.diff_case = None;
        return;
    };
    let Some(case) = rep.cases.iter().find(|c| c.name == name) else {
        app.run.diff_case = None;
        return;
    };

    let mut open = true;
    egui::Window::new(format!("差异 · {}", case.name)).open(&mut open).default_size([780.0, 440.0]).show(ctx, |ui| {
        ui.columns(3, |cols| {
            cols[0].label(RichText::new("输入").strong());
            cols[1].label(RichText::new("你的输出").strong());
            cols[2].label(RichText::new("期望输出").strong());
        });
        ui.separator();
        egui::ScrollArea::both().show(ui, |ui| {
            // 虚拟滚动：三列共用同一可视行范围（报告单字段上限 64KB，最坏几千行，
            // 全量建 label 每帧上万 widget 会卡）。行距 = 等宽字高 + item_spacing
            let row_h = ui.text_style_height(&egui::TextStyle::Monospace) + ui.spacing().item_spacing.y;
            let total = case.input.lines().count().max(case.actual.lines().count()).max(case.expected.lines().count());
            let vis = ui.clip_rect();
            let top = ui.cursor().top();
            let first = ((vis.top() - top) / row_h).floor().clamp(0.0, total as f32) as usize;
            let last = (((vis.bottom() - top) / row_h).ceil() as usize + 1).min(total).max(first);
            let range = first..last;
            ui.columns(3, |cols| {
                diff_text(&mut cols[0], &case.input, "", total, &range);
                diff_text(&mut cols[1], &case.actual, &case.expected, total, &range);
                diff_text(&mut cols[2], &case.expected, &case.actual, total, &range);
            });
        });
        if !case.message.is_empty() {
            ui.separator();
            ui.label(RichText::new(&case.message).color(Color32::from_rgb(230, 90, 90)));
        }
    });
    if !open {
        app.run.diff_case = None;
    }
}

/// 逐行高亮差异：只渲染 range 内的可视行，前后用空白占位撑出滚动高度。
/// 三列渲染同一 range 且每列都撑满 total 行，天然对齐。
fn diff_text(ui: &mut Ui, text: &str, other: &str, total: usize, range: &std::ops::Range<usize>) {
    let row_h = ui.text_style_height(&egui::TextStyle::Monospace) + ui.spacing().item_spacing.y;
    let lines: Vec<&str> = text.lines().collect();
    let other_lines: Vec<&str> = other.lines().collect();
    ui.allocate_space(egui::vec2(1.0, range.start as f32 * row_h));
    for i in range.clone() {
        // 行数不够的列补空行；空行用空格占位保住行高
        let line = lines.get(i).copied().filter(|l| !l.is_empty()).unwrap_or(" ");
        let same = other_lines.get(i).map(|o| o.trim() == line.trim()).unwrap_or(false);
        let rt = if same {
            RichText::new(line).monospace()
        } else {
            RichText::new(line).monospace().color(Color32::from_rgb(230, 90, 90))
        };
        ui.label(rt);
    }
    let rest = total.saturating_sub(range.end);
    if rest > 0 {
        ui.allocate_space(egui::vec2(1.0, rest as f32 * row_h));
    }
}

// ═══ 题库屏 ═══════════════════════════════════════════

fn library_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.horizontal(|ui| {
        ui.label("拉题 URL:");
        let resp = ui.add(
            egui::TextEdit::singleline(&mut app.url_input)
                .desired_width(360.0)
                .hint_text("https://www.luogu.com.cn/problem/P1001"),
        );
        // 回车 = 获取
        let enter = resp.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter));
        let btn = ui.add_enabled(!app.fetching && !app.url_input.trim().is_empty(), egui::Button::new("获取"));
        if btn.clicked() || (enter && !app.url_input.trim().is_empty()) {
            app.trigger_fetch(app.url_input.trim().to_string());
        }
        if app.fetching {
            ui.spinner();
        }
    });
    if !app.lib_msg.is_empty() {
        let color = if app.lib_msg.contains("失败") {
            Color32::from_rgb(230, 90, 90)
        } else {
            Color32::from_rgb(80, 200, 120)
        };
        ui.label(RichText::new(&app.lib_msg).color(color));
    }

    ui.horizontal(|ui| {
        ui.label("新建本地题:");
        ui.text_edit_singleline(&mut app.new_title);
        if ui.button("创建").clicked() && !app.new_title.trim().is_empty() {
            let title = app.new_title.trim().to_string();
            // 秒级 ts + 纳秒后缀：同秒连点/脚本批量建不撞目录
            let nanos = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.subsec_nanos())
                .unwrap_or(0);
            let slug = format!("{}{:03}", shuati_store::now_ts(), nanos % 1000);
            let id = shuati_core::ProblemId::new(shuati_core::Source::Local, &slug);
            let meta = shuati_core::Problem {
                id: id.clone(),
                title,
                url: String::new(),
                difficulty: None,
                tags: vec![],
                limits: Default::default(),
                statement: std::path::PathBuf::from("statement.md"),
                checker: Default::default(),
                created_at: shuati_store::now_ts(),
            };
            let dir = app.ws.problem_dir(&id);
            match shuati_store::ProblemDir::create(&dir, &meta) {
                Ok(_) => {
                    app.reload_problems();
                    app.select_problem(id.to_string());
                    app.new_title.clear();
                    app.lib_msg = format!("已创建 {id}，去评测页添加用例");
                }
                Err(e) => app.lib_msg = format!("创建失败: {e}"),
            }
        }
        if ui.button("刷新").clicked() {
            app.reload_problems();
        }
    });

    ui.horizontal(|ui| {
        ui.label("搜索:");
        ui.add(egui::TextEdit::singleline(&mut app.lib_filter).desired_width(220.0).hint_text("按题号或标题过滤"));
    });

    ui.add_space(4.0);

    egui::ScrollArea::vertical().show(ui, |ui| {
        egui::Grid::new("prob_grid").striped(true).show(ui, |ui| {
            ui.label(RichText::new("ID").strong());
            ui.label(RichText::new("标题").strong());
            ui.label(RichText::new("来源").strong());
            ui.label(RichText::new("最近").strong());
            ui.label("");
            ui.end_row();
            let needle = app.lib_filter.trim().to_lowercase();
            let items: Vec<(String, String, String, String)> = app
                .problems
                .iter()
                .filter(|p| {
                    needle.is_empty()
                        || p.meta.id.to_string().to_lowercase().contains(&needle)
                        || p.meta.title.to_lowercase().contains(&needle)
                })
                .map(|p| {
                    (
                        p.meta.id.to_string(),
                        p.meta.title.clone(),
                        p.meta.id.source.to_string(),
                        app.state.last_verdicts.get(&p.meta.id.to_string()).cloned().unwrap_or_else(|| "—".into()),
                    )
                })
                .collect();
            let mut to_delete: Option<String> = None;
            for (id, title, src, verdict) in items {
                ui.monospace(&id);
                ui.label(&title);
                ui.label(src);
                ui.label(verdict);
                ui.horizontal(|ui| {
                    if ui.button("选中").clicked() {
                        app.select_problem(id.clone());
                        app.tab = Tab::Test;
                    }
                    // 删除：连点两次确认（内存态，不写进 state.json）
                    let confirming = app.confirm_delete.as_deref() == Some(id.as_str());
                    if ui.small_button(if confirming { "确认删?" } else { "删" }).clicked() {
                        if confirming {
                            to_delete = Some(id.clone());
                            app.confirm_delete = None;
                        } else {
                            app.confirm_delete = Some(id.clone());
                        }
                    }
                });
                ui.end_row();
            }
            if let Some(id) = to_delete {
                app.delete_problem(&id);
            }
        });
    });
}

// ═══ 对拍屏 ═══════════════════════════════════════════

/// 文件就绪状态图标
fn stress_file_status(ui: &mut Ui, ready: bool) {
    if ready {
        ui.label(RichText::new("✓").color(Color32::from_rgb(80, 200, 120)));
    } else {
        ui.label(RichText::new("✗").color(Color32::from_rgb(230, 90, 90)));
    }
}

fn stress_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.label(RichText::new("随机数据自动造 → 你的程序和暴力标程比输出，找出让你 WA 的那组数据").weak());
    ui.add_space(6.0);

    if app.current_problem().is_none() {
        ui.add_space(10.0);
        ui.group(|ui| {
            ui.label(RichText::new("先选一道题").strong());
            ui.label("对拍需要题目来定时限和比对方式。");
            if ui.button("去题库选题").clicked() {
                app.tab = Tab::Library;
            }
        });
        return;
    }

    // 当前题目指示（对拍沿用其时限/比对方式）
    if let Some(pd) = app.current_problem() {
        ui.horizontal(|ui| {
            ui.label("题目:");
            ui.label(RichText::new(format!("{} · {}", pd.meta.id, pd.meta.title)).weak());
        });
    }

    // ── 三个文件：缺什么一眼可见（用户解 = watch 文件 > solutions/main.*，与对拍逻辑一致）
    let has_gen = !app.stress.gen_path.trim().is_empty();
    let has_oracle = !app.stress.oracle_path.trim().is_empty();
    let user_path = app.user_solution();
    let has_user = user_path.is_some();

    egui::Grid::new("stress_files").num_columns(3).spacing([12.0, 6.0]).show(ui, |ui| {
        // 你的代码
        stress_file_status(ui, has_user);
        ui.label("你的代码:");
        ui.horizontal(|ui| {
            match &user_path {
                Some(p) => {
                    ui.monospace(p.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default())
                        .on_hover_text(p.display().to_string());
                }
                None => {
                    ui.label(RichText::new("（未选择）").weak());
                }
            }
            if ui.small_button("选择…").clicked()
                && let Some(p) = rfd::FileDialog::new().add_filter("源代码", &["cpp", "cc", "cxx", "py"]).pick_file()
            {
                app.set_watch_file(p);
            }
        });
        ui.end_row();
        // 生成器
        stress_file_status(ui, has_gen);
        ui.label("数据生成器 gen:").on_hover_text("每次运行给它一个 seed（argv[1]），它输出一组测试数据");
        ui.horizontal(|ui| {
            ui.add(
                egui::TextEdit::singleline(&mut app.stress.gen_path)
                    .desired_width(300.0)
                    .hint_text("gen/gen.cpp 或 .py"),
            );
            if ui.small_button("…").clicked()
                && let Some(p) = rfd::FileDialog::new().add_filter("源代码", &["cpp", "cc", "cxx", "py"]).pick_file()
            {
                app.stress.gen_path = p.display().to_string();
            }
        });
        ui.end_row();
        // 标程
        stress_file_status(ui, has_oracle);
        ui.label("暴力标程 oracle:").on_hover_text("一个保证正确但可能慢的程序，通常写暴力解法");
        ui.horizontal(|ui| {
            ui.add(
                egui::TextEdit::singleline(&mut app.stress.oracle_path)
                    .desired_width(300.0)
                    .hint_text("solutions/brute.cpp 或 .py"),
            );
            if ui.small_button("…").clicked()
                && let Some(p) = rfd::FileDialog::new().add_filter("源代码", &["cpp", "cc", "cxx", "py"]).pick_file()
            {
                app.stress.oracle_path = p.display().to_string();
            }
        });
        ui.end_row();
    });

    // ── 缺文件 → 引导补齐；齐全 → 大开始按钮
    if !(has_gen && has_oracle) {
        ui.add_space(4.0);
        ui.horizontal(|ui| {
            ui.label("还没有生成器/标程？");
            let ai_ok = app.ai_ready() && !app.ai.busy;
            if ui
                .add_enabled(ai_ok, egui::Button::new("AI 一键生成"))
                .on_hover_text("AI 读题面写 gen+标程，自动验证能跑才给你（需配置 api_key）")
                .clicked()
            {
                app.trigger_ai_scripts();
            }
            if ui.button("写代码模板").on_hover_text("在题目目录生成 gen.cpp/brute.cpp 骨架，改一改就能用").clicked()
            {
                write_stress_templates(app);
            }
        });
    }

    ui.add_space(4.0);
    ui.horizontal(|ui| {
        let ready = has_gen && has_oracle && has_user && !app.stress.running;
        let btn = egui::Button::new(RichText::new("▶ 开始对拍").size(16.0)).min_size(egui::vec2(120.0, 30.0));
        if ui.add_enabled(ready, btn).clicked() {
            app.trigger_stress();
        }
        if app.stress.running && ui.button("■ 停止").clicked() {
            app.bridge.cancel_current();
        }
        if !has_user {
            ui.label(RichText::new("还差：选择你的代码").weak());
        }
    });

    // ── 高级选项（轮数/seed/批量造例）默认收起
    egui::CollapsingHeader::new("高级选项").default_open(false).show(ui, |ui| {
        ui.horizontal(|ui| {
            ui.label("轮数:");
            ui.add(egui::TextEdit::singleline(&mut app.stress.rounds_input).desired_width(70.0));
            ui.label("起始 seed:");
            ui.add(egui::TextEdit::singleline(&mut app.stress.seed_input).desired_width(90.0).hint_text("随机"));
            ui.checkbox(&mut app.stress.keep_going, "出错后继续");
        });
        ui.horizontal(|ui| {
            ui.label("批量造例:");
            ui.add(egui::TextEdit::singleline(&mut app.stress.gen_count_input).desired_width(60.0));
            ui.label("组");
            let can = !app.stress.genning && has_gen && has_oracle;
            if ui
                .add_enabled(can, egui::Button::new("生成 → tests/"))
                .on_hover_text("gen 产输入、标程产答案，直接落到用例库")
                .clicked()
            {
                app.trigger_gen_cases();
            }
            if app.stress.genning {
                ui.spinner();
            }
            ui.separator();
            if ui.button("重新探测文件").on_hover_text("按题目目录约定位置重找 gen/brute").clicked() {
                app.stress.gen_path.clear();
                app.stress.oracle_path.clear();
                app.autodetect_stress_paths();
            }
        });
    });

    if !app.stress.msg.is_empty() {
        // 消息分级：含"失败/找不到/没有/需要"显示红色，其余弱色
        let errish = ["失败", "找不到", "没有", "需要", "先选"].iter().any(|k| app.stress.msg.contains(k));
        let color = if errish { Color32::from_rgb(230, 90, 90) } else { Color32::from_rgb(150, 150, 150) };
        ui.colored_label(color, &app.stress.msg);
    }

    // 实时状态
    if app.stress.running || app.stress.report.is_some() {
        let total: u32 = app.stress.rounds_input.trim().parse().unwrap_or(100);
        let frac = app.stress.done as f32 / total.max(1) as f32;
        ui.add(egui::ProgressBar::new(frac).text(format!("{}/{}", app.stress.done, total)));
        ui.horizontal(|ui| {
            ui.label(RichText::new(format!("✓ {} 一致", app.stress.matched)).color(Color32::from_rgb(80, 200, 120)));
            ui.label(RichText::new(format!("✗ {} 反例", app.stress.hack_count)).color(Color32::from_rgb(230, 90, 90)));
            if app.stress.gen_fail > 0 {
                ui.label(
                    RichText::new(format!("gen异常 {}", app.stress.gen_fail)).color(Color32::from_rgb(230, 180, 60)),
                );
            }
            if app.stress.oracle_fail > 0 {
                ui.label(
                    RichText::new(format!("标程异常 {}", app.stress.oracle_fail))
                        .color(Color32::from_rgb(230, 180, 60)),
                );
            }
        });
    }

    ui.add_space(8.0);
    ui.separator();

    // hack 列表（克隆出来避免借用冲突）
    let hacks = app.stress.hacks.clone();
    if !hacks.is_empty() {
        ui.label(RichText::new(format!("反例（{}）", hacks.len())).strong());
        egui::ScrollArea::vertical().show(ui, |ui| {
            for (i, h) in hacks.iter().enumerate() {
                let open = app.stress.open_hack == Some(i);
                ui.horizontal(|ui| {
                    if ui.button(if open { "▼" } else { "▶" }).clicked() {
                        app.stress.open_hack = if open { None } else { Some(i) };
                    }
                    ui.label(format!("seed={} [{}]", h.seed, h.kind));
                    ui.label(RichText::new(&h.detail).weak().small());
                    if ui.small_button("存为用例").clicked() {
                        let input = h.input.clone();
                        let output = h.oracle_out.clone();
                        app.add_case(&input, &output);
                        app.reload_cases();
                    }
                    if ui.small_button("重放").clicked() {
                        app.stress.seed_input = h.seed.to_string();
                        app.stress.rounds_input = "1".into();
                        app.trigger_stress();
                    }
                    if ui.small_button("调试").on_hover_text("把这组反例送去快速运行页单跑").clicked() {
                        app.send_case_to_quick(h.input.clone(), h.oracle_out.clone());
                    }
                });
                if open {
                    ui.group(|ui| {
                        ui.label(RichText::new("输入").strong().small());
                        ui.add(egui::Label::new(RichText::new(&h.input).monospace().small()).wrap());
                        ui.columns(2, |cols| {
                            cols[0].label(RichText::new("你的输出").strong().small());
                            cols[0].add(egui::Label::new(RichText::new(&h.user_out).monospace().small()).wrap());
                            cols[1].label(RichText::new("标程输出").strong().small());
                            cols[1].add(egui::Label::new(RichText::new(&h.oracle_out).monospace().small()).wrap());
                        });
                    });
                }
            }
        });
    } else if let Some(rep) = &app.stress.report
        && rep.is_clean()
    {
        ui.label(
            RichText::new(format!("{} 轮全部一致，没找到反例 ✓", rep.rounds_done))
                .color(Color32::from_rgb(80, 200, 120)),
        );
    }
}

/// 用设置的编辑器打开源文件；未配置走系统默认关联程序
fn open_source_file(app: &mut ShuatiApp, path: &std::path::Path) {
    let editor = app.cfg_editor.trim();
    let spawned = if editor.is_empty() {
        #[cfg(target_os = "macos")]
        {
            std::process::Command::new("open").arg(path).spawn().is_ok()
        }
        #[cfg(target_os = "windows")]
        {
            std::process::Command::new("explorer").arg(path).spawn().is_ok()
        }
        #[cfg(target_os = "linux")]
        {
            std::process::Command::new("xdg-open").arg(path).spawn().is_ok()
        }
    } else {
        std::process::Command::new(editor).arg(path).spawn().is_ok()
    };
    if !spawned {
        app.run.error = Some(if editor.is_empty() {
            "打开失败：系统默认程序不可用".into()
        } else {
            format!("打开失败：{editor}")
        });
    }
}

/// 在系统文件管理器中打开目录
fn open_in_explorer(path: &std::path::Path) {
    #[cfg(target_os = "macos")]
    let _ = std::process::Command::new("open").arg(path).spawn();
    #[cfg(target_os = "windows")]
    let _ = std::process::Command::new("explorer").arg(path).spawn();
    #[cfg(target_os = "linux")]
    let _ = std::process::Command::new("xdg-open").arg(path).spawn();
}

/// 在题目目录写 gen.cpp / brute.cpp 模板骨架
fn write_stress_templates(app: &mut ShuatiApp) {
    let Some(pd) = app.current_problem() else {
        app.stress.msg = "先选题目".into();
        return;
    };
    let gen_dir = pd.gen_dir();
    let sol_dir = pd.solutions_dir();
    let gen_path = gen_dir.join("gen.cpp");
    let brute_path = sol_dir.join("brute.cpp");
    // 只用标准头：bits/stdc++.h 是 GCC 私有，clang/MSVC 编译不过
    const GEN_TPL: &str = r#"#include <cstdio>
#include <cstdlib>
#include <random>
// 生成器：argv[1] = seed，向 stdout 打印一份合法测试输入
int main(int argc, char** argv) {
    unsigned seed = argc > 1 ? std::atoi(argv[1]) : 1;
    std::mt19937 rng(seed);
    // TODO: 按题目格式生成输入，例如：
    // int n = rng() % 10 + 1;
    // std::printf("%d\n", n);
    return 0;
}
"#;
    const BRUTE_TPL: &str = r#"#include <cstdio>
// 标程/暴力解：读 stdin 打印正确答案，正确性优先
int main() {
    // TODO: 最朴素的暴力实现
    return 0;
}
"#;
    let mut made = Vec::new();
    if !gen_path.exists() && std::fs::write(&gen_path, GEN_TPL).is_ok() {
        made.push(gen_path.display().to_string());
    }
    if !brute_path.exists() && std::fs::write(&brute_path, BRUTE_TPL).is_ok() {
        made.push(brute_path.display().to_string());
    }
    if made.is_empty() {
        app.stress.msg = "gen.cpp / brute.cpp 已存在".into();
    } else {
        app.stress.msg = format!("已写模板: {}", made.join(", "));
        app.autodetect_stress_paths();
    }
}

// ═══ 设置屏 ═══════════════════════════════════════════

fn settings_view(app: &mut ShuatiApp, ui: &mut Ui) {
    egui::Grid::new("settings").num_columns(2).spacing([16.0, 8.0]).show(ui, |ui| {
        ui.label("C++ 编译器:");
        ui.horizontal(|ui| {
            ui.text_edit_singleline(&mut app.cfg_cpp);
            if let Some(env) = &app.env
                && let Some(p) = &env.cpp
            {
                ui.label(RichText::new(format!("自动: {}", p.display())).weak().small());
            }
        });
        ui.end_row();

        ui.label("Python:");
        ui.horizontal(|ui| {
            ui.text_edit_singleline(&mut app.cfg_py);
            if let Some(env) = &app.env
                && let Some(p) = &env.python
            {
                ui.label(RichText::new(format!("自动: {}", p.display())).weak().small());
            }
        });
        ui.end_row();

        ui.label("编辑器:");
        ui.horizontal(|ui| {
            ui.add(
                egui::TextEdit::singleline(&mut app.cfg_editor)
                    .desired_width(300.0)
                    .hint_text("例: devcpp.exe / code / notepad++（留空=系统默认打开）"),
            );
            ui.label(RichText::new("评测页源文件旁的「编辑」按钮用它打开").weak().small());
        });
        ui.end_row();

        ui.label("AI 服务商:");
        ui.horizontal(|ui| {
            for (name, base, model) in [
                ("DeepSeek", "https://api.deepseek.com/v1", "deepseek-chat"),
                ("Kimi", "https://api.moonshot.cn/v1", "moonshot-v1-8k"),
                ("通义", "https://dashscope.aliyuncs.com/compatible-mode/v1", "qwen-plus"),
                ("OpenAI", "https://api.openai.com/v1", "gpt-4o-mini"),
            ] {
                if ui.small_button(name).clicked() {
                    app.cfg_api_base = base.to_string();
                    app.cfg_model = model.to_string();
                }
            }
            ui.label(RichText::new("点一下自动填地址和模型").weak().small());
        });
        ui.end_row();

        ui.label("AI API Key:");
        ui.horizontal(|ui| {
            ui.add(egui::TextEdit::singleline(&mut app.cfg_api_key).password(true).hint_text("sk-..."));
            // key 现存的来源（缓存值，保存后 refresh_env 刷新；留空输入框 = 不改动已存 key）
            let src_note = match app.key_src {
                shuati_store::secrets::KeySource::Env => "（生效中：环境变量 SHUATI_API_KEY）",
                shuati_store::secrets::KeySource::Keyring => "（生效中：系统钥匙串，留空则不改动）",
                shuati_store::secrets::KeySource::File => "（生效中：config.toml 明文）",
                shuati_store::secrets::KeySource::None => "",
            };
            if !src_note.is_empty() {
                ui.label(RichText::new(src_note).weak().small());
            }
            if app.key_src != shuati_store::secrets::KeySource::None
                && ui
                    .small_button("清除已存 key")
                    .on_hover_text("删掉钥匙串和 config.toml 里存的 key（环境变量仍生效）")
                    .clicked()
            {
                shuati_store::secrets::delete_api_key();
                app.cfg.ai.api_key.clear();
                app.cfg_api_key.clear();
                match app.cfg.save(&app.ws.config_path()) {
                    Ok(()) => {
                        app.cfg_msg = "已清除存储的 key".into();
                        app.refresh_env();
                    }
                    Err(e) => app.cfg_msg = format!("清除失败: {e}"),
                }
            }
        });
        ui.end_row();

        ui.label("AI 接口地址:");
        ui.add(
            egui::TextEdit::singleline(&mut app.cfg_api_base)
                .desired_width(340.0)
                .hint_text("https://api.deepseek.com/v1"),
        );
        ui.end_row();

        ui.label("AI 模型:");
        ui.add(egui::TextEdit::singleline(&mut app.cfg_model).desired_width(200.0).hint_text("deepseek-chat"));
        ui.end_row();

        ui.label("");
        ui.label(RichText::new("key 存系统钥匙串；也认环境变量 SHUATI_API_KEY；都不设则 AI 功能关闭").weak().small());
        ui.end_row();

        ui.label("蓝桥 Cookie:");
        ui.add(
            egui::TextEdit::singleline(&mut app.cfg_lanqiao_cookie)
                .password(true)
                .desired_width(340.0)
                .hint_text("拉蓝桥题需登录，粘浏览器 Cookie"),
        );
        ui.end_row();

        ui.label("题库目录:");
        ui.monospace(app.ws.root.display().to_string());
        ui.end_row();
    });

    ui.add_space(8.0);
    ui.horizontal(|ui| {
        if ui.button("保存").clicked() {
            app.cfg.compiler_cpp = app.cfg_cpp.trim().to_string();
            app.cfg.compiler_python = app.cfg_py.trim().to_string();
            // key 优先进系统钥匙串；输入框留空 = 不改动已存的 key
            let new_key = app.cfg_api_key.trim().to_string();
            let mut key_note = "";
            if !new_key.is_empty() {
                match shuati_store::secrets::store_api_key(&new_key) {
                    Ok(()) => {
                        // 进钥匙串了就别再落明文
                        app.cfg.ai.api_key.clear();
                        key_note = "（key 已存系统钥匙串）";
                    }
                    Err(e) => {
                        tracing::warn!("keyring 不可用，回退 config.toml 明文存储: {e}");
                        app.cfg.ai.api_key = new_key;
                        key_note = "（此系统无钥匙串，key 存于 config.toml）";
                    }
                }
            }
            app.cfg.ai.api_base = app.cfg_api_base.trim().to_string();
            app.cfg.ai.model = app.cfg_model.trim().to_string();
            app.cfg.editor = app.cfg_editor.trim().to_string();
            app.cfg.lanqiao_cookie = app.cfg_lanqiao_cookie.trim().to_string();
            match app.cfg.save(&app.ws.config_path()) {
                Ok(()) => {
                    app.cfg_msg = format!("已保存{key_note}");
                    app.refresh_env();
                }
                Err(e) => app.run.error = Some(format!("配置保存失败: {e}")),
            }
        }
        if ui
            .add_enabled(!app.ai_pinging, egui::Button::new("测试连接"))
            .on_hover_text("用当前输入的地址/密钥/模型发一个最小请求")
            .clicked()
        {
            app.trigger_ai_ping();
        }
        if app.ai_pinging {
            ui.spinner();
        }
        if !app.cfg_msg.is_empty() {
            let ok = app.cfg_msg.starts_with("连接正常") || app.cfg_msg == "已保存";
            ui.colored_label(
                if ok { Color32::from_rgb(80, 200, 120) } else { Color32::from_rgb(230, 90, 90) },
                &app.cfg_msg,
            );
        }
    });
    ui.add_space(4.0);
    ui.label(RichText::new("提示：Dev-C++ 自带的 g++ 通常在 C:\\Program Files (x86)\\Dev-Cpp\\MinGW64\\bin\\g++.exe，不在 PATH 时把完整路径填到上面").weak().small());
}
