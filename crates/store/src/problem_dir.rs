use crate::{Result, StoreError, atomic_write};
use serde::{Deserialize, Serialize};
use shuati_core::{CaseOrigin, CheckerSpec, Limits, Problem, ProblemId, TestCase};
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
        let text =
            std::fs::read_to_string(&meta_path).map_err(|e| StoreError::Invalid(meta_path.clone(), e.to_string()))?;
        let t: ProblemToml = toml::from_str(&text)?;
        let id = ProblemId::parse(&t.id)
            .ok_or_else(|| StoreError::Invalid(meta_path.clone(), format!("bad id {}", t.id)))?;
        // spj 程序路径按题目目录解析，与进程 CWD 解耦
        let checker = match t.checker {
            CheckerSpec::Spj { program } if program.is_relative() => CheckerSpec::Spj { program: dir.join(program) },
            c => c,
        };
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
                checker,
                created_at: t.created_at,
            },
        })
    }

    /// 创建新题目目录（含 problem.toml / statement.md / 子目录）。
    /// 已存在 problem.toml 时报错，不静默覆盖用户数据。
    pub fn create(dir: impl Into<PathBuf>, meta: &Problem) -> Result<Self> {
        let dir = dir.into();
        if dir.join("problem.toml").exists() {
            return Err(StoreError::Exists(dir));
        }
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
            // spj 路径落盘时收敛回相对路径：题目目录整体可搬迁/同步
            checker: match &self.meta.checker {
                CheckerSpec::Spj { program } => match program.strip_prefix(&self.dir) {
                    Ok(rel) => CheckerSpec::Spj { program: rel.to_path_buf() },
                    Err(_) => self.meta.checker.clone(),
                },
                c => c.clone(),
            },
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
            let expected = if out_path.exists() { Some(std::fs::read_to_string(&out_path)?) } else { None };
            let origin = match manifest.cases.get(&name).map(|s| s.as_str()) {
                Some("gen") => CaseOrigin::Generated,
                Some("stress") => CaseOrigin::Stress,
                _ => CaseOrigin::Sample,
            };
            cases.push(TestCase { name, input, expected, origin });
        }
        Ok(cases)
    }

    /// 轻量用例数：只列 tests/ 目录数 *.in，不读文件内容（list/拉题提示用）
    pub fn count_cases(&self) -> usize {
        std::fs::read_dir(self.tests_dir())
            .ok()
            .into_iter()
            .flatten()
            .filter_map(|e| e.ok())
            .filter(|e| e.path().extension().is_some_and(|x| x == "in"))
            .count()
    }

    /// 写入用例文件。output=None 时不写 .out（无答案的 smoke 用例）
    pub fn write_case(&self, name: &str, input: &str, output: Option<&str>, origin: CaseOrigin) -> Result<()> {
        let tests = self.tests_dir();
        atomic_write(&tests.join(format!("{name}.in")), input.as_bytes())?;
        if let Some(out) = output {
            atomic_write(&tests.join(format!("{name}.out")), out.as_bytes())?;
        }
        let mut m = self.load_manifest().unwrap_or_default();
        let key = match origin {
            CaseOrigin::Sample => "sample",
            CaseOrigin::Generated => "gen",
            CaseOrigin::Stress => "stress",
        };
        m.cases.insert(name.to_string(), key.to_string());
        self.save_manifest(&m)
    }

    /// 下一个可用用例名：取 tests/*.in 数字 stem 的最大值+1（防删中间后覆盖）
    pub fn next_case_name(&self) -> String {
        let next = std::fs::read_dir(self.tests_dir())
            .ok()
            .into_iter()
            .flatten()
            .filter_map(|e| e.ok())
            .filter_map(|e| e.path().file_stem().map(|s| s.to_string_lossy().to_string()))
            .filter(|n| self.tests_dir().join(format!("{n}.in")).exists())
            .filter_map(|n| n.parse::<u32>().ok())
            .max()
            .unwrap_or(0)
            + 1;
        format!("{next:04}")
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

    /// 保存评测报告 reports/<ts>-<nanos>.json（纳秒后缀防同秒覆盖）
    pub fn save_report(&self, report: &shuati_core::TestReport) -> Result<PathBuf> {
        let nanos =
            std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.subsec_nanos()).unwrap_or(0);
        let name = format!("{}-{:09}.json", report.timestamp, nanos);
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
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
}

/// 扫描工作区，加载全部题目（损坏的目录跳过并记日志）
pub fn load_all_problems(ws: &crate::Workspace) -> Vec<ProblemDir> {
    let mut out = Vec::new();
    if let Ok(dirs) = ws.list_problem_dirs() {
        for d in dirs {
            match ProblemDir::open(&d) {
                Ok(pd) => out.push(pd),
                Err(e) => tracing::warn!("skip {}: {}", d.display(), e),
            }
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tmp_pd() -> (tempfile::TempDir, ProblemDir) {
        let d = tempfile::tempdir().unwrap();
        let meta = Problem {
            id: ProblemId::new(shuati_core::Source::Local, "t1"),
            title: "t".into(),
            url: String::new(),
            difficulty: None,
            tags: vec![],
            limits: Default::default(),
            statement: PathBuf::from("statement.md"),
            checker: Default::default(),
            created_at: now_ts(),
        };
        let pd = ProblemDir::create(d.path().join("p"), &meta).unwrap();
        (d, pd)
    }

    #[test]
    fn create_twice_errors_not_overwrites() {
        let (_d, pd) = tmp_pd();
        let r = ProblemDir::create(&pd.dir, &pd.meta);
        assert!(matches!(r, Err(StoreError::Exists(_))));
    }

    #[test]
    fn next_case_name_survives_gaps() {
        let (_d, pd) = tmp_pd();
        assert_eq!(pd.next_case_name(), "0001");
        pd.write_case("0001", "in1", Some("out1"), CaseOrigin::Sample).unwrap();
        pd.write_case("0003", "in3", Some("out3"), CaseOrigin::Sample).unwrap();
        // 删掉 0002 的中间空洞下，新用例取 max+1 而不是 count+1（防覆盖 0003）
        assert_eq!(pd.next_case_name(), "0004");
    }

    #[test]
    fn write_case_without_out_loads_none() {
        let (_d, pd) = tmp_pd();
        pd.write_case("0001", "1 2", None, CaseOrigin::Generated).unwrap();
        let cases = pd.load_cases().unwrap();
        assert_eq!(cases.len(), 1);
        assert_eq!(cases[0].expected, None);
        assert!(!pd.tests_dir().join("0001.out").exists());
    }

    #[test]
    fn report_filenames_are_unique() {
        let (_d, pd) = tmp_pd();
        let rep = shuati_core::TestReport {
            problem: pd.meta.id.clone(),
            timestamp: now_ts(),
            kind: shuati_core::RunKind::Test,
            verdict: shuati_core::Verdict::Ac,
            total: 0,
            pass: 0,
            cases: vec![],
        };
        let a = pd.save_report(&rep).unwrap();
        let b = pd.save_report(&rep).unwrap();
        assert_ne!(a, b);
        assert!(a.exists() && b.exists());
    }

    #[test]
    fn spj_relative_path_resolves_under_problem_dir() {
        let (_d, mut pd) = tmp_pd();
        pd.meta.checker = CheckerSpec::Spj { program: PathBuf::from("checkers/wcmp") };
        pd.save_meta().unwrap();
        let pd2 = ProblemDir::open(&pd.dir).unwrap();
        match pd2.meta.checker {
            CheckerSpec::Spj { program } => {
                assert!(program.is_absolute());
                assert_eq!(program, pd.dir.join("checkers/wcmp"));
            }
            c => panic!("expect spj, got {c:?}"),
        }
        // round-trip：写盘时收敛回相对路径，problem.toml 不记宿主绝对路径
        let text = std::fs::read_to_string(pd.dir.join("problem.toml")).unwrap();
        assert!(text.contains("checkers/wcmp"), "{text}");
        assert!(!text.contains(pd.dir.to_str().unwrap()), "{text}");
    }
}
