# PRD｜智能刷题教练（CLI-First）

## 1. 产品概述（Overview）

### 1.1 产品名称 shuati CLI

**shuati CLI**

> 一个以 CLI 为核心的智能刷题教练，通过低成本 AI + 错误建模，帮助用户用最少时间真正提升算法能力。

---

### 1.2 一句话价值主张

> **不是帮你刷更多题，而是让你更少犯错、更快进步。**

---

### 1.3 核心理念

* **教练（Coach）而非题库**
* **错误驱动学习（Mistake-Driven Learning）**
* **低 Token、高反馈密度**
* **CLI 优先，GUI 后置**

---

## 2. 用户与使用场景

### 2.1 目标用户

* 算法学习者 / ACM / OI / 刷题党
* 大学生、程序员、技术面试准备者
* 已长期使用 LeetCode / 洛谷 / CF，但效率低

---

### 2.2 典型痛点

| 痛点    | 描述         |
| ----- | ---------- |
| 刷题盲目  | 不知道下一题刷什么  |
| 错误反复  | 同类错误多次出现   |
| AI 依赖 | 看解法但没吸收    |
| 题获取麻烦 | 多平台切换、复制粘贴 |

---

### 2.3 典型使用流程（MVP）

```text
1. 打开题目网页
2. coach pull <url>
3. 本地编码
4. coach submit solution.cpp
5. coach review
6. coach next
```

---

## 3. 产品范围（Scope）

### 3.1 包含内容（In Scope）

* CLI 工具
* 题目导入（URL / 本地）
* 解题提交
* 错误分析与提示
* 学习节奏推荐
* AI 辅助纠错（低 Token）

---

### 3.2 不包含内容（Out of Scope / 延后）

* 社区 / 排行榜
* 全量题库
* 在线判题系统
* 强社交功能

---

## 4. 功能需求（Functional Requirements）

---

### 4.1 CLI 系统

#### 4.1.1 基础命令

```bash
coach init
coach pull <url>
coach solve <problem_id>
coach submit <file>
coach review
coach next
coach stats
```

---

### 4.2 题源系统（Problem Source）

#### 4.2.1 URL 导入（MVP 核心）

* 支持复制网页 URL
* 自动抽取：

  * 题目描述
  * 输入 / 输出
  * 限制
  * 示例

输出统一格式：

```json
Problem {
  id
  source
  title
  statement
  input_desc
  output_desc
  constraints
  samples
}
```

---

#### 4.2.2 本地题目（出题人模式）

* 支持 Markdown
* 5 分钟出题
* 字段极简

---

#### 4.2.3 站点适配（Phase 2）

* LeetCode
* Codeforces
* luogu

---

### 4.3 解题与提交

* 本地代码文件
* 不依赖在线判题
* 支持多语言（c++ , python）

---

### 4.4 错误分析系统（核心）

#### 4.4.1 错误建模

```json
Mistake {
  type
  description
  frequency
  related_problems
}
```

#### 常见类型：

* off-by-one
* 状态定义错误
* 贪心误用
* 时间复杂度错误

---

### 4.5 AI 教练系统（DeepSeek）

#### 4.5.1 AI 角色

> **不是解题者，而是纠错教练**

#### 4.5.2 Prompt 范式（低 Token）

输入：

* 错误摘要
* 标签
* 失败点

输出：

* 指导性提示
* 不给完整解法

---

### 4.6 学习节奏与推荐

* 基于错误频率
* 基于遗忘曲线
* 输出下一题建议

```bash
coach next
```

---

## 5. 非功能需求（Non-Functional）

### 5.1 性能

* 单次 AI 调用 < 300 tokens
* CLI 响应 < 200ms（不含模型）

---

### 5.2 可扩展性

* 插件化题源
* AI Provider 可替换

---

### 5.3 可维护性

* 题源解析模块解耦
* Prompt 独立管理

---

## 6. 技术方案（高层）

### 6.1 架构概览

```text
CLI
 ├── Problem Source
 ├── Local Analyzer
 ├── AI Coach
 ├── Mistake Memory
 └── Storage
```

---

### 6.2 AI 调用策略

* 本地分析优先
* AI 仅用于“语义判断”
* DeepSeek API

---

## 7. 风险与对策

| 风险       | 应对     |
| -------- | ------ |
| 题源变更     | URL 兜底 |
| Token 成本 | 分段摘要   |
| 学习效果差    | 错误驱动   |

---

## 8. 里程碑（Milestones）

### Phase 1（2–3 周）

* CLI
* URL 导入
* 错误分析
* AI 教练

### Phase 2

* 站点适配
* 推荐系统

### Phase 3

* GUI / 插件
* 数据分析

---

## 9. 成功指标（Success Metrics）

* 连续使用 7 天
* 同类错误下降
* 用户主动推荐

---

## 10. 开源与未来

* MIT License
* CLI 优先
* 教练逻辑可复用

---

## 结语（给未来用户）

> **刷题不是为了 AC，而是为了不再犯同样的错。**