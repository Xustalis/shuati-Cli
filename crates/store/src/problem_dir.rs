use crate::{atomic_write, Result, StoreError};
use shuati_core::{CaseOrigin, CheckerSpec, Limits, Problem, ProblemId, TestCase};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// 单个题目目录的全部内容访问。
/// 目录结构：
///   problem.toml / statement.md / tests/*.in|*.out + manifest.toml
///   solutions/  gen/  stress/  reports/
#[derive(Debug, Clone)]
pub struct ProblemDir {
    pub dir: PathBuf,
    pub meta: Problem,
}

/// problem.toml 的磁盘格式（与 Problem 同构但容忍缺省）
#[derive(Debug, Serialize, Deserialize)]
struct ProblemToml {
    id: String,
    title: String,
    #[serde(default)]
    url: String,
    #[serde(default)]
    difficulty: Option<String>,
    #[serde(default)]
    tags: Vec<String>,
    #[serde(default)]
    limits: Limits,
    #[serde(default)]
    checker: CheckerSpec,
    #[serde(default)]
    created_at: i64,
}

/// tests/manifest.toml
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Manifest {
    /// name -> origin ("sample" | "gen" | "stress")
    #[serde(default)]
    pub cases: std::collections::BTreeMap<String, String>,
}

impl ProblemDir {
    /// 打开已有题目目录（读 problem.toml）
    pub fn open(dir: impl Into<PathBuf>) -> Result<Self> {
        let dir = dir.into();
        let meta_path = dir.join("problem.toml");
        let text = std::fs::read_to_string(&meta_path)
            .map_err(|e| StoreError::Invalid(meta_path.clone(), e.to_string()))?;
        let t: ProblemToml = toml::from_str(&text)?;
        let id = ProblemId::parse(&t.id)
            .ok_or_else(|| StoreError::Invalid(meta_path.clone(), format!("bad id {}", t.id)))?;
        Ok(Self {
            dir,
            meta: Problem {
                id,
                title: t.title,
                url: t.url,
                difficulty: t.difficulty,
                tags: t.tags,
                limits: t.limits,
                statement: PathBuf::from("statement.md"),
                checker: t.checker,
                created_at: t.created_at,
            },
        })
    }

    /// 创建新题目目录（含 problem.toml / statement.md / 子目录）
    pub fn create(dir: impl Into<PathBuf>, meta: &Problem) -> Result<Self> {
        let dir = dir.into();
        for sub in ["tests", "solutions", "gen", "stress", "reports"] {
            std::fs::create_dir_all(dir.join(sub))?;
        }
        let pd = Self { dir, meta: meta.clone() };
        pd.save_meta()?;
        let stmt = pd.statement_path();
        if !stmt.exists() {
            atomic_write(&stmt, b"")?;
        }
        Ok(pd)
    }

    pub fn save_meta(&self) -> Result<()> {
        let t = ProblemToml {
            id: self.meta.id.to_string(),
            title: self.meta.title.clone(),
            url: self.meta.url.clone(),
            difficulty: self.meta.difficulty.clone(),
            tags: self.meta.tags.clone(),
            limits: self.meta.limits,
            checker: self.meta.checker.clone(),
            created_at: self.meta.created_at,
        };
        Ok(atomic_write(&self.dir.join("problem.toml"), toml::to_string_pretty(&t)?.as_bytes())?)
    }

    pub fn statement_path(&self) -> PathBuf {
        self.dir.join(&self.meta.statement)
    }

    pub fn read_statement(&self) -> Result<String> {
        Ok(std::fs::read_to_string(self.statement_path()).unwrap_or_default())
    }

    pub fn write_statement(&self, text: &str) -> Result<()> {
        Ok(atomic_write(&self.statement_path(), text.as_bytes())?)
    }

    pub fn tests_dir(&self) -> PathBuf {
        self.dir.join("tests")
    }

    pub fn stress_dir(&self) -> PathBuf {
        self.dir.join("stress")
    }

    pub fn reports_dir(&self) -> PathBuf {
        self.dir.join("reports")
    }

    pub fn solutions_dir(&self) -> PathBuf {
        self.dir.join("solutions")
    }

    pub fn gen_dir(&self) -> PathBuf {
        self.dir.join("gen")
    }

    /// 读取 tests/ 下全部用例（按文件名排序），*.in 必须有，*.out 可缺
    pub fn load_cases(&self) -> Result<Vec<TestCase>> {
        let manifest = self.load_manifest().unwrap_or_default();
        let tests = self.tests_dir();
        let mut cases = Vec::new();
        if !tests.exists() {
            return Ok(cases);
        }
        let mut ins: Vec<PathBuf> = std::fs::read_dir(tests)?
            .filter_map(|e| e.ok().map(|x| x.path()))
            .filter(|p| p.extension().is_some_and(|x| x == "in"))
            .collect();
        ins.sort();
        for in_path in ins {
            let name = in_path.file_stem().unwrap_or_default().to_string_lossy().to_string();
            let input = std::fs::read_to_string(&in_path)?;
            let out_path = in_path.with_extension("out");
            let expected = if out_path.exists() {
                Some(std::fs::read_to_string(&out_path)?)
            } else {
                None
            };
            let origin = match manifest.cases.get(&name).map(|s| s.as_str()) {
                Some("gen") => CaseOrigin::Generated,
                Some("stress") => CaseOrigin::Stress,
                _ => CaseOrigin::Sample,
            };
            cases.push(TestCase { name, input, expected, origin });
        }
        Ok(cases)
    }

    /// 写入一对用例文件
    pub fn write_case(&self, name: &str, input: &str, output: &str, origin: CaseOrigin) -> Result<()> {
        let tests = self.tests_dir();
        atomic_write(&tests.join(format!("{name}.in")), input.as_bytes())?;
        atomic_write(&tests.join(format!("{name}.out")), output.as_bytes())?;
        let mut m = self.load_manifest().unwrap_or_default();
        let key = match origin {
            CaseOrigin::Sample => "sample",
            CaseOrigin::Generated => "gen",
            CaseOrigin::Stress => "stress",
        };
        m.cases.insert(name.to_string(), key.to_string());
        self.save_manifest(&m)
    }

    /// 删除用例（.in/.out + manifest 记录）
    pub fn delete_case(&self, name: &str) -> Result<()> {
        for ext in ["in", "out"] {
            let p = self.tests_dir().join(format!("{name}.{ext}"));
            if p.exists() {
                std::fs::remove_file(&p)?;
            }
        }
        let mut m = self.load_manifest().unwrap_or_default();
        m.cases.remove(name);
        self.save_manifest(&m)
    }

    fn manifest_path(&self) -> PathBuf {
        self.tests_dir().join("manifest.toml")
    }

    fn load_manifest(&self) -> Result<Manifest> {
        let p = self.manifest_path();
        if !p.exists() {
            return Ok(Manifest::default());
        }
        Ok(toml::from_str(&std::fs::read_to_string(p)?)?)
    }

    fn save_manifest(&self, m: &Manifest) -> Result<()> {
        Ok(atomic_write(&self.manifest_path(), toml::to_string_pretty(m)?.as_bytes())?)
    }

    /// 保存评测报告 reports/<ts>.json
    pub fn save_report(&self, report: &shuati_core::TestReport) -> Result<PathBuf> {
        let name = format!("{}.json", report.timestamp);
        let path = self.reports_dir().join(name);
        atomic_write(&path, serde_json::to_string_pretty(report)?.as_bytes())?;
        Ok(path)
    }

    /// 保存对拍 hack case
    pub fn save_hack(&self, seed: u64, input: &str, actual: &str, expected: &str, note: &str) -> Result<PathBuf> {
        let base = self.stress_dir().join(format!("hack_{seed}"));
        atomic_write(&base.with_extension("in"), input.as_bytes())?;
        atomic_write(&base.with_extension("out"), actual.as_bytes())?;
        atomic_write(&base.with_extension("ans"), expected.as_bytes())?;
        let meta = serde_json::json!({ "seed": seed, "note": note, "ts": now_ts() });
        atomic_write(&base.with_extension("meta.json"), meta.to_string().as_bytes())?;
        Ok(base.with_extension("in"))
    }

    /// 默认解文件（solutions/main.<ext>），按语言
    pub fn default_solution(&self, lang: shuati_core::Lang) -> PathBuf {
        let ext = match lang {
            shuati_core::Lang::Cpp => "cpp",
            shuati_core::Lang::Python => "py",
        };
        self.solutions_dir().join(format!("main.{ext}"))
    }

    /// 找已有解：优先 solutions/main.*，否则目录下任何源码文件
    pub fn find_solution(&self) -> Option<PathBuf> {
        let sol_dir = self.solutions_dir();
        for ext in ["cpp", "py"] {
            let p = sol_dir.join(format!("main.{ext}"));
            if p.exists() {
                return Some(p);
            }
        }
        // 回退：目录内唯一源码文件
        let files: Vec<PathBuf> = std::fs::read_dir(&sol_dir)
            .ok()?
            .filter_map(|e| e.ok().map(|x| x.path()))
            .filter(|p| shuati_core::Lang::from_path(p).is_some())
            .collect();
        if files.len() == 1 { files.into_iter().next() } else { None }
    }
}

pub fn now_ts() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_secs() as i64)
        .unwrap_or(0)
}

/// 扫描工作区，加载全部题目（损坏的目录跳过并记日志）
pub fn load_all_problems(ws: &crate::Workspace) -> Vec<ProblemDir> {
    let mut out = Vec::new();
    if let Ok(dirs) = ws.list_problem_dirs() {
        for d in dirs {
            match ProblemDir::open(&d) {
                Ok(pd) => out.push(pd),
                Err(e) => eprintln!("[store] skip {}: {}", d.display(), e),
            }
        }
    }
    out
}
