# 实现说明：与 SPEC v1.0 的偏差登记

> 对应代码：`repo/app/drill_engine.{h,c}`、`repo/app/app.c`（drill 段）、`repo/app/texts.c`、`repo/app/tests/drill_test.c`、`repo/app/tests/drill.txt`
> 依据：`SPEC-v1.0.md`（需求）+ `REVIEW-v1.0.md`（核实结论）。本文件只记"为什么和 spec 写的不一样"。
> 状态（2026-11-14）：`sh tests/run_tests.sh` 全绿 —— theory 917 checks、drill 397 checks、上游 5 组 host 脚本（含 texts_fit 5 语言）。

## 1. 结构

| # | SPEC | 实现 | 原因 |
|---|---|---|---|
| 1 | `drill_save_t` 塞进 `settings_t`（1160 B） | 独立 store 对象 `OBJ_DRILL` @ `0xDA000`，`drill_save_t` = 812 B（`best` 792 + `stage` 位图 99 + 头 12） | REVIEW B1：`app_init()` 比对 `sizeof cfg` 精确等长，加字段等于每次升级清空 quiz 记录；`PLAT_STORE_MAX`=3840 但没必要动用户数据 |
| 2 | `best_flags` 每格 1 B（只 3 bit 有效） | `best` 存 `eff/2`（u8，0 = 无记录）+ `stage` 每格 1 bit | REVIEW S5：792 B 换 99 B 的信息不值得 |
| 3 | `uint8_t oct_shift` | 删除，用宿主的 `ui.octave` | REVIEW B3：`mk.*` 是 12 bit 音高类掩码，宿主已按 `(53+k+12*octave)%12` 投影到 27 键 |
| 4 | `midi_to_led_idx()` | 未实现 | 同上，这层映射在宿主里已经存在，再做一层只会打架 |
| 5 | `snprintf(verdict_msg,...)` | 引擎存 `vkind`（0..7 的码），屏幕用 `texts.c` 的 5 语言表 | REVIEW S1：unity build 不引 `stdio`；文案必须进 `texts_fit` 的 240 px 检查 |

## 2. 灯光

SPEC §5 的四级表按字面写不出来（REVIEW B2）：宿主 `ui_lights()` 的优先级固定为 held → `root|ok`（bright）→ `bad|scale`（mid）→ `hint`（dim），且**没有闪烁逻辑**。实际映射：

| 语义 | 掩码 | 观感 |
|---|---|---|
| 根音 | `mk.root` | 亮 |
| 当前期望音 | `mk.ok` | 亮（GUIDE / FRAME / HOLD 都亮，HOLD 时也不熄 —— 它是指路标） |
| 音阶内其余音 | `mk.scale` | 中亮 |
| 音阶外 / BLIND | 不点 | 灭 |
| 弹错 | `mk.bad`（drill 每 130 ms 翻转） | 中亮红闪；期望音保持亮，不跟着变红 |
| ANCHOR | 只有 `mk.root` | 亮根音 |

`mk.hint`（dim，最暗）没有使用。错音闪红是 drill 自己在 `drill_lights()` 里按 `plat_ms()/130` 翻转掩码实现的。

## 3. 判定与阶梯

| # | 处理 | 原因 |
|---|---|---|
| 6 | 示范不走 `play_scale()`，drill 自己按 `drill_note_ms()` 调 `play_note()` | REVIEW B4：`play_scale()` 用 `step_ms()`（520/360/240 ms，由 `cfg.speed` 定），与 drill 的 BPM 无关，也演示不了 2 八度与旋律小调下行 |
| 7 | `is_clean` 额外要求 `turn_slow == 0` | SPEC §4.4 的 `is_even`（29 音允许 1 次不均）与 §6 场景 3（穿指停顿时"bpm 保持 60"）直接冲突。穿指卡顿正是本功能存在的理由，不能算干净 |
| 8 | 速度阶梯跑在 `eff = bpm × sub`（每分钟音数）上，`sub = eff<=120?1:eff<=240?2:4` | REVIEW S2：spec 的 `bpm>100 → bpm=56, sub×2` 会让音速从 104 跳到 112 |
| 9 | STAGE = 锁 BLIND + 锁速度 + 错音记 `miss` 但**仍逐音等待** | REVIEW B6：spec 的"不停、保留 pos"在逻辑上就是停住；真正的节奏考级要时钟驱动引擎，V1 不做，文档里已明说 |
| 10 | best 只在 STAGE 干净通过时写，`best` 存 `eff/2` | 防止 PRACTICE 里带辅助的高分污染记录 |
| 11 | 换型态只在"变长"时降 30%（2 八度 → 1 八度不涨也不降） | 降了不还回去，避免同一维度反复横跳刷分 |
| 12 | `drill_dim()` = `eff × 0.7`，下限 40 | REVIEW B5；SPEC 只有测试里写了这条规则，正文缺失 |

## 4. 指法

- 数据在 `drill_engine.c` 的 `FINGER_R / FINGER_L`（12 调 × 7 指，按五度圈索引），加 `TOP_R / TOP_L` 覆盖表（全 0，留作教师修订位）。
- 只对 7 声音阶给指法；`n != 7` 一律 `finger = 0`（屏幕不显示指法）。SPEC 说半音阶用"黑键 3、白键 1"，但那条规则不完整（黑键之间还要 2/4），给错指法比不给更糟（REVIEW I1）。
- 最高音规则照 spec：循环指法落在 1 指则改 5 指；**F#/Db 两调的顶音仍落在 2 指**（这是 spec 规则的真结果），已在 REVIEW I1 标为待教师核对，改数据即可，不用改代码。
- 下行严格镜像上行（`seq[2*top-i].finger = seq[i].finger`），单元测试逐音断言。

## 5. 测试

| 文件 | 内容 |
|---|---|
| `app/tests/drill_test.c` | 397 checks：22 音阶 × 3 型态的长度与首尾音、C 大调右手 2 八度逐音指法与穿指点、上下行镜像、12 调双手指法合法域、旋律小调上下行不同、相对八度锁（首音定八度、换八度算错）、同键 120 ms 防抖、干净轮 +8、穿指停顿不进速、`eff` 换挡连续、换型态 −30%、STAGE 记 miss 不阻塞 |
| `app/tests/drill.txt` | host 脚本：菜单进法（LESSONS 第 10 项）、29 音一整轮、穿指停 1.5 s → `vkind 6` / `deff 60` / `dbpm` 不动、干净轮 → `deff 68` / `dassist 1`、八度锁报错、STAGE 记 miss、型态降速 |
| `app/tests/texts_fit.txt` | 由 `gen_tests.py` 重生成；drill 屏在 5 语言 × 4 辅助级 × 3 型态下全画过一遍，无文字溢出 |
| `tests/run_tests.sh` | 追加 `drill_test` 与 `drill.txt`，其余 5 组保持原样 |

新增 host 命令：`drill ROOT SCALE SHAPE [HAND]`、`dgo`、`dnote N`、`doff N`；新增断言项：`dstate dpos dlen dbpm deff dsub derr dhes dturn dvkind dassist dmiss dfing`。

## 6. 本机出包（2026-11-14 打通）

nuc7 是 x86-64，JieLi 的工具链也是 x86-64，所以**不需要 CI 也能本地出 `.fwsc`**（之前判断"必须 GitHub Actions"是错的，撤回）：

```sh
sh base/tools/get_toolchain.sh                    # ~/.jieli/toolchain (136 MB)
sh base/bsp/tools/get_sdk.sh                      # ~/.jieli/ac79-sdk (Apache-2.0, d179b44)
# get_sdk.sh 的 sparse 集合少了 apps/common/config/，补一次否则 BLE 构建找不到 .c：
cd ~/.jieli/ac79-sdk && git sparse-checkout add /apps/common/config/
cd repo && JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/.jieli/ac79-sdk python3 base/tools/build.py app
```

产物：`app/build/theory1.fwsc`（611 496 B，identity `FM-1_5800`）、`app/build/theory1.bin`（486 912 B），构建自检全过（`.ram_text` 调用、无 64 位乘法、无软浮点、RAM 84 468/98 304 B）。

刷机：`pip` 的 python-rtmidi 在 Debian 13 上要 ALSA 头文件，直接用发行版包更省事 —— `apt install python3-mido python3-rtmidi`，然后 `python3 base/tools/fm1_install.py app/build/theory1.fwsc`（USB 数据线直连，不过 hub，写入时别拔）。

## 7. 未完成 / 未验证

1. **上真机未验证**：`.fwsc` 已在本机出包（见 §6），但还没刷进设备；灯的实际亮度/闪烁频率、节拍器音量、OCT± 移窗都还没在真机上验。
2. 屏幕版式只做过 240 px 宽度检查与 4 张截图目检（`/tmp/opencode/shots`）；没有做 5 语言逐页目检。
3. 西/法/德新增 20 条文案由我写，未经母语者过目。
4. 指法表 F#/Db 顶音、左手 F# 的 1 指落 C# 这两处待教师核对。

## 8. 你的 PR #2（UC25HQ80 flash）已并入

`base/` 的 pin 从 `5649be9` 提到 `b7043f9`（fm1-base PR #35 的 merge commit），`base/bsp` 随之 `cd76d43 → 773b0b9`。这就是 `wave00066/theory1#2`（作者 hardass）的全部内容：`device/fm1_flash_ok.h` 的 `FM1B_FLASH_JEDEC_OK(id)` 现在接受 `0x856014`（Puya P25Q80）与 `0xB36014`（UC25HQ80）两种 flash。

没有这一条，`flash_ok` 在 UC25HQ80 机器上恒假，整个 store 只在 RAM 里：SETTINGS 的 SAVE 报 ERROR、语言和 quiz 成绩掉电即失、蓝牙配对丢失，**并且本项目的 `OBJ_DRILL`（成绩、设置）同样不会落盘**。

## 9. 速度旋钮

K4 直接改 BPM，每次 1 拍/分钟（40..200），不再走 `eff ±8` 那套。阶梯仍会在每轮结束时自动 ±8；STAGE 模式梯子不动，只有 K4 能动。

## 10. 按实测反馈改的（v1.1.0）

| 反馈 | 改动 |
|---|---|
| REC 在所有屏都是蓝牙配对，别动 | drill 屏不再抢 REC / SAVE。REC = 蓝牙设备，SAVE = 保存设置（和其他屏一致）。STAGE 轮（原来挂在 SAVE 上）删掉 |
| 27 键弹不了跨八度 | 新增键盘模式：**SEL 切换 `PIANO` / `27 TASTI`**。`27 TASTI` 下强制 1 八度型态、自动把主音放到 F3–G5 里（`drill_oct_base()`）、`ui.octave` 锁 0、判定改为**按绝对音高**（不认"换个八度的同一个音"），OCT± 在该模式下无效。`PIANO` 下仍是相对八度（首音定八度） |
| 换根音/调性要立刻听到 | 任何设置变化（K1 根音、K2 音阶、K3 形态、ALGO 手）→ 立刻按 200 ms/音 播一遍整条音阶（上行+下行），PLAY 打断它并进入练习 |
| 四个 knob 没说清 | 屏幕底部两行小字常显：`CHIAVE Do   SCALA maggiore` / `FORMA 1 ottava   VEL 60/min`；最底提示行是 `EDIT: aiuto   SEL: tastiera` |
| `60x1` 不懂 | 去掉"每拍几个音"的概念：现在 **1 音 = 1 拍 = 1 次咔哒**，显示 `60/min` 就是每分钟 60 个音。阶梯、K4、显示三者同一个数 |
| 屏幕上别放 best | `best` 行删掉。每轮干净通过时在 RAM 里更新 `ds.best`（存 **BPM**，就是每分钟音数），**退出 drill 屏时**才写 flash（`drill_dirty`） |
| Edit 是改什么 | EDIT 切"灯帮多少忙"：GUIDE（全亮+指法）→ FRAME（不显示音名）→ ANCHOR（只亮根音）→ BLIND（全灭） |

## 11. 与上游语法对齐（v1.2.0）

查了 THEORY-1 自己的约定（`texts.c` 的 `S_H_*` 提示行）：

| 屏 | 提示行 |
|---|---|
| 主菜单 | `SELECT CHOOSE   SEL ENTER` |
| 主题列表 | `SELECT CHOOSE   SEL ENTER   HOME MENU` |
| 测验设置 | `SELECT TYPE   K1 LEVEL   SEL START` |
| SYNTH | `SELECT PAGE  SEL NEXT  PLAY EXAMPLE  FX RESTART` |

即：**SELECT 选"练什么"，K1 调"根音/级别"，SEL 是"开始/进入"**。DRILL 现在照这条：

| 控件 | 原来 | 现在 |
|---|---|---|
| SELECT | 键盘模式 | **音阶（skill）**，与 LEZIONI/QUIZ 一致 |
| K1 / PRESETS | 根音 | 根音（不变） |
| K2 | 音阶 | **形态** |
| K3 | 形态 | **BPM** |
| K4 | BPM | **辅助等级**（屏上直接显示 GUIDE/FRAME/ANCHOR/BLIND） |
| ALGO | 手 | 手（不变） |
| SEL（按键） | — | **键盘模式 PIANO / 27 TASTI** |

底部两行常显四个 knob 的当前值（`CHIAVE Do  FORMA 1 ottava` / `BPM 60  AIUTO GUIDE`），最底行是 `SELECT SCALA   SEL TASTIERA`，与上面那些屏同一句式。

**速度的口径**：固件里原本没有 BPM 概念，只有 SETTINGS ▸ `Velocità esempi`（slow/medium/fast = 520/360/240 ms/音，换算 115/167/250 音每分钟）。所以 DRILL 的 `BPM` 是新增的显式数值，但单位与之一致（都是"每分钟多少个音"），且 40–200 覆盖那三档。1 音 = 1 拍 = 1 次咔哒。
