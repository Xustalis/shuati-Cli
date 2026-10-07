//! 界面视图：顶栏四屏 + 底栏环境状态 + diff 弹窗。

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
                env_item(ui, "AI", app.cfg.ai.enabled(), Some("已配置".to_string()));
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

// ═══ 评测屏 ═══════════════════════════════════════════

fn test_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.horizontal(|ui| {
        ui.label("题目:");
        // 题目选择下拉
        let cur = app.state.current_problem.clone().unwrap_or_else(|| "（未选择）".into());
        egui::ComboBox::from_id_salt("prob_sel")
            .selected_text(&cur)
            .show_ui(ui, |ui| {
                let ids: Vec<String> = app.problems.iter().map(|p| p.meta.id.to_string()).collect();
                for id in ids {
                    let label = {
                        let p = app.problems.iter().find(|x| x.meta.id.to_string() == id).unwrap();
                        let verdict = app.state.last_verdicts.get(&id).cloned().unwrap_or_default();
                        format!("{}  {}{}", id, p.meta.title, if verdict.is_empty() { "".into() } else { format!("  [{verdict}]") })
                    };
                    if ui.selectable_label(cur == id, label).clicked() {
                        app.select_problem(id);
                    }
                }
            });
        if ui.button("前往题库").clicked() {
            app.tab = Tab::Library;
        }
    });

    ui.add_space(8.0);

    // 源文件区（拖入）
    ui.group(|ui| {
        ui.horizontal(|ui| {
            ui.label("源文件:");
            let file_label = app
                .state
                .watch_file
                .as_ref()
                .map(|p| p.display().to_string())
                .unwrap_or_else(|| "把 .cpp/.py 拖进窗口，或在下面填路径".into());
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

    ui.add_space(8.0);

    // 错误信息
    if let Some(err) = &app.run.error {
        ui.colored_label(Color32::from_rgb(230, 90, 90), err);
        if ui.button("清除").clicked() {
            app.run.error = None;
        }
    }

    // 运行状态条
    if app.run.compiling {
        ui.label("编译中…");
    } else if app.run.running || app.run.report.is_some() {
        let done = app.run.done;
        let total = app.run.total.max(1);
        ui.add(egui::ProgressBar::new(done as f32 / total as f32).text(format!("{done}/{total}")));
    }

    ui.add_space(4.0);

    // 用例结果列表（实时）
    let rows: Vec<(String, shuati_core::Verdict)> = if !app.run.live_cases.is_empty() {
        app.run.live_cases.clone()
    } else if let Some(rep) = &app.run.report {
        rep.cases.iter().map(|c| (c.name.clone(), c.verdict)).collect()
    } else {
        Vec::new()
    };
    let summary = app.run.report.as_ref().map(|r| (r.verdict, r.pass, r.total));

    egui::ScrollArea::vertical().show(ui, |ui| {
        for (i, (name, v)) in rows.iter().enumerate() {
            case_row(app, ui, i, name, *v);
        }
        if let Some((v, pass, total)) = summary {
            ui.separator();
            ui.label(
                RichText::new(format!("结果 {v} · {pass}/{total}"))
                    .color(verdict_color(v))
                    .size(16.0),
            );
        }
    });
}

fn case_row(app: &mut ShuatiApp, ui: &mut Ui, idx: usize, name: &str, v: shuati_core::Verdict) {
    ui.horizontal(|ui| {
        ui.label(RichText::new(format!("{name:12}")).monospace());
        ui.label(RichText::new(v.to_string()).color(verdict_color(v)).strong().monospace());
        // 有报告 → 可点 diff
        if app.run.report.is_some() {
            if ui.small_button("diff").clicked() {
                app.run.diff_case = Some(idx);
            }
        }
    });
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

/// 逐 token 高亮差异部分（简化 diff：逐行比对）
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
        ui.text_edit_singleline(&mut app.url_input);
        if ui.button("获取").clicked() {
            // M3 里程碑：接 fetch crate
            app.lib_msg = "拉题功能在 M3 里程碑实现".into();
        }
    });
    if !app.lib_msg.is_empty() {
        ui.label(&app.lib_msg);
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

// ═══ 对拍屏（M2 占位） ═════════════════════════════════

fn stress_view(app: &mut ShuatiApp, ui: &mut Ui) {
    ui.heading("对拍（深度测试）");
    ui.add_space(8.0);
    ui.label("M2 里程碑实现：seed 驱动循环对拍 + hack 自动落盘 + AI 生成 gen/oracle。");
    ui.label("界面槽位已预留：gen 文件 | oracle 文件 | 轮数 | [开始] | 实时计数 | hack 列表");
    let _ = app;
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
}
