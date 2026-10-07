//! 界面视图：顶栏四屏 + 底栏环境状态 + diff 弹窗 + 用例查看弹窗。

use crate::app::{ShuatiApp, Tab};
use egui::{Color32, RichText, Ui};

pub fn render(app: &mut ShuatiApp, ctx: &egui::Context) {
    // ── 顶栏
    egui::TopBottomPanel::top("top").show(ctx, |ui| {
        ui.horizontal(|ui| {
            ui.heading("刷题助手");
            ui.separator();
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
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label(RichText::new(app.ws.root.display().to_string()).weak().small());
            });
        });
    });

    // ── 主区
    egui::CentralPanel::default().show(ctx, |ui| match app.tab {
        Tab::Test => test_view(app, ui),
        Tab::Library => library_view(app, ui),
        Tab::Stress => stress_view(app, ui),
        Tab::Settings => settings_view(app, ui),
    });

    diff_window(app, ctx);
    case_view_window(app, ctx);
}

fn tab_btn(ui: &mut Ui, app: &mut ShuatiApp, tab: Tab, label: &str) {
    let sel = app.tab == tab;
    if ui.selectable_label(sel, label).clicked() {
        app.tab = tab;
    }
}

fn env_item(ui: &mut Ui, name: &str, ok: bool, tip: Option<String>) {
    let (icon, color) = if ok { ("✓", Color32::from_rgb(80, 200, 120)) } else { ("✗", Color32::from_rgb(230, 90, 90)) };
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
        Re => Color32::from_rgb(220, 90, 160),
        Ce | Se => Color32::from_rgb(160, 160, 160),
    }
}

// ═══ 评测屏 ═══════════════════════════════════════════

fn test_view(app: &mut ShuatiApp, ui: &mut Ui) {
    egui::ScrollArea::vertical().show(ui, |ui| {
        // 题目行
        ui.horizontal(|ui| {
            ui.label("题目:");
            let cur = app.state.current_problem.clone().unwrap_or_else(|| "（未选择）".into());
            egui::ComboBox::from_id_salt("prob_sel")
                .selected_text(&cur)
                .show_ui(ui, |ui| {
                    let items: Vec<(String, String)> = app
                        .problems
                        .iter()
                        .map(|p| (p.meta.id.to_string(), p.meta.title.clone()))
                        .collect();
                    for (id, title) in items {
                        let verdict = app.state.last_verdicts.get(&id).cloned().unwrap_or_default();
                        let label = format!("{id}  {title}{}", if verdict.is_empty() { String::new() } else { format!("  [{verdict}]") });
                        if ui.selectable_label(cur == id, label).clicked() {
                            app.select_problem(id);
                        }
                    }
                });
            if ui.button("题库").clicked() {
                app.tab = Tab::Library;
            }
            if let Some(title) = app.current_problem().map(|p| p.meta.title.clone()) {
                if ui.button("题面").clicked() {
                    app.show_statement = !app.show_statement;
                }
                ui.label(RichText::new(title).weak());
            }
        });

        // 题面（可折叠）
        if app.show_statement {
            if let Some(pd) = app.current_problem() {
                let stmt = pd.read_statement().unwrap_or_default();
                ui.group(|ui| {
                    egui::ScrollArea::vertical().max_height(220.0).show(ui, |ui| {
                        ui.add(egui::Label::new(RichText::new(if stmt.is_empty() { "（无题面）" } else { &stmt }).monospace()).wrap());
                    });
                });
            }
        }

        ui.add_space(6.0);

        // 源文件区
        ui.group(|ui| {
            ui.horizontal(|ui| {
                ui.label("源文件:");
                let file_label = app
                    .state
                    .watch_file
                    .as_ref()
                    .map(|p| p.display().to_string())
                    .unwrap_or_else(|| "把 .cpp/.py 拖进窗口，或填路径".into());
                ui.monospace(file_label);
            });
            ui.horizontal(|ui| {
                ui.text_edit_singleline(&mut app.src_input);
                if ui.button("选定").clicked() && !app.src_input.is_empty() {
                    let p = std::path::PathBuf::from(app.src_input.trim());
                    if p.exists() {
                        app.set_watch_file(p);
                    } else {
                        app.run.error = Some("文件不存在".into());
                    }
                }
                let mut watch = app.state.watch_enabled;
                if ui.checkbox(&mut watch, "保存即评测").changed() {
                    app.state.watch_enabled = watch;
                    app.save_state();
                }
                ui.separator();
                let can_run = app.current_problem().is_some() && app.state.watch_file.is_some() && !app.run.running;
                if ui.add_enabled(can_run, egui::Button::new("▶ 评测")).clicked() {
                    app.trigger_test();
                }
                if app.run.running && ui.button("■ 停止").clicked() {
                    app.bridge.cancel_current();
                }
            });
        });

        // 错误
        if let Some(err) = app.run.error.clone() {
            ui.horizontal(|ui| {
                ui.colored_label(Color32::from_rgb(230, 90, 90), &err);
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

        // 用例结果
        let rows: Vec<(String, shuati_core::Verdict)> = if !app.run.live_cases.is_empty() {
            app.run.live_cases.clone()
        } else if let Some(rep) = &app.run.report {
            rep.cases.iter().map(|c| (c.name.clone(), c.verdict)).collect()
        } else {
            Vec::new()
        };
        let summary = app.run.report.as_ref().map(|r| (r.verdict, r.pass, r.total));

        for (i, (name, v)) in rows.iter().enumerate() {
            ui.horizontal(|ui| {
                ui.label(RichText::new(format!("{name:12}")).monospace());
                ui.label(RichText::new(v.to_string()).color(verdict_color(*v)).strong().monospace());
                if app.run.report.is_some() && ui.small_button("diff").clicked() {
                    app.run.diff_case = Some(i);
                }
            });
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
    egui::Grid::new("case_grid").striped(true).show(ui, |ui| {
        for (i, (name, input, output)) in app.cases.cases.iter().enumerate() {
            ui.monospace(name);
            ui.label(RichText::new(input_preview(input)).weak().small().monospace());
            ui.label(RichText::new("→").weak());
            ui.label(RichText::new(input_preview(output)).weak().small().monospace());
            if ui.small_button("看").clicked() {
                to_view = Some(i);
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
    if let Some(n) = to_delete {
        app.delete_case(&n);
    }

    // 添加表单
    if app.cases.show_add {
        ui.group(|ui| {
            ui.label("输入 / 期望输出（粘贴样例）:");
            ui.horizontal(|ui| {
                ui.add(egui::TextEdit::multiline(&mut app.cases.new_in).desired_width(280.0).desired_rows(4).hint_text("输入…"));
                ui.add(egui::TextEdit::multiline(&mut app.cases.new_out).desired_width(280.0).desired_rows(4).hint_text("期望输出…"));
            });
            if ui.button("添加用例").clicked() {
                if app.cases.new_in.trim().is_empty() {
                    app.cases.msg = "输入为空".into();
                } else {
                    let (i, o) = (app.cases.new_in.clone(), app.cases.new_out.clone());
                    app.add_case(&i, &o);
                    app.cases.new_in.clear();
                    app.cases.new_out.clear();
                }
            }
        });
    }
}

fn input_preview(s: &str) -> String {
    let one = s.lines().next().unwrap_or("");
    if one.len() > 40 { format!("{}…", &one[..40]) } else { one.to_string() }
}

/// 用例内容查看弹窗
fn case_view_window(app: &mut ShuatiApp, ctx: &egui::Context) {
    let Some(idx) = app.cases.view_case else { return };
    let Some((name, input, output)) = app.cases.cases.get(idx).cloned() else {
        app.cases.view_case = None;
        return;
    };
    let mut open = true;
    egui::Window::new(format!("用例 {name}"))
        .open(&mut open)
        .default_size([640.0, 360.0])
        .show(ctx, |ui| {
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
    let Some(idx) = app.run.diff_case else { return };
    let Some(rep) = &app.run.report else {
        app.run.diff_case = None;
        return;
    };
    let Some(case) = rep.cases.get(idx) else {
        app.run.diff_case = None;
        return;
    };

    let mut open = true;
    egui::Window::new(format!("差异 · {}", case.name))
        .open(&mut open)
        .default_size([700.0, 420.0])
        .show(ctx, |ui| {
            ui.columns(2, |cols| {
                cols[0].label(RichText::new("你的输出").strong());
                cols[1].label(RichText::new("期望输出").strong());
            });
            ui.separator();
            egui::ScrollArea::both().show(ui, |ui| {
                ui.columns(2, |cols| {
                    diff_text(&mut cols[0], &case.actual, &case.expected);
                    diff_text(&mut cols[1], &case.expected, &case.actual);
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

/// 逐行高亮差异
fn diff_text(ui: &mut Ui, text: &str, other: &str) {
    let lines: Vec<&str> = text.lines().collect();
    let other_lines: Vec<&str> = other.lines().collect();
    for (i, line) in lines.iter().enumerate() {
        let same = other_lines.get(i).map(|o| o.trim() == line.trim()).unwrap_or(false);
        let rt = if same { RichText::new(*line).monospace() } else { RichText::new(*line).monospace().color(Color32::from_rgb(230, 90, 90)) };
        ui.label(rt);
    }
}

// ═══ 题库屏 ═══════════════════════════════════════════

fn library_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.horizontal(|ui| {
        ui.label("拉题 URL:");
        ui.add(egui::TextEdit::singleline(&mut app.url_input).desired_width(360.0).hint_text("https://www.luogu.com.cn/problem/P1001"));
        let btn = ui.add_enabled(!app.fetching && !app.url_input.trim().is_empty(), egui::Button::new("获取"));
        if btn.clicked() {
            app.trigger_fetch(app.url_input.trim().to_string());
        }
        if app.fetching {
            ui.spinner();
        }
    });
    if !app.lib_msg.is_empty() {
        let color = if app.lib_msg.contains("失败") { Color32::from_rgb(230, 90, 90) } else { Color32::from_rgb(80, 200, 120) };
        ui.label(RichText::new(&app.lib_msg).color(color));
    }

    ui.horizontal(|ui| {
        ui.label("新建本地题:");
        ui.text_edit_singleline(&mut app.new_title);
        if ui.button("创建").clicked() && !app.new_title.trim().is_empty() {
            let title = app.new_title.trim().to_string();
            let slug = format!("{}", shuati_store::now_ts());
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

    ui.add_space(8.0);

    egui::ScrollArea::vertical().show(ui, |ui| {
        egui::Grid::new("prob_grid").striped(true).show(ui, |ui| {
            ui.label(RichText::new("ID").strong());
            ui.label(RichText::new("标题").strong());
            ui.label(RichText::new("来源").strong());
            ui.label(RichText::new("最近").strong());
            ui.label("");
            ui.end_row();
            let items: Vec<(String, String, String, String)> = app
                .problems
                .iter()
                .map(|p| {
                    (
                        p.meta.id.to_string(),
                        p.meta.title.clone(),
                        p.meta.id.source.to_string(),
                        app.state.last_verdicts.get(&p.meta.id.to_string()).cloned().unwrap_or_else(|| "—".into()),
                    )
                })
                .collect();
            for (id, title, src, verdict) in items {
                ui.monospace(&id);
                ui.label(&title);
                ui.label(src);
                ui.label(verdict);
                if ui.button("选中").clicked() {
                    app.select_problem(id);
                    app.tab = Tab::Test;
                }
                ui.end_row();
            }
        });
    });
}

// ═══ 对拍屏 ═══════════════════════════════════════════

fn stress_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.heading("对拍（深度测试）");
    ui.label(RichText::new("生成器随机造数据 → 你的程序和暴力标程同时跑 → 输出不一致即抓到反例").weak());
    ui.add_space(8.0);

    // 文件选择
    egui::Grid::new("stress_files").num_columns(2).spacing([12.0, 6.0]).show(ui, |ui| {
        ui.label("生成器 gen（argv[1]=seed）:");
        ui.horizontal(|ui| {
            ui.add(egui::TextEdit::singleline(&mut app.stress.gen_path).desired_width(340.0).hint_text("gen/gen.cpp 或 .py"));
        });
        ui.end_row();
        ui.label("标程 oracle/brute:");
        ui.horizontal(|ui| {
            ui.add(egui::TextEdit::singleline(&mut app.stress.oracle_path).desired_width(340.0).hint_text("solutions/brute.cpp 或 .py"));
        });
        ui.end_row();
        ui.label("用户解:");
        ui.monospace(
            app.state
                .watch_file
                .as_ref()
                .map(|p| p.display().to_string())
                .unwrap_or_else(|| "（在评测页选定源文件）".into()),
        );
        ui.end_row();
    });

    // 参数行
    ui.horizontal(|ui| {
        ui.label("轮数:");
        ui.add(egui::TextEdit::singleline(&mut app.stress.rounds_input).desired_width(70.0));
        ui.label("起始 seed:");
        ui.add(egui::TextEdit::singleline(&mut app.stress.seed_input).desired_width(90.0).hint_text("随机"));
        ui.checkbox(&mut app.stress.keep_going, "出错后继续");
        ui.separator();
        let can = app.current_problem().is_some() && !app.stress.running;
        if ui.add_enabled(can, egui::Button::new("▶ 开始对拍")).clicked() {
            app.trigger_stress();
        }
        if app.stress.running && ui.button("■ 停止").clicked() {
            app.bridge.cancel_current();
        }
        if ui.button("自动探测文件").clicked() {
            app.stress.gen_path.clear();
            app.stress.oracle_path.clear();
            app.autodetect_stress_paths();
        }
    });
    if !app.stress.msg.is_empty() {
        ui.colored_label(Color32::from_rgb(230, 90, 90), &app.stress.msg);
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
                ui.label(RichText::new(format!("gen异常 {}", app.stress.gen_fail)).color(Color32::from_rgb(230, 180, 60)));
            }
            if app.stress.oracle_fail > 0 {
                ui.label(RichText::new(format!("标程异常 {}", app.stress.oracle_fail)).color(Color32::from_rgb(230, 180, 60)));
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
    } else if let Some(rep) = &app.stress.report {
        if rep.is_clean() {
            ui.label(RichText::new(format!("{} 轮全部一致，没找到反例 ✓", rep.rounds_done)).color(Color32::from_rgb(80, 200, 120)));
        }
    }
}

// ═══ 设置屏 ═══════════════════════════════════════════

fn settings_view(app: &mut ShuatiApp, ui: &mut Ui) {
    egui::Grid::new("settings").num_columns(2).spacing([16.0, 8.0]).show(ui, |ui| {
        ui.label("C++ 编译器:");
        ui.horizontal(|ui| {
            ui.text_edit_singleline(&mut app.cfg_cpp);
            if let Some(env) = &app.env {
                if let Some(p) = &env.cpp {
                    ui.label(RichText::new(format!("自动: {}", p.display())).weak().small());
                }
            }
        });
        ui.end_row();

        ui.label("Python:");
        ui.horizontal(|ui| {
            ui.text_edit_singleline(&mut app.cfg_py);
            if let Some(env) = &app.env {
                if let Some(p) = &env.python {
                    ui.label(RichText::new(format!("自动: {}", p.display())).weak().small());
                }
            }
        });
        ui.end_row();

        ui.label("AI API Key:");
        ui.add(egui::TextEdit::singleline(&mut app.cfg_api_key).password(true).hint_text("sk-..."));
        ui.end_row();

        ui.label("题库目录:");
        ui.monospace(app.ws.root.display().to_string());
        ui.end_row();
    });

    ui.add_space(8.0);
    if ui.button("保存").clicked() {
        app.cfg.compiler_cpp = app.cfg_cpp.trim().to_string();
        app.cfg.compiler_python = app.cfg_py.trim().to_string();
        app.cfg.ai.api_key = app.cfg_api_key.trim().to_string();
        app.cfg.editor = app.cfg_editor.trim().to_string();
        match app.cfg.save(&app.ws.config_path()) {
            Ok(()) => app.refresh_env(),
            Err(e) => app.run.error = Some(format!("配置保存失败: {e}")),
        }
    }
    ui.add_space(4.0);
    ui.label(RichText::new("提示：Dev-C++ 自带的 g++ 通常在 C:\\Program Files (x86)\\Dev-Cpp\\MinGW64\\bin\\g++.exe，不在 PATH 时把完整路径填到上面").weak().small());
}
