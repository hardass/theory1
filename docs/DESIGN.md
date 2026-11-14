# FM-1 SCALE DRILL — 设计文稿 v0.1（已由 SPEC-v1.0.md 取代）

> **注意**：本文件是第一版草稿，保留作历史。实现以 `SPEC-v1.0.md`（需求）+ `REVIEW-v1.0.md`（核查）+ `IMPL-NOTES.md`（偏差）为准；
> 与本文件不一致的地方（入口、型态定义、音阶库范围、灯光映射等）以那三份为准。

> 目标设备：M-VAVE FM-1（27 键带键灯、240×240 彩屏、4 旋钮 + SELECT 编码器、USB/BLE/TRS 三重 MIDI）
> 底座：THEORY-1（`github.com/wave00066/theory1`，MPL-2.0，基于 `fm1-base`）
> 状态：草案，只描述设计，不含实现 · 2026-11-14

## 0. 一句话

把 FM-1 从"乐理教学屏"扩成一台**音阶练习机**：选调 → 示范 → 灯指路 → 你弹 → 逐音判定 → 按表现自动升降速 → 达标换下一个调。设计不发明轮子，只搬音乐练习软件里已被验证的机制。

## 1. 问题定义

我有一台电钢琴（练习面，手感对）+ 一台 $80 的 FM-1（有**键下的灯**，这是唯一独特资产）。音阶的正确练法是公认的：慢速、跟节拍器、一次只处理一件事、错了立刻停。但自己练极难自控——没人喊停，也不知道现在该多快。

| 替代方案 | 为什么不够 |
|---|---|
| flowkey / Simply Piano / Synthesia / Piano Marvel | 订阅 $8–20/月；要屏 + 支架；练习时眼睛看屏、手在另一台键盘上；开机流程都是摩擦 |
| 硬件 Light Guide（Komplete Kontrol S 系列） | 就是"根音全亮、音阶音半亮"，但整机 $500–700，为练音阶买不合理 |
| 节拍器 + 纸质音阶表 | 无判定、无反馈、无进度 |
| 老师 | 一周一次，覆盖不了每天 |

**价值主张**：用已有设备把"灯在键上 + 能听你弹 + 会催你提速"这三件事做出来，成本 0，且不依赖屏幕。

**副产品**：这个练习引擎以后可复用到琶音、哈农、视奏，不用重写。

## 2. 竞品研究：好软件到底怎么设计的

拆成机制而不是功能列表，并标注在我们这台设备上的可移植性。

| # | 机制 | 证据 | 设计要点 | 可移植性 |
|---|---|---|---|---|
| 1 | **Wait Mode（弹对才走）** | flowkey 三大支柱之一（Loop / Select a Hand / Wait Mode）；Synthesia "wait for you"；Simply Piano 后补 | 宁可不前进，也不让错音建立肌肉记忆 | ✅ `note_in()` 已是统一入口 |
| 2 | **Auto-BPM 阶梯** | Melodics Practice Mode：稳就 +10 BPM；"Flow" = 难度贴着能力边缘 | 速度是音阶练习唯一有意义的进度轴 | ✅ 比 Melodics 更简单（只跑音阶，不必对齐谱面） |
| 3 | **准确率 → 离散奖牌** | Piano Marvel Play Mode：80 铜 / 90 银 / 96 金 / 100 超级金 | 离散等级比百分比更能催"再来一次" | ✅ 屏够放 4 档 |
| 4 | **Loop / 慢放 / 分手** | flowkey | 拆小、降速、减维度 | Loop ✅ 慢放 ✅ **分手 ❌**（单通道 MIDI 分不出左右手） |
| 5 | **Light Guide：根音全亮，其余音阶音半亮** | NI Komplete Kontrol："selected Root Note via fully lit LEDs, while other active keys are half-lit" | 两档亮度就够表达"骨干 vs 填充" | ✅ THEORY-1 已是这个语法（`mk.root` / `mk.scale`），几乎白送 |
| 6 | **Practice 与 Stage 分离** | Melodics：lessons 用来练，Stages 用来"证明"，按序解锁 | 练习给辅助，考试零辅助 | ✅ 一套引擎两套判定 |
| 7 | **参数化线性练习器** | 专项音阶 app 的参数就是：音阶 / 八度数 / 手 / 方向 | 音阶练习的最小参数集 | ✅ 除"手"外全可做 |
| 8 | **考级速度表当外部锚点** | ABRSM 2025–26：Initial ♩=54 / G1 60 / G2 66 / G3 80 / G4 100 / G5 60 / G6 72 / G7 80 / G8 88 | 给"我该多快"一个权威答案 | ⚠️ G5 后数字**下降**，因为要求变成双手 4 八度——**目标速度取决于型态，不是级别**（§7.3） |
| 9 | **反馈投射到硬件灯上** | Melodics：`if your instrument has light-up Pads or Keys it's displayed there as well` | 反馈要在手所在的地方 | ✅ 正是 FM-1 的强项 |
| 10 | **每日目标 / 连续天数** | Melodics Daily Goal、streak | 长期靠"别断签" | ⚠️ 优先级最低（屏画不下日历） |

**教学法侧证据**：

- 慢练 + 节拍器 + 均匀是所有教材的起点；提速通常 **10 BPM 一档**。
- **加维度就降速 ≥30%**（双手合奏约等于单手的 70%）。→ 规则应是"加一个维度先回退"，而非线性加速。
- **变奏**（附点、三连音、断连奏、反向）才是音阶真正的训练价值，而不是"能弹快"。
- **不要练错**：错误动作会被强化（Melodics 文档直说）。但"完全无错"也有代价——错误率过低意味着没在学。工程折中：**早期给提示，后期撤提示**（§7.1）。

## 3. 设计原则（冲突时按序取舍）

1. 不让用户重复错误，也不许他卡死：错了停住，3 秒给提示，2 次错自动降一级辅助。
2. 一次只考一个维度：V1 只判音高；时序判定是可选层（精度有限，§6.3）。
3. 提示必须能撤：灯是拐杖，四级淡出，否则学的是灯不是音阶。
4. 速度是唯一进度轴：不做 XP、金币、成就墙。
5. 结束要有明确判决：不给"87%"，给"这个调现在 ♩=72，G3 要求 80，差一档"。
6. 反馈落在手上：灯 > 屏 > 声音。屏只答"下一个是什么 / 我该多快"。
7. 短：默认一次 3–5 分钟。

## 4. 产品定义

### 4.1 用户故事

我坐下，插上电钢琴（或直接用 FM-1 的键），选 C 大调。它先示范一遍上下行。灯亮在 C 上，节拍器响，我弹 C——绿闪，下一个灯移到 D。我弹错成 D♭，两个键同时红闪，它停住等我。按对才继续。一轮跑完，屏上写 `96% / 错 1 / ♩=72 → 下一轮 ♩=80`，下一轮直接按 80 起。四轮后到 88，屏上写 `G3 达标`，问 `下一个调：G`。

### 4.2 会话结构

```
预热（当前 BPM × 1 轮，全辅助）
  ↓
阶梯（最多 3 轮，自动升降速）
  ↓
Stage（1 轮，零辅助，判分入档）  ← 按 SAVE，可选
  ↓
冷却（降速 30% × 1 轮）
```

### 4.3 两种模式

| | PRACTICE | STAGE |
|---|---|---|
| 辅助 | 4 级可调 | 强制 BLIND |
| 速度 | 自动阶梯 | 固定 |
| 错了 | 停下等 | 记 slip 继续 |
| 结果 | 只影响下一轮速度 | 写 best、决定解锁下一个调 |
| 对应 | Melodics lessons | Melodics Stages |

### 4.4 成功指标（可测）

| 指标 | 目标 |
|---|---|
| 首次可用 | 开机到跑完一轮 C 大调 < 90 秒，**全程看屏 ≤ 2 次** |
| 学习生效 | 4 周内 ≥3 个调的最佳 BPM 提升 ≥ 20 |
| **灯没变成拐杖** | BLIND 准确率 ≥ GUIDE 准确率的 80% |
| 可靠性 | 零变砖、零设置/成绩丢失 |

第三条是最关键的：撤掉灯就崩，说明做了个玩具。

### 4.5 非目标

- ❌ 视奏 / 五线谱教学（240×240 放不下）
- ❌ 左右手分开判定（单通道 MIDI 不含手别，物理上做不到）
- ❌ 力度/触键评价（硅胶键与电钢琴力度曲线不可比）
- ❌ 歌曲库、云同步、账号
- ❌ 游戏化（XP / 皮肤 / 成就）

## 5. 交互规格

### 5.1 入口

**LESSONS → 第 10 项 `DRILL`**。列表本来就滚动（`big_list()` 5 行可见 + 滚动条），加一项零成本，不动 5 行主菜单版式（5×39 px 已占满 221 px）。

### 5.2 控件映射

| 控件 | 设置页 | 运行中 |
|---|---|---|
| K1 / PRESETS | ROOT（12 调） | 当前根音（只读） |
| K2 | SCALE 组：CORE / MINOR / PENTA-BLUES / WORLD（§9 三层库） | 无效（防误触） |
| K3 | SHAPE：1 八度 / 2 八度 / 只上行 / 上下行（组内条目用 SELECT 上下移动） | 无效 |
| K4 | BPM 40–200 | 临时调速（下一轮生效） |
| SELECT | 在设置项间移动 | — |
| PLAY | 开始 / 重试 | 暂停 / 继续 |
| REC | 听示范（`play_scale()` 已有） | 重放示范 |
| EDIT | 切辅助等级 GUIDE/FRAME/ANCHOR/BLIND | 临时降一级（不存盘） |
| SAVE | 存设置 | 进入 STAGE 判定 |
| HOME | 返回 | 返回（结束本轮，不存成绩） |
| OCT± | 移灯窗（不改判定） | 同 |

### 5.3 屏幕（240×240，两半刷新）

```
┌──────────────────────────────────────────┐
│ C MAJOR            G3   ♩=72   G3目标80  │  header
├──────────────────────────────────────────┤
│   C    D    E    F    G    A    B    C'  │  音符行（当前高亮）
│   ●    ●    ●    ○    ●    ●    ●    ●   │  进度点
│   96%       错 1        WAIT             │  状态
│  ┌ 时序提示 ─────────────────────────┐   │
│  │ 你的 G3 比节拍器早了 40 ms         │   │ 默认关
│  └───────────────────────────────────┘   │
│  屏上键盘（与灯同步）                     │
└──────────────────────────────────────────┘
```

### 5.4 状态机

```
IDLE ──PLAY──► COUNTIN(1 小节) ──► RUN ──(型态跑完)──► VERDICT ──► RUN'
  ▲                                 │
  │                                 ├──(错音 & WAIT)──► HOLD(等正确键) ──┘
  │                                 └──(错音 & STAGE)──► 记 slip 继续
  └──────── HOME ───────────────────┴──(会话结束)──► SUMMARY
```

`COUNTIN` 灯全灭（制造安静）；`VERDICT` 停 1.5 s；`HOLD` 最长 8 s，超时按"跳过本轮"处理（不记成绩）。

## 6. 判定引擎

### 6.1 期望序列

由 `(scale, root, shape)` 生成：

```c
struct drill_note {
    uint8_t pc;    /* 音高类 0-11：FREE 判定 */
    uint8_t midi;  /* 绝对音高：STRICT 判定 + 映射 27 键 */
    uint8_t deg;   /* 音级 0..n-1：显示与统计 */
    uint8_t dir;   /* 上行 1 / 下行 0 */
};
```

1 八度上行 = n+1 个（含回到主音的八度）；上下行 = 2n+1。7 声 2 八度上下行 = 29 个音。

### 6.2 判定规则

| 情况 | 处理 |
|---|---|
| 音高类 == 期望（FREE） | 正确，推进 |
| 绝对音高 == 期望（STRICT） | 同上 |
| 音高类不在期望里 | 错音：`errors++`，红闪**你按的键 + 应为的键**；WAIT 则停住 |
| 音高类 == 刚弹过的 | 忽略（重按不算错） |
| 音高类在音阶里但顺序不对 | 错音（跳音），提示"不是下一个" |
| 8 秒无输入 | 重放示范一次（借鉴 FM1 Keyboard Academy） |

FREE 是默认：主键盘是 88 键电钢琴，跨八度弹同一个音不该算错。STRICT 用于"这个八度位置也要对"的进阶练习。

### 6.3 时序判定（可选层，默认关）

记录 `t_note` 与最近节拍点 `t_click`，归一化到拍长：`|Δ| < 0.25 拍` → OK；`Δ<0` 早；`Δ>0` 晚。

**精度上限（诚实声明）**：输入是主循环轮询（音频块 256 帧 ≈ 5.8 ms），MIDI 包**无时间戳**。只能到 ±20 ms 级粗糙判断，**不能**声称做到 Melodics 那种毫秒级判分。默认关，开启后只给"偏早/偏晚"趋势。

## 7. 教学算法

### 7.1 四级辅助淡出

| 级别 | 灯 | 屏 | 用途 |
|---|---|---|---|
| **GUIDE** | 下一个音全亮，音阶内其余中亮，根音另色 | 显示音名 | 不认识音阶时 |
| **FRAME** | 同上但不显示音名 | 只显示第几个音 | 逼你看键不看字 |
| **ANCHOR** | 只有每个八度的根音亮 | — | 记锚点 |
| **BLIND** | 全灭；**只在弹错时**闪正确键 | — | 检验 |

这是 errorless → error-based 的过渡，也是 §4.4 第三条指标的来源。

### 7.2 速度阶梯

```
一轮结束：
  acc = 正确数 / 期望数（WAIT 模式下"停下等"扣分）
  if acc == 100% and waits == 0:  bpm += 8
  elif acc < 70%:                 bpm -= 8
  else:                           不变
  bpm = clamp(bpm, 40, 200)
```

补充：

- **加维度先降 30%**：1 八度 → 2 八度、或 GUIDE → BLIND，`bpm = max(40, bpm*0.7)`
- **只在 STAGE 存 best**，PRACTICE 的升降不写记录，否则辅助下的虚假高分污染数据
- **阶梯有上限**：到达该 SHAPE 的考纲目标后不再自动加速，改为提示"换下一个调 / 变换奏"

### 7.3 目标速度表（外部锚点）

采用 ABRSM 2025–26 音阶速度（♩=），但**按型态取数**，不按级别硬套：

| 型态 | 适用级别 | 目标 BPM |
|---|---|---|
| 1 八度，单手 | Initial–G2 | 54 / 60 / 66 |
| 2 八度，单手 | G3–G4 | 80 / 100 |
| 双手同步（本设计不判手，仅提示） | G5+ | 60–88 |

因为 G5 后要求变成双手 4 八度，数字反而下降。**把级别当自变量是错的，型态才是。**

### 7.4 变奏（V1.1，先不做）

- 附点 / 三连音：改期望时值
- 断奏：note-on→note-off 长度 `gate < 0.3 拍`，可粗糙判
- 反向运动：**只能提示不能判**（单通道 MIDI 分不出手）

## 8. 硬件约束（诚实清单）

| 约束 | 事实 | 影响 |
|---|---|---|
| 键盘 | 27 键 **F3–G5（MIDI 53–79）**，16 白 11 黑 | 2 八度正合适；更长靠 `OCT±` 移窗 |
| 灯 | **三级亮度**（bright/mid/dim） | 够表达 5 种语义（错音与期望都红，靠闪区分） |
| 屏 | 240×240，两半分区刷新；字形 ASCII + 重音后绘 | 文案必须短；不做中文 i18n |
| 输入延迟 | 轮询 ≈ 5.8 ms/次，MIDI 无时间戳 | 时序判定只能粗粒度 |
| 发音 | 内置喇叭；synth 有钢琴/电钢/风琴/铃 | 节拍器用短促高音；示范复用现有音色 |
| 电钢琴 | 88 键，MIDI 单通道 | "双手"判不了；音域远超 FM-1，必须 FREE + 移窗 |
| 存储 | 设置扇区 4 KB；`settings_t` 现为 ~36 B | 22 音阶 × 12 调 × 3 型态 × 1 B = 792 B，余量充足，无需稀疏表 |

## 9. 音阶库（复用 LESSONS 的 22 个音阶）

已核实：`theory.c` 的 `SCALES[NSCALES]` **已有 22 个音阶**，结构 `{name[NLANGS], n, semi[12], let[12], text[NLANGS]}`，`NSCALES = 9 基础 + 13 世界/对称`。判定引擎只需要 `n` 和 `semi[]`，所以**drill 复用它们的数据成本为零**，5 语言名称和讲解全部白送。

- 基础 9：major、natural minor、harmonic minor、melodic minor、penta major、penta minor、blues、whole-tone、chromatic
- 世界 13：octatonic、enigmatic、double harmonic、hungarian、purvi、marwa、todi、insen、yo、hirajoshi、kumoi、iwato、ryukyu

但"能练"不等于"同一种练法"。按**有没有外部速度锚点**分三层：

| 层 | 音阶 | 目标 BPM | 达标判定 |
|---|---|---|---|
| **CORE**（有锚点） | major / natural minor / harmonic minor / chromatic / whole-tone | ABRSM | 有 |
| **BUILT**（无锚点） | melodic minor（爵士）/ penta×2 / blues / 世界 13 | 无，"练到哪算哪" | 无，只记个人 best |
| **不进 DRILL** | 调式 21 个（`MODES[]` 已存在） | — | 那是 LESSONS 的脑，DRILL 只练手 |

ABRSM 只对这 4 类给速度，其余 18 个硬套就是编数字。

### 9.1 三个必须处理的差异（有代码证据）

1. **旋律小调上下行不对称**：古典旋律小调上行升 6/7、下行回自然小调；而 `play_scale()` 正反两遍用同一组 `semi[]`。要么沿用并明确标 **"jazz melodic minor"**（爵士确实上下同形，是合法解释），要么为它单独加下行表。**不能默默当成古典旋律小调。**
2. **每轮长度差 2 倍**：`n` 从 5（五声）到 12（半音）。同 BPM 下一轮 11 s vs 25 s，"1 八度 / 2 八度"对五声与半音并不对称。→ 型态按"每八度 n+1 个音"归一，不按绝对音数。
3. **旋钮选不过来**：K2 从 9 项变 22 项。改为**两级选择**（§5.2）：K2 转组，SELECT 转组内条目，另留"最近 4 个"。

### 9.2 存储

22 × 12 调 × 3 型态 = 792 格，每格 1 B（BPM 40–200 装得下 u8，0 = 无记录）→ **792 B**。设置扇区 4 KB，一次会话末写一次。

### 9.3 交付节奏

- **V1**：CORE 5 个 × 12 调 × 3 型态（有锚点、能判"达标"）——产品承诺不膨胀
- **V1.1**：打开 BUILT（+17 个音阶，只记 best）——"想练哪个都有"，但不背"达标"包袱
- **V1.2**：变奏（附点 / 三连音 / 断奏）

调的顺序按五度圈 `C G D A E B F# D♭ A♭ E♭ B♭ F`（与 THEORY-1 的调号课一致，避免认知冲突）；每调一张卡：当前 BPM / best / 目标 / 达标标记。

**成本影响**：`texts.c` 新增从"约 25 条"降到"约 20 条"——音阶名与讲解已在 `SCALES[].name/text` 里，不需要重写；`theory.c` 仍然零改动。

## 10. 实现映射

| 需要 | 现成的 | 新增 |
|---|---|---|
| 音符输入（内部键 + MIDI） | `note_in(note, vel, on, outside)` | 加 `SCR_DRILL` 分支 |
| 键盘灯三级 | `ui_lights()` + `plat_lights/mid` | 不改，drill 只写 `mk.*` |
| 屏上键盘 | `ui_keyboard()` | 不改 |
| 音阶数据/拼写 | `SCALES[22]`、`scale_note()`、`degree_labels()` | 不改（连 5 语言名称与讲解都复用，§9） |
| 示范 | `play_scale(s, root, updown)` | 不改 |
| 定时/节拍器 | `plat_ms()` + `play_ev/play_tick()` | 加 `drill_tick()` |
| 顺序判题骨架 | `q_note()` 的 `K_SEQ` | 抽成 `drill_note()`，逻辑同源 |
| 列表 UI | `big_list()`、`note_row()` | 复用 |
| 5 语言文案 | `texts.c` 的 `S()` / `SSTR[][]` | 约 20 条 × 5 语言（音阶名与讲解已存在） |
| 无硬件测试 | `app/tests/*.txt` + `app_host_cmd()` | 新增 `drill.txt` + `drill` 命令 |
| 出包 | `.github/workflows/release.yml`（CI 拉 JieLi SDK） | 不改 |

改动面：`app.c`（主要）、`texts.c`、`app/tests/`、`README.md`。**`theory.c` 零改动**——这是方案风险低的根本原因。

## 11. 测试计划

| 层 | 内容 | 手段 |
|---|---|---|
| 单元 | 期望序列生成（各型态长度、首尾音） | 纯 C，仿 `theory_test.c` |
| 集成 | 起步 → 弹错 → HOLD → 弹对 → 一轮结束 → 提速 → BLIND → 达标 | 新增 `app/tests/drill.txt` |
| 文案 | 25 条 × 5 语言不溢出 | `gen_tests.py` 重生成 `texts_fit.txt` |
| 回归 | 上游 5 组测试全绿 | `sh tests/run_tests.sh` |
| 视觉 | 各状态截图 | host 模拟器导出 PNG |
| 真机 | 装完跑 C 大调一轮，确认灯序、延迟、音量 | 人工 |

## 12. 里程碑与成本

| 阶段 | 内容 | 预估 |
|---|---|---|
| P0 | 克隆上游 + host 模拟器跑绿 | 0.5 天 |
| P1 | 状态机 + 判定 + 灯（含测试） | 2–3 天 |
| P2 | 设置页 + 版式 + 5 语言 + 文案测试 | 1–2 天 |
| P3 | 节拍器、速度阶梯、成绩存盘、STAGE | 1–2 天 |
| P4 | 出包 + 上机验证 + 文档 | 1 天 |
| **合计** | | **约 6–9 人日** |

运行成本 0，金钱成本 0（设备已有）。

## 13. 风险与缓解

| 风险 | 等级 | 缓解 |
|---|---|---|
| **变砖**（2026-11 起新批次换芯片，屏下有 M-VAVE logo 的那批） | 高 | 刷前确认批次；准备 V15 官方包 + `fm1-base` 安全模式恢复 |
| 灯被当拐杖 | 中 | 四级淡出 + BLIND 指标 |
| 上游漂移（THEORY-1 仍活跃） | 中 | 基于 `main@92b9ab9`；改动集中在 `app.c`；必要时长期独立分支 |
| 时序精度不足被当 bug | 中 | 默认关；文案写"粗粒度趋势" |
| 5 语言文案质量 | 低 | 英/意自查，西/法/德标注待母语审 |
| CI 依赖（本机无 JieLi 工具链） | 中 | 需 GitHub 仓库 + token；否则停在源码 + 模拟器截图 |
| MPL-2.0 义务 | 低 | 自家仓库公开源码即可；不得闭源分发 |

## 14. 待决问题

1. **CI 路径**：给一个 GitHub 空仓库 + `repo` token（推荐），还是先只做源码 + 模拟器截图？
2. **主键盘是谁**：电钢琴（→ 默认 FREE、灯作镜像、屏给细节），还是 FM-1 的 27 键（→ 可默认 STRICT）？
3. **入口**：LESSONS 第 10 项，还是主菜单第 6 项（需压行高到 32 px）？
4. **要不要把考纲目标写进屏**（`G3目标80`）？会不会让人觉得被考试绑架？
5. **西/法/德文案谁审**？还是先只上英/意？
6. 会话是否**强制结束**（3–5 分钟到点提示收工）？
7. **旋律小调**：按"爵士同形"（上下行一致，默认，零成本）还是额外做下行表（还原古典语义）？
8. **音阶库范围**：V1 只开 CORE 5 个，还是直接 22 个全开（只为 BUILT 记 best、不判达标）？

## 附录 A：参考来源

- flowkey 功能（Loop / Select a Hand / Wait Mode）：`play.google.com/store/apps/details?id=com.flowkey.app`
- Melodics How it works：`melodics.com/how-it-works`
- Melodics Practice Mode（"错音会被强化"、+10 BPM）：`support.melodics.com/en/articles/6777027-practice-mode`
- Melodics 进度四系统（Lessons vs Stages）：`support.melodics.com/en/articles/15091765`
- Piano Marvel Play Mode 奖牌阈值：`pianomarvel.com/en/feature/play-mode`
- NI Komplete Kontrol Light Guide：`docs.native-instruments.com/online-guides/kontrol-s-mk3-manual/en/scale`
- ABRSM 2025 & 2026 钢琴考纲（音阶速度表）：abrsm.org 官方 PDF
- 音阶练法：`piano.org/theory/scale-practice/`、`truemetronome.app/blog/practice-scales-with-metronome/`、`pianote.com/blog/contrary-motion-scales/`
- 无错学习 vs 纠错：`bulletproofmusician.com/how-important-is-it-to-perform-a-new-skill-without-errors-from-day-1/`
- 底座源码：`github.com/wave00066/theory1`（MPL-2.0）、`github.com/charlesvestal/fm1-base`
- 硬件：`cuvave.com/product?id=fm-1`

## 附录 B：本机已核实的代码事实

- 键位：`ui_input()` 注释 `key 0 is F3 (MIDI 53)`，27 键 → MIDI 53–79 = F3–G5
- 灯：`plat_lights(btn, bright, btn_live, dim, 1)` + `plat_lights_mid(0, mid)`，三档
- 语义遮罩：`mk.scale / mk.root / mk.ok / mk.bad / mk.hint`（`ui_keyboard()` 与 `ui_lights()` 共用）
- 顺序判题：`q_note()` 的 `K_SEQ`（`q.pos` 推进、`q.wrong_pc` 红标、3 次错给提示）
- 示范：`play_scale(const scale_t*, nt_t root, int updown)`
- 输入：`note_in(note, vel, on, outside)`；MIDI 经 `midi_in()` 走同一入口
- 时钟：`plat_ms()`；定时音符 `play_ev()` + `play_tick()`（`ui_input()` 尾调用）
- 音频块：`APP_BLOCK 256` 帧 ≈ 5.8 ms @44.1 kHz
- 测试 DSL：`wait / turn / tap / seed / solve / wrong / shot`，另有 `app_host_cmd()` 可扩命令
- 存储：`APP_STORE_MAP {0xFC000 设置, 0xDC000 面板, 0xDE000 蓝牙}`，`settings_t` 有余量
