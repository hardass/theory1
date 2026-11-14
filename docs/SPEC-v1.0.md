# FM-1 SCALE DRILL — 工程与教学法完整规格书 v1.0

> **来源**：由 Boss 与另一位资深 agent 共同梳理定稿，2026-11-14。
> **性质**：Spec-for-Implementation。含数据结构、状态转移、边界条件、算法公式、指法查找表、Flash 布局与测试脚本规范。
> **本文件为原文存档**；对原文的核查意见与修订见 `REVIEW-v1.0.md`（不改动本文件，便于追溯）。

目标硬件：M-VAVE FM-1（27 键 F3–G5 / MIDI 53–79，三档键下 LED，240×240 彩屏两半分区刷新，4 旋钮 + SELECT 编码器，USB/BLE/TRS MIDI）
软件底座：THEORY-1（github.com/wave00066/theory1，MPL-2.0，基于 fm1-base）

## 1. 核心目标与系统边界

### 1.1 产品定位

将 FM-1 从"静态乐理教学屏"扩展为专业级键盘音阶练习机（Scale Drill Engine）。以 88 键外接电钢琴为主力练习键盘（亦兼容 FM-1 本机 27 键独立练习），FM-1 担任灯塔（Key LEDs）、大脑（状态机与判定）、节拍器与考官。

### 1.2 必须解决的四大教学痛点（核心验收标准）

1. 阻断错误肌肉记忆（Wait Mode）：练习模式下弹错立刻双键红灯闪烁并停住，按对才推进。
2. 抓取穿指/跨指卡顿（IOI Evenness）：绕开硬件 5.8ms 轮询延迟限制，通过相邻音时间间隔（Inter-Onset Interval）相对偏差，精准抓出大拇指穿指（Thumb-under）或 3/4 指跨指（Cross-over）时的停顿与抢拍。
3. 动态指法提示与最高音封顶：显示 12 调左右手标准指法（1–5），自动处理最高音 5 指封顶（不穿指）与下行折返逻辑，并动态标记物理穿指/跨指点。
4. 防"打地鼠依赖"自动脱盲：GUIDE 模式下连续跑对 1 轮后，自动推进至 FRAME / ANCHOR / BLIND，迫使视线离开 FM-1 微缩键盘，回到电钢琴真实键距上建立本体感觉。

### 1.3 严格的非目标（Out of Scope，编码时严禁实现）

- ❌ 五线谱渲染（240×240 分辨率不支持）
- ❌ 双手同弹自动分轨（单通道 MIDI 物理上无法区分左右手，改为手动选择 HAND: R / L 切换指法表）
- ❌ 触键力度（Velocity）评分
- ❌ 任何 XP、金币、连续打卡日历等游戏化元素

## 2. 宿主代码库接口契约（Host API Contract）

Agent 在修改代码前，必须严格遵守 THEORY-1 现有接口，严禁修改 theory.c 核心数据结构：

| 宿主符号 / 文件 | 现有签名或规格 | 本模块调用与约束 |
|---|---|---|
| `SCALES[NSCALES]` (theory.h) | 包含 22 个音阶（索引 0..8 为基础音阶，9..21 为世界音阶） | 只读引用 `name`、`n`、`semi[]`；古典旋律小调下行表在 `drill_engine.c` 中独立静态定义 |
| `note_in(uint8_t note, uint8_t vel, int on, int outside)` (app.c) | 统一按键入口（本机 27 键与外部 MIDI 均汇入此函数） | 在 `scr == SCR_DRILL` 时拦截 `on && vel > 0` 事件转发给 `drill_on_note_on(note, outside)` |
| `ui_lights()` / `mk.*` (app.c) | `mk.scale`, `mk.root`, `mk.ok`, `mk.bad`, `mk.hint` 五组 27-bit 掩码 | `drill_render_lights()` 直接填充 `mk.*` 掩码，由宿主统一驱动三档硬件 LED（bright/mid/dim） |
| `ui_keyboard()` (app.c) | 渲染底部 240px 宽的 27 键虚拟键盘 | 直接复用，与 `mk.*` 掩码严格同步 |
| `play_scale(const scale_t *s, nt_t root, int updown)` | 播放音阶示范 | 仅在 IDLE 按 REC 或超时重播时调用；进入 COUNTIN 前必须调用 `play_stop()` |
| `play_ev()` / `play_tick()` | 音频事件调度器 | `drill_tick(uint32_t now_ms)` 挂载于 `ui_input()` 尾部，利用短促高音（如 MIDI 88/96，时长 25ms）实现节拍器滴答声 |
| `plat_ms()` | 单调递增毫秒时间戳（步进约 5.8ms，对应 256 帧 @44.1kHz） | 用于状态超时与 IOI 相邻音间隔计算 |
| `APP_STORE_MAP` (0xFC000) | 4KB Flash 设置扇区，`settings_t` 当前约 36 Bytes | 在 `settings_t` 末尾追加 `drill_save_t`（固定 1160 Bytes），总大小 < 1.3KB，安全裕量充足 |

## 3. 核心数据结构定义（drill_engine.h）

```c
#ifndef DRILL_ENGINE_H
#define DRILL_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#define DRILL_MAX_SEQ       64   /* 最大单轮音符数：12音阶(半音) * 2八度 * 2(上下行) + 1 = 49 < 64 */
#define DRILL_NUM_SCALES    22   /* 与 theory.c NSCALES 一致 */
#define DRILL_NUM_ROOTS     12   /* 按五度圈排列: C,G,D,A,E,B,F#,Db,Ab,Eb,Bb,F */
#define DRILL_NUM_SHAPES    3    /* 0: 1八度上下行, 1: 2八度上下行, 2: 2八度仅上行 */

/* 辅助等级（四级淡出） */
typedef enum {
    ASSIST_GUIDE  = 0, /* 亮目标音(全亮)+音阶音(中亮)+根音；屏显音名+指法 */
    ASSIST_FRAME  = 1, /* 亮灯同上；屏隐藏音名，仅显指法数字与进度点 */
    ASSIST_ANCHOR = 2, /* 仅亮八度根音；屏隐藏音名与指法 */
    ASSIST_BLIND  = 3  /* 全灭（仅错音闪红）；屏仅显速度与均匀度 */
} drill_assist_t;

/* 练习模式 */
typedef enum {
    MODE_PRACTICE = 0, /* 自动阶梯调速 + Wait Mode 错音停等 */
    MODE_STAGE    = 1  /* 强制 BLIND + 固定 BPM + 错音记 slip 不停 + 写入最佳成绩 */
} drill_mode_t;

/* 状态机状态 */
typedef enum {
    DRILL_ST_IDLE    = 0, /* 参数设置与等待开始 */
    DRILL_ST_COUNTIN = 1, /* 1小节（4拍）预备拍，灯全灭 */
    DRILL_ST_RUN     = 2, /* 正常运行等待当前音符 */
    DRILL_ST_HOLD    = 3, /* Wait Mode 错音停等状态（双键红闪） */
    DRILL_ST_VERDICT = 4, /* 单轮结束判决展示（停留 1500ms） */
    DRILL_ST_SUMMARY = 5  /* 会话总结 / 考纲达标提示与换调询问 */
} drill_state_t;

/* 单音期望节点 */
typedef struct {
    uint8_t pc;       /* 音高类 0..11 (C=0 .. B=11) */
    uint8_t midi;     /* 基准 MIDI 音高（以 C3=48 为基准八度计算，用于计算相对音程） */
    uint8_t finger;   /* 标准指法 1..5 (1=拇指 .. 5=小指) */
    uint8_t is_turn;  /* 1=物理穿指(Thumb-under)或跨指(Cross-over)关键动作点，0=普通顺指 */
    uint8_t dir;      /* 1=上行, 0=下行 */
} drill_note_t;

/* 持久化存储结构（存入 settings_t，总计 1160 Bytes） */
typedef struct __attribute__((packed)) {
    uint16_t magic;          /* 固定魔数 0x4452 ("DR")，校验未初始化 Flash */
    uint8_t  last_root_idx;  /* 0..11 (五度圈索引) */
    uint8_t  last_scale_idx; /* 0..21 */
    uint8_t  last_shape;     /* 0..2 */
    uint8_t  last_hand;      /* 0=右手(R), 1=左手(L) */
    uint8_t  last_assist;    /* 0..3 */
    uint8_t  ioi_tolerance;  /* 0=宽松(45%), 1=标准(35%, 默认), 2=严格(25%) */
    /* 成绩矩阵：22音阶 * 12调 * 3型态 = 792 格 */
    uint8_t  best_bpm[DRILL_NUM_SCALES][DRILL_NUM_ROOTS][DRILL_NUM_SHAPES]; /* 0=无记录, 40..200 */
    /* 附加状态位图：bit[1:0]=达到best_bpm时的subdivision(1/2/4), bit[2]=STAGE盲弹达标标记 */
    uint8_t  best_flags[DRILL_NUM_SCALES][DRILL_NUM_ROOTS][DRILL_NUM_SHAPES]; /* 396B 压缩或 792B，此处用 360B 对齐或直接 396B */
} drill_save_t;

/* 运行时上下文 */
typedef struct {
    /* 配置参数 */
    uint8_t        root_idx;     /* 0..11 五度圈索引 */
    uint8_t        scale_idx;    /* 0..21 */
    uint8_t        shape;        /* 0..2 */
    uint8_t        hand;         /* 0=R, 1=L */
    drill_assist_t assist;
    drill_mode_t   mode;
    uint8_t        bpm;          /* 40..200 */
    uint8_t        subdivision;  /* 1, 2, 或 4 (音/拍) */
    uint8_t        oct_shift;    /* -1, 0, +1 (OCT± 调整 27 键灯映射窗口) */

    /* 序列数据 */
    drill_note_t   seq[DRILL_MAX_SEQ];
    uint8_t        seq_len;
    uint8_t        pos;          /* 当前期望音符索引 0..seq_len-1 */

    /* 状态机与计时 */
    drill_state_t  state;
    uint32_t       state_enter_ms;
    uint32_t       last_tick_ms;
    uint8_t        countin_beats;/* 0..4 */
    uint32_t       last_note_ms; /* 上一个正确音按下时刻（用于 IOI 计算） */
    uint8_t        prev_ok_midi; /* 上一个正确按下的实际 MIDI（用于防抖） */

    /* 相对八度追踪 */
    bool           octave_locked;
    int8_t         octave_offset;/* played_midi - seq[0].midi */

    /* 单轮统计与诊断 */
    uint8_t        errors;       /* 音高错误次数 */
    uint8_t        waits;        /* 触发 Wait Mode 停等次数 */
    uint8_t        hesitations;  /* 穿指/普通卡顿次数 (Δt > target * (1 + tol)) */
    uint8_t        rushes;       /* 抢拍次数 (Δt < target * (1 - tol)) */
    bool           turn_hesitated; /* 本轮卡顿是否发生在 is_turn==1 的穿指/跨指点 */
    uint8_t        last_wrong_midi;/* 最近一次按错的键（用于红灯闪烁） */
    uint8_t        clean_streak; /* GUIDE 模式下连续全对且均匀的轮数（满 1 自动脱盲） */
    uint8_t        round_in_sess;/* 当前会话已跑轮数 */
    char           verdict_msg[32];/* 结算诊断短句 */
} drill_ctx_t;

#endif /* DRILL_ENGINE_H */
```

## 4. 核心算法规格（必须严格按此实现）

### 4.1 音阶与五度圈映射表

- 五度圈索引 `root_idx` (0..11) 对应的半音值 `ROOT_PC[12]`：`{0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5}`（即 C, G, D, A, E, B, F#, Db, Ab, Eb, Bb, F）。
- 古典旋律小调（假设在 `SCALES[]` 中索引为 3）：
  - 上行半音间隔：`{0, 2, 3, 5, 7, 9, 11}`（升 6、7 级）
  - 下行半音间隔：`{0, 2, 3, 5, 7, 8, 10}`（还原 6、7 级，等同自然小调）
  - 生成序列时，根据 `dir` 标志选择对应半音表。

### 4.2 指法序列生成与"最高音 5 指封顶 + 动态穿指/跨指点"算法（极关键！）

严禁使用简单的 `i % 7` 查表覆盖最高音！必须按以下规则生成 `seq[0..seq_len-1]`：

1. **基础 7 声大小调指法查找表**（存储 1 个八度的循环基础指法 `BASE_FINGER[hand][root_idx][0..6]`）：
   - 右手 `MAJOR_FINGER_R[12][7]`（适用于大调、自然/和声/旋律小调；注：F# 与 Db 等黑键小调若有特殊起始指法，可复用大调物理黑键避让规则，核心原则是拇指不落黑键）：
     - C, G, D, A, E, B: `{1, 2, 3, 1, 2, 3, 4}`
     - F#: `{2, 3, 4, 1, 2, 3, 1}`
     - Db: `{2, 3, 1, 2, 3, 4, 1}`
     - Ab: `{3, 4, 1, 2, 3, 1, 2}`
     - Eb: `{3, 1, 2, 3, 4, 1, 2}`
     - Bb: `{4, 1, 2, 3, 1, 2, 3}`
     - F:  `{1, 2, 3, 4, 1, 2, 3}`
   - 左手 `MAJOR_FINGER_L[12][7]`：
     - C, G, D, A, E, F: `{5, 4, 3, 2, 1, 3, 2}`
     - B: `{4, 3, 2, 1, 4, 3, 2}`
     - F#: `{4, 3, 2, 1, 3, 2, 1}`
     - Db, Ab, Eb, Bb: `{3, 2, 1, 4, 3, 2, 1}`
   - 非 7 声音阶（如五声音阶、半音阶、世界音阶）：当 `scale.n != 7` 时，不查上述表，半音阶固定采用钢琴标准半音指法（黑键用 3 指，白键用 1 指，相邻白键如 E-F/B-C 用 1-2 指），其余非 7 声音阶将 `finger` 设为 0（UI 隐藏指法行，仅靠灯与音名引导）。
2. **上行与最高音封顶规则**（以 2 八度上下行为例，共 `2*14 + 1 = 29` 个音，索引 0..28）：
   - 索引 0..13（上行过程）：`finger = BASE_FINGER[hand][root_idx][i % 7]`。
   - 索引 14（最高音折返点，Top Note）：
     - 若 `hand == 0`（右手）且标准循环指法在该音为 1（例如 C/G/D/A/E/B/F 大调）：强制覆盖为 5 指（小指封顶）！例外：若循环指法本身已不是 1（如 F# 大调最高音自然落在 2 指，Eb 大调落在 4 指），则保留原指法。
     - 若 `hand == 1`（左手）且标准循环指法在该音为 5 或 4：保留原指法；若为 1（如 F#, Db, Ab, Eb, Bb 左手最高音落在 1 指），保留 1 指。
   - 索引 15..28（下行过程，镜像对称）：
     - 严格镜像上行指法：`seq[28 - k].finger = seq[k].finger`（k = 0..13）！这样保证了上下行指法完全可逆，手指无需在折返时重新换把位。
3. **动态标记物理穿指/跨指点 `is_turn`**（替代静态硬编码）：遍历生成好的 `seq[1..seq_len-1]`：
   - `seq[0].is_turn = 0;`
   - 情况 A（大拇指穿指 Thumb-under）：若 `seq[i].finger == 1` 且 `seq[i-1].finger >= 3` → `seq[i].is_turn = 1;`（右手上行或左手下行时发生）。
   - 情况 B（3/4 指跨大拇指 Cross-over）：若 `seq[i-1].finger == 1` 且 `seq[i].finger >= 3` → `seq[i].is_turn = 1;`（右手下行或左手上行时发生）。
   - 其余情况 `seq[i].is_turn = 0;`。

### 4.3 相对八度追踪与按键判定（drill_on_note_on）

当状态处于 `DRILL_ST_RUN` 或 `DRILL_ST_HOLD` 时，收到 note（MIDI 0..127）：

```c
/* 1. 机械回弹防抖：若刚按对同一个键且间隔 < 120ms，直接忽略 */
if (ctx->pos > 0 && note == ctx->prev_ok_midi && (now_ms - ctx->last_note_ms) < 120) {
    return;
}

drill_note_t *exp = &ctx->seq[ctx->pos];
bool is_correct = false;

/* 2. 首音定八度偏移 */
if (ctx->pos == 0 && !ctx->octave_locked) {
    if ((note % 12) == exp->pc) {
        ctx->octave_offset = (int8_t)((int)note - (int)exp->midi);
        ctx->octave_locked = true;
        is_correct = true;
    }
} else {
    /* 3. 后续音严格匹配相对八度偏移（若开启 LOCAL_27 本机窄键盘模式，则按 pc 匹配） */
    int target_midi = (int)exp->midi + (int)ctx->octave_offset;
    if (note == target_midi) {
        is_correct = true;
    }
}
```

- 若 `is_correct == true`：
  1. IOI 均匀度计算（仅当 `ctx->pos > 0` 且上一音未触发过 HOLD 停等时执行）：
     - 理论间隔 `T_target = 60000 / (bpm * subdivision)`。
     - 实际间隔 `Δt = now_ms - ctx->last_note_ms`。
     - 容差系数 `α ∈ {0.45, 0.35, 0.25}`（对应宽松/标准/严格，默认 0.35）。
     - 若 `Δt > T_target * (1.0 + α)`：`ctx->hesitations++`；若 `exp->is_turn == 1`，置 `ctx->turn_hesitated = true`。
     - 若 `Δt < T_target * (1.0 - α)`：`ctx->rushes++`。
  2. 更新 `ctx->last_note_ms = now_ms; ctx->prev_ok_midi = note;`。
  3. 触发本机短促绿灯反馈（`mk.ok` 置位 80ms），状态切回 `DRILL_ST_RUN`（若此前在 HOLD）。
  4. `ctx->pos++`；若 `ctx->pos >= ctx->seq_len`，进入 `drill_finish_round()`。
- 若 `is_correct == false`：
  1. `ctx->errors++`；记录 `ctx->last_wrong_midi = note`。
  2. 若 `ctx->mode == MODE_PRACTICE`：
     - 切入 `DRILL_ST_HOLD`，`ctx->waits++`，`ctx->state_enter_ms = now_ms`。
     - 键盘灯同时将期望键（映射至 27 键窗）与按错的键（`note % 12` 映射至对应音高）置入 `mk.bad` 进行红灯闪烁。
  3. 若 `ctx->mode == MODE_STAGE`：
     - 不进入 HOLD，不阻塞流程，红灯闪一下 80ms 提示错误后保留当前 `pos`（或按配置记 slip 继续），保证考级盲弹流不被打断。

### 4.4 单轮结算、自动脱盲与速度阶梯（drill_finish_round）

1. 计算均匀度与准确率：
   - `bool is_even = (ctx->hesitations + ctx->rushes) <= 1;`
   - `bool is_clean = (ctx->errors == 0 && ctx->waits == 0 && is_even);`
2. PRACTICE 模式下的结算逻辑：
   - **自动脱盲机制（防"打地鼠依赖"）**：
     - 若当前 `ctx->assist == ASSIST_GUIDE` 且 `is_clean == true`：`ctx->clean_streak++`。当 `clean_streak >= 1` 时，自动将 `ctx->assist` 升级为 `ASSIST_FRAME`（隐藏音名），再跑对 1 轮自动升为 `ASSIST_ANCHOR`（仅亮根音），并在 `verdict_msg` 提示 `"AUTO FADE -> ANCHOR"`。
     - 若单轮 `errors >= 3`：自动降一级辅助救场（最低回退至 `ASSIST_GUIDE`）。
   - **Auto-BPM 与拍子细分阶梯**：
     - 若 `is_clean == true`：
       - `ctx->bpm += 8;`
       - 若 `ctx->bpm > 100 && ctx->subdivision < 4`：`ctx->bpm = 56; ctx->subdivision *= 2;`（平滑切换拍子细分，避免高频节拍器刺耳）。
       - `snprintf(verdict_msg, 32, "CLEAN! -> %d (%dx)", ctx->bpm, ctx->subdivision);`
     - 否则若 `ctx->errors >= 3 || (ctx->hesitations + ctx->rushes) >= 4`：
       - `ctx->bpm = MAX(40, ctx->bpm - 8);`
       - 诊断原因优先级：若 `ctx->turn_hesitated` 为真，显示 `"TURN SLOW! -8 BPM"`；否则若 `errors >= 3` 显示 `"ERRORS! -8 BPM"`；否则显示 `"UNEVEN! -8 BPM"`。
     - 否则（轻微瑕疵，如错 1 音或卡顿 2 次）：
       - 保持 bpm 不变，显示 `ctx->turn_hesitated ? "WATCH THUMB TURN!" : "KEEP STEADY"`。
3. STAGE 模式下的结算逻辑：
   - 必须满足 `errors == 0 && is_even` 才算 Pass。
   - 若 Pass：
     - 对比当前等效速度 `eff_bpm = ctx->bpm * ctx->subdivision` 与历史记录，若超越历史则更新 `best_bpm` 与 `best_flags`，并标记 `STAGE_PASSED` bit，写入 Flash。
     - 若达到 ABRSM 考纲目标（1 八度 2x 下 `bpm >= 66`；2 八度 2x 下 `bpm >= 80`），置位达标徽章，进入 `DRILL_ST_SUMMARY` 询问是否按 K1 切换五度圈下一个调。

## 5. 硬件灯光映射规则（27 键 F3–G5）

FM-1 物理键盘仅覆盖 MIDI 53..79（27 键）。所有逻辑 MIDI 音高通过辅助函数 `midi_to_led_idx(uint8_t logic_midi, int8_t oct_shift)` 映射至 0..26（超出范围时按 ±12 八度折叠入窗，保证任何调在任何 `oct_shift` 下都有灯亮）：

| 辅助等级 | 根音（pc == root_pc） | 音阶内其他音 | 当前期望音 seq[pos] | 错音（HOLD 状态） |
|---|---|---|---|---|
| GUIDE | `mk.root`（全亮常亮） | `mk.scale`（中亮常亮） | `mk.hint`（全亮高频呼吸/闪烁） | `mk.bad`（错音键 + 期望键双闪） |
| FRAME | `mk.root`（全亮常亮） | `mk.scale`（中亮常亮） | `mk.hint`（全亮闪烁） | `mk.bad`（双闪） |
| ANCHOR | `mk.root`（全亮常亮，作为物理路标） | 全灭 | 不提前亮灯 | `mk.bad`（仅错音时闪红） |
| BLIND | 全灭 | 全灭 | 全灭 | `mk.bad`（仅错音时闪红） |

## 6. 自动化测试规格（app/tests/drill.txt）

Agent 实现代码后，必须在 Host 模拟器中编写并跑通 `app/tests/drill.txt` 测试脚本，覆盖以下 6 个核心断言场景：

1. **相对八度追踪断言**：设置 C 大调 2 八度上下行（`seq_len = 29`），模拟电钢琴从 C3 (MIDI 48) 起手，依次输入 48, 50, 52, 53... 直到 72 再下行回 48，断言全程判定正确；若中途输入 60 (C4) 代替 48 (C3)，断言触发 HOLD 报错（证明防跨八度偷懒生效）。
2. **最高音 5 指封顶与动态穿指点断言**：检查生成的 C 大调右手 2 八度序列：`seq[3]`（F）为 `finger=1, is_turn=1`；`seq[7]`（C4）为 `finger=1, is_turn=1`；`seq[14]`（最高音 C5）必须为 `finger=5, is_turn=0`；下行时 `seq[18]`（F4）为 `finger=1, is_turn=0`，而 `seq[19]`（E4，3指跨过1指）必须为 `finger=3, is_turn=1`。
3. **穿指卡顿拦截升速断言**：在 BPM=60 (1x) 下跑完 29 音，全音高正确，但故意在 `seq[3]`（穿指点）延迟 1500ms（超过 1000ms * 1.35）后按键。断言轮次结束时 `turn_hesitated == true`，bpm 保持 60 不升速，且屏幕诊断文案包含 `"TURN"`。
4. **自动脱盲断言**：在 `ASSIST_GUIDE` 下无错且均匀跑完第 1 轮，断言第 2 轮自动切换为 `ASSIST_FRAME`（屏显隐藏音名，保留指法数字）。
5. **加维度降速 30% 断言**：当 `bpm == 100` 时，模拟用户将 shape 从 1 八度切换为 2 八度，断言 bpm 立即回退至 70。
6. **回归与屏幕像素不溢出断言**：执行 `sh tests/run_tests.sh`，确保上游原有 5 组测试与 `texts_fit.txt` 240px 宽度检查 100% 通过。

## 7. 执行步骤指令（给 Coding Agent 的 Action Plan）

1. **Step 1**：新建 `app/drill_engine.h` 与 `app/drill_engine.c`，实现 §3 数据结构与 §4 全部纯 C 逻辑（无 UI 依赖），编写纯 C 单元测试验证指法表、最高音 5 指封顶、古典旋律小调上下行及 IOI 判定。
2. **Step 2**：在 `app/settings.h`（或对应持久化头）的 `settings_t` 尾部追加 `drill_save_t`，实现校验魔数 `0x4452` 的加载与防脏写保存。
3. **Step 3**：在 `app/texts.c` 中新增 Drill 专属短文案（控制在 18 字符以内以防 240px 溢出），运行 `gen_tests.py` 更新 `texts_fit.txt`。
4. **Step 4**：在 `app/app.c` 的 LESSONS 菜单第 10 项注册 DRILL，实现 `SCR_DRILL` 的两半分区刷新绘制函数（严格区分 GUIDE/FRAME/ANCHOR/BLIND 的信息隐藏逻辑），接管 `note_in()`、`ui_lights()` 与 `ui_input()` 定时器。
5. **Step 5**：编写 `app/tests/drill.txt`，运行 `sh tests/run_tests.sh` 确认全绿，并导出 IDLE、RUN(GUIDE)、HOLD、VERDICT 四个状态的模拟器 PNG 截图用于最终验收。
