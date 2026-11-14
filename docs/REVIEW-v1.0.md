# SPEC v1.0 审阅意见（对照 `theory1@92b9ab9` 源码核实）

> 审阅对象：`SPEC-v1.0.md`。所有引用均已在本机 `~/fm1-scale-drill/repo` 核实，附 `file:line`。
> 结论：**架构方向可用，但有 6 处会直接写坏编码的硬伤**（B1–B6），其余为口径问题。实现时按本文件的"修正"执行，`SPEC-v1.0.md` 原文不改，便于追溯。

---

## 阻断级（B1–B6，不修就写错）

### B1 `drill_save_t` 实际不是 1160 B；且塞进 `settings_t` 会清空用户 quiz 记录

- `settings_t` 现有 36 B（`app/app.c:22-35`）。
- spec 声明了两个 `[22][12][3]` 的 u8 数组：**792 B × 2 + 8 = 1592 B**，不是 1160（1160 = 792+360+8，与其自身声明矛盾，注释里"396B 压缩或 792B…用 360B"是三种都没定下来的草稿）。
- `PLAT_STORE_MAX = 3840`（`base/include/plat.h:244`），1592+36 放得下，**容量不是问题**。
- 真正的问题是 `app_init()` 要求**精确等长**：

  ```c
  if (plat_store_load(OBJ_SETTINGS, &cfg, sizeof cfg) != (int)sizeof cfg || ...) settings_defaults();   // app.c:2796
  ```

  给 `settings_t` 加字段 = 老版本存的对象长度不匹配 = **每次升级都把用户的 quiz 成绩清零一次**。

**修正**：新建独立 store 对象 `OBJ_DRILL`，偏移 `0xDA000`（`0xDA000–0xDC000` 正好两个 4 KiB 扇区，且现为空闲：panel 在 `0xDC000`、BLE 在 `0xDE000`，见 `app/app.h:20`）。`drill_save_t` 自带 magic + version，`settings_t` 一个字节都不动。存储布局：

```c
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_DRILL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0xDA000u}

typedef struct __attribute__((packed)) {
    uint32_t magic;        /* 'DRL1' */
    uint8_t  ver;          /* 1 */
    uint8_t  root_idx, scale_idx, shape, hand, assist, ioi_tol;
    uint8_t  best_bpm[DRILL_NUM_SCALES][DRILL_NUM_ROOTS][DRILL_NUM_SHAPES];   /* 792 B */
    uint8_t  stage_bits[(DRILL_NUM_SCALES * DRILL_NUM_ROOTS * DRILL_NUM_SHAPES + 7) / 8]; /* 99 B：STAGE 盲弹达标 */
} drill_save_t;            /* 812 B */
```

### B2 灯光四级表按字面写不出来：`mk.hint` 是最暗档，且宿主**没有闪烁**

`ui_lights()`（`app/app.c:733-753`）的优先级是**写死**的：

```
held / ex_sounding      -> bright
(root | ok)             -> bright
(bad | scale)           -> mid
hint                    -> dim          ← 最暗，不是"全亮"
```

而且 `ui_lights()` 是掩码的纯函数，**没有任何时间相位**。spec §5 说"`mk.hint` 全亮高频呼吸/闪烁"两处都错。

**修正**（不碰宿主，drill 自己按 `plat_ms()` 相位翻转掩码）：

| 语义 | 掩码 | 实际观感 |
|---|---|---|
| 根音 | `mk.root` | bright |
| 当前期望音 | `mk.ok` | bright（与根音同档，靠"只有一个音亮"区分） |
| 音阶内其余音 | `mk.scale` | mid |
| 错音（双闪） | `mk.bad`，drill 以 ~7 Hz 置/清 | mid 闪 |
| 八度锚点（ANCHOR 级） | `mk.root` | bright |

### B3 `mk.*` 是**音高类**掩码，不是 27 键位图；`midi_to_led_idx()` 是多余抽象

`#define PCB(x) (1u << (x))`（`app/texts.c:370`），`mk` 五个字段都是 12 bit 音高类（`app/app.c:666-668`）。宿主自己完成键位投影：`pc = (53 + k + 12*ui.octave) % 12`（`app/app.c:691, 711, 738`）。

**修正**：删除 `midi_to_led_idx()`；八度窗直接用已有的 `ui.octave`（OCT±）。`drill_ctx_t` 里也不要 `oct_shift`——spec 把它声明成 `uint8_t` 却注释 `-1/0/+1`，类型本身就矛盾。

### B4 `play_scale()` 用的是全局示例速度，不是 drill 的 BPM

`step_ms()` 返回 `cfg.speed ? 520/360/240`（`app/app.c:116`），`play_scale()` 直接用它（`app/app.c:172-181`）。所以拿它做示范：速度与当前 BPM 脱节，且演示不了 drill 自己的型态（2 八度、旋律小调上/下行两张表）。

**修正**：drill 用 `play_note(t, midi, dur, vel)`（`app/app.c:110`）按 `60000/(bpm*sub)` 逐音调度自己的 `seq[]`；`play_stop()` 在进 COUNTIN 前调用（spec 这条正确）。

### B5 §4.4 与测试 5 冲突：降 30% 规则在正文里不存在

测试 5 断言"切型态 bpm 100 → 70"，但 §4.4 的结算逻辑没有任何"换维度降价"条目。

**修正**：新增 `drill_dim_changed()`：`shape / assist / hand` 任一变化 → `eff = max(40, eff * 7 / 10)`，再按 8 的网格取整。

### B6 STAGE 分支自相矛盾："错音不停、保留当前 pos" = 停住

保留 `pos` 就等价于 PRACTICE 的 HOLD，只是不显示。真 STAGE 必须**时钟驱动**推进（按节拍丢判定），那是另一套引擎。

**修正（取小改动、行为可解释的版本）**：STAGE = 锁 BLIND + 锁 BPM + 错音记 `miss` 且**继续等待**（视觉上不停拍、但判定仍逐音），结算要求 `miss == 0` 才写 best。文档里明确写"本版 STAGE 不是节奏考级，只判音高与均匀度"，避免误解。

---

## 重要但非阻断（I1–I6）

### I1 指法表需权威核对，且"最高音封顶"规则对黑键调产出可疑指法

- 单八度循环表与主流教材一致（RH C/G/D/A/E/B `1 2 3 1 2 3 4`、F `1 2 3 4 1 2 3`、Bb `4 1 2 3 1 2 3`；LH C/G/D/A/E/F `5 4 3 2 1 3 2`、B `4 3 2 1 4 3 2`）。
- 但规则"若循环指法在该音为 1 则强制 5，否则保留"在 2 八度上给出：C→5 ✓、Bb→4 ✓、**Db→2、F#→2**。Db/F# 大调 2 八度右手收在 2 指，绝大多数教材不会这么写。
- 左手 `F# {4,3,2,1,3,2,1}` 让 1 指落在 **C#（黑键）**，与"拇指不落黑键"原则冲突。

**修正**：规则保留为默认，另加**覆盖表** `TOP_FINGER_R[12] / TOP_FINGER_L[12]`（0 = 用默认规则），教师可只改数据不改代码；并加单元测试断言"RH 最高音 ≠ 1/2（除覆盖表明确允许）"、"上下行镜像一致"。`finger` 只用于显示，**不参与判定**，所以指法错不会误判，只是教学口径问题。

### I2 `seq[].midi` 的"C3=48 基准"表述含糊

`base_midi()` 把示例放在 G3..F#4（`app/app.c:137`），与 C3=48 不是一回事。直接定义：`seq[0].midi = 60 + ROOT_PC[root_idx]`（中央 C 起，1 八度型态落在 C4..C5），**绝对 MIDI**，测试据此可写死数字。

### I3 拦截点签名与 HOLD 行为

`note_in(int note, int vel, int on, int outside)`（`app/app.c:2307`，都是 `int`，不是 `uint8_t`）。HOLD 状态必须继续吃输入（否则按对也出不来）。MIDI 走 `midi_in()` → `note_in(..., 1, 1)`（`app/app.c:2348`）。

### I4 节拍器与示范共用 `NPEV = 96` 事件池

`play_ev()` 池满即**静默丢弃**（`app/app.c:96-108`）。drill 一次最多 49 个示范音 + 每拍 2 个 click，够用，但别在 `drill_tick()` 里每帧无条件 `play_note`。CI 里可用 `expect voices N` 验证没漏 note-off。

### I5 "文案 18 字符以内"是猜的

`texts_fit.txt` 是 `gen_tests.py` **生成**的 240 px 溢出检查（27935 行）。正确流程：先写 → 跑 `gen_tests.py` → 跑模拟器 → 超了会打印具体字符串 → 再改。不要靠字符数猜（德语永远最长）。

### I6 加第 10 个主题要同步 4 处

`NTOPICS`、`TOPIC_ITEMS[]`、`TOPIC_NAME/TOPIC_SUB[5][NTOPICS]`（`app/texts.c`）、`draw_lesson()` 里的 topic 分支（`app/app.c:941+`）。列表本身滚动，版式不动 ✓。

---

## 口径修正（小）

| # | 原文 | 问题 | 修正 |
|---|---|---|---|
| S1 | `snprintf(verdict_msg, ...)` | 嵌入式 unity build 里没必要引入 `snprintf` | 用现成的 `t1_strcpy()` + `cat_int()`（`app/app.c:260-295`） |
| S2 | `bpm > 100 → bpm = 56, sub *= 2` | 100→104 时换挡，音速从 104 跳到 112，阶梯不连续 | 在 `eff = bpm*sub`（音/分钟）上做 +8 阶梯，显示时反解 `bpm = eff/sub`，`sub = eff<=120?1:eff<=240?2:4` |
| S3 | `is_even = hesitations+rushes <= 1` | 29 音允许 1 次卡顿仍算 clean 并 +8 BPM，太松 | `allow = (seq_len * 5 + 99) / 100`（5%，最少 1） |
| S4 | ABRSM 目标 `bpm >= 66` | 没说"1 拍 = 几个音"，差一倍 | 加常量 `ABRSM_NOTES_PER_BEAT 2`（考纲 ♩=X 指每拍 2 个八分音符），比较用 `eff` |
| S5 | `best_flags` 每格 1 B 只用 3 bit | 792 B 换 99 B 的信息 | 改位图 `stage_bits[]`，99 B |
| S6 | `drill_ctx_t.oct_shift` | 与 `ui.octave` 重复 | 删，用 `ui.octave` |

---

## 与实现的偏差登记

实现按本文件执行；每处偏差在 `docs/IMPL-NOTES.md` 留一行（含原因），便于复核。
