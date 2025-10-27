### 基于 RT-Thread 的秒表/计时器项目（STM32F103C8T6）

本项目在 RT-Thread Studio 下开发，利用 `STM32F103C8T6`、`0.96寸 I2C OLED`、`CH340 串口`、`3 路 LED`、`有源蜂鸣器（低电平触发）`、`光敏电阻模块（4 引脚）` 等硬件，实现“秒表/计时器 + 圈速（Lap）+ 可视化 + 串口交互”。文档将随每次功能修改“实时更新”。

---

## 1. 需求分析

- **核心计时功能**
  - 启动、暂停、继续、复位
  - 圈速记录（Lap）：记录本圈用时与累计用时；圈速容量可配置（缺省 20 圈）
  - 分辨率：目标 1 ms，显示到 1 ms 或 10 ms（OLED 可配置显示精度）
  - 误差目标：< 5 ms/分钟（常温、稳定供电）

- **显示与提示**
  - OLED：显示当前计时、圈速列表（滚动）、运行状态
  - LED：运行/暂停/告警状态指示（3 路）
  - 蜂鸣器：开始/停止/圈速提示音，支持静音开关

- **交互与数据**
  - 串口 msh 命令控制：`sw_start`、`sw_stop`、`sw_reset`、`sw_lap`、`sw_status`、`sw_csv on|off`、`sw_beep on|off`
  - CSV 导出：圈速与时间序列导出到串口，便于 PC 端记录/分析
  - 可选：光敏传感器联动（黑暗环境自动静音、OLED 反色或降低刷新率）

- **可靠性与可维护性**
  - RT-Thread 多线程结构，非阻塞刷新
  - 线程安全：命令与计时核心通过消息或互斥保护
  - 代码模块化、便于后续扩展（存储掉电保持、脚本回放等）

---

## 2. 可行性分析

- **资源评估**
  - F103C8T6：64KB Flash / 20KB SRAM，满足 RT-Thread + OLED 缓冲 + 业务线程
  - OLED 软件 I2C（PB8/9）已在 `qu_dong/OLED` 给出参考实现，可直接复用或改为 RT-Thread pin/i2c 设备
  - 蜂鸣器与光敏传感器 GPIO 简单，LED 指示占用 3 个 GPIO

- **计时精度**
  - 基线采用 `rt_tick_get()`（1ms Tick），核心计时通过“累计增量法”与“暂停段扣除”保证一致性
  - 若需更高精度：可启用 `hwtimer` 或 `TIM2` 定时器（后续可选里程碑）

- **与现有驱动的兼容性**
  - `qu_dong/OLED` 使用 HAL 位带 I2C（PB8/PB9），`Buzzer` 与 `LightSensor` 使用 StdPeriph 库风格
  - 在 RT-Thread 中建议统一用 `rt_pin_mode/rt_pin_write/rt_pin_read` 或 HAL 驱动；本项目将“参考 `qu_dong` 时序与引脚”，在 RT-Thread 下做适配层，降低重写成本

- **开发与调试**
  - 串口（CH340）作为 msh 控制台，命令可快速迭代测试
  - ST-Link SWD 调试可靠

---

## 3. 整体功能与架构设计

- **模块划分**
  - `stopwatch_service`：核心计时服务（状态机：IDLE/RUNNING/PAUSED），提供 API：start/stop/reset/lap/status/get_records
  - `cli_msh`：msh 命令解析与调用服务 API（已实现）
- `ui_oled`：OLED 界面线程，渲染当前时间与圈速（已实现，默认 10ms 刷新，可命令调整）
  - `indicator_led`：LED 状态指示（运行/暂停/错误，已实现）
  - `notifier_buzzer`：提示音（开始/停止/圈速短促提示、可开关，已实现）
  - `sensor_light`（可选）：光敏传感器联动（自动静音/省电/反色，计划中）

- **线程与优先级（建议）**
  - 计时服务线程：中高优先级，处理命令与高精度计时（若用 hwtimer 则在回调/下半部）
- OLED 线程：中优先级，50~100 Hz（10~20 ms）或按 `sw_oled_rate` 配置，避免阻塞
  - CLI：依托 msh 任务
  - LED/蜂鸣器：低优先级或定时器回调

- **数据结构**
  - `StopwatchState`：当前状态、开始时间戳、累计暂停时间、最近一圈起点
  - `LapRecord[]`：圈速数组（上限 N=20，可配置；超过上限会丢弃最早一圈）

---

## 3.1 功能清单（当前实现）

- 秒表：开始/暂停/继续/复位；圈速记录；状态查询；CSV 连续输出
- OLED UI：显示“Stopwatch”、当前时间（mm:ss.cc）与最近一圈（mm:ss.cc）；
  - 局部刷新：固定矩形（主时间 128×16、最近一圈从 X=30 宽98），切页/初始化才全屏重绘
  - 体积优化：裁剪未用 OLED 数字/几何/反色等接口，保留字符串与区域刷新
- LED 指示：运行（PC13 闪烁）/暂停（PB0 常亮）/空闲（全灭）
- 蜂鸣器：`sw_start/stop/reset/lap` 时短促提示；`sw_beep on|off` 开关
- 命令集：`sw_start`、`sw_stop`、`sw_reset`、`sw_lap`、`sw_status`、`sw_csv on|off [period_ms]`、`sw_beep on|off`
  - CSV 缓冲/导出：按需格式化输出

---

## 4. 外设分步集成与驱动参考（循序渐进）

里程碑按从易到难推进，每个里程碑都可独立测试与回退。

1) 核心计时 + 串口命令（无外设依赖）
   - 完成 `stopwatch_service` 与 `sw_*` 命令；串口打印状态/圈速/CSV
   - Kconfig：开启 `RT_USING_FINSH`、`FINSH_USING_MSH`、`FINSH_USING_MSH_ONLY`

2) LED 指示集成（3 路）
   - 参考引脚：`PC13`（运行闪烁）、`PB0`（暂停常亮）、`PB1`（错误/溢出）
   - 若板载 LED 已占用，请根据实际连线在文末“接线”中调整

3) 蜂鸣器提示音
   - 参考 `qu_dong/Buzzer`：`PB12` 低电平响
   - 事件：start/stop/lap 时短促提示；支持 `sw_beep on|off`

4) OLED 显示
  - 参考 `qu_dong/OLED`：PB8=SCL、PB9=SDA（开漏+上拉），SSD1306 128x64
  - UI：主界面显示 mm:ss.mmm；子页显示圈速表（分页/滚动）；采用局部刷新提升帧率

5) 光敏传感器（可选）
   - 参考 `qu_dong/LightSensor`：`PB13` 数字输入（模块 DO）
   - 联动策略：黑暗→静音、OLED 降帧（300ms）；亮度恢复→OLED 10ms（可被手动 `sw_oled_rate` 覆盖）

6) 精度增强（可选）
   - 启用 `hwtimer` 或 `TIM2` 捕获/比较，减小抖动；保持 API 不变

> 驱动参考：
> - `qu_dong/OLED/*`（I2C 位操作，HAL 风格）
> - `qu_dong/Buzzer/*`（StdPeriph：PB12 低电平触发）
> - `qu_dong/LightSensor/*`（StdPeriph：PB13 上拉输入）

---

## 5. 测试方案与 msh 命令说明（当前实现）

- **命令集合**
  - `sw_start`：开始/继续计时
  - `sw_stop`：暂停计时
  - `sw_reset`：复位（清零时间与圈速）
  - `sw_lap`：记录一圈（保存自上圈以来的用时）
  - `sw_status`：打印当前状态、当前时间、圈速统计（数量、最快/最慢/平均）
  - `sw_csv on [period_ms]`：开启周期性 CSV 输出（默认 200ms），格式见下
  - `sw_csv off`：关闭 CSV 输出
  - `sw_csv_header on|off`：在下一行数据输出前打印一次表头
  - `sw_timefmt human|ms`：切换 CSV 时间格式（人类可读 mm:ss.mmm 或原始 ms）
  - `sw_beep on|off`：开启/关闭提示音
  - `sw_light on|off`：开启/关闭光敏联动（黑暗静音+OLED降帧）
  - `sw_light_invert on|off`：光敏极性反转开关（不同模块 DO 逻辑相反时使用）
- `sw_oled_rate <ms>`：设置 OLED 刷新周期（ms），建议 ≥10ms（如出现抖动可用 20ms）
  - `sw_page main|laps`：切换 OLED 页面（主界面/圈速列表）
  - `sw_clear_laps`：清空圈速记录
  - `sw_laps_prev`/`sw_laps_next`：圈速页向前/向后翻页（每页 6 条）

- **CSV 行格式（串口输出）**
  - `t_ms,lap_index,lap_delta_ms,total_ms`
  - 例：`12345,3,2500,12345` 表示当前时间 12.345s，第 3 圈，上一圈用时 2.5s，总计 12.345s

- **圈速说明**
  - 计算：`lap_delta_ms = 当前累计用时 − 上次 lap 的累计用时`
  - 首圈：复位后首次 `sw_lap` 的值等于自启动以来的用时
  - 暂停：暂停期间不累计，用时自动排除暂停时长
  - 容量：最多 20 圈，超出后覆盖最早一圈；`sw_status` 可查看统计

- **里程碑测试用例**
  1) 核心计时
     - 步骤：`sw_reset` → `sw_start` 3s → `sw_lap` → 2s → `sw_stop` → `sw_status`
     - 期望：状态 RUNNING→PAUSED；lap 数=1；累计约 5s；误差 <±10ms
  2) LED 指示
     - RUNNING：`PC13` 2Hz 闪烁；PAUSED：`PB0` 常亮；错误/溢出：`PB1` 闪烁
  3) 蜂鸣器
     - `sw_beep on` 后执行 `sw_start/sw_stop/sw_lap`，听到短促“滴”提示
  4) OLED
    - 主界面 mm:ss.cc 平滑刷新；底部显示最近一圈（mm:ss.cc）
    - Laps 页底部统计 m/M/a 恢复为毫秒显示（例如 m123 M456 a234）
    - 可用 `sw_oled_rate <ms>` 调整刷新周期（最小 10ms，建议≥10ms）；开启光敏联动时黑暗场景自动 500ms
  5) CSV 流
     - `sw_csv on 100` 后串口持续输出；`sw_csv_header on` 打印一次表头；`sw_timefmt human` 输出 mm:ss.mmm；`sw_csv off` 停止

---

## 6. 使用说明与接线

- **电源与调试**
  - 供电：开发板 3.3V；OLED 与传感器共地
  - ST-Link：SWDIO→`PA13`，SWCLK→`PA14`，NRST→NRST，共地
  - 串口（CH340）：`PA9`(USART1_TX)→CH340_RX，`PA10`(USART1_RX)←CH340_TX，GND 共地（3.3V 逻辑）

- **OLED（SSD1306 I2C 4 引脚）**
  - VCC→3.3V，GND→GND，SCL→`PB8`，SDA→`PB9`（开漏+上拉，若模块无上拉需 4.7k）
  - 方向与地址：常见 I2C 地址 0x3C（写地址 0x78），默认横屏 128x64
  - 现象与排查：不亮/花屏→检查接线、上拉、电源纹波、线长；必要时互换 SCL/SDA 验证
  - 体积优化：默认关闭 UTF8 汉字字库以减小固件体积（仅英文/数字/符号显示）
  - 默认地址：本工程默认 `0x78`（7bit 地址 0x3C 左移一位）。如需支持 0x7A，请在 `applications/ui_oled.c` 中调用 `OLED_SetI2CAddress(0x7A)` 后再 `OLED_Init()`

- **LED（3 路，示例引脚，可改）**
  - LED1（运行）：`PC13` → 电阻 → LED → GND（也可反接）
  - LED2（暂停）：`PB0` → 电阻 → LED → GND
  - LED3（错误）：`PB1` → 电阻 → LED → GND
  - 若为低电平点亮，请在 `applications/indicator_led.c` 中调整 `led_set()` 逻辑

- **蜂鸣器（有源，低电平触发）**
  - VCC→3.3V，GND→GND，SIG→`PB12`；输出低电平响
  - 命令：`sw_beep on|off`；事件：start/stop/reset/lap 短促提示

- **光敏传感器（4 引脚模块）**
  - VCC→3.3V，GND→GND，AO→可选接 ADC（如 `PA0`），DO→`PB13`（数字输入，上拉）
  - 后续联动（可选）：黑暗→自动静音与 OLED 降帧；亮度恢复→恢复正常

- **Kconfig 建议**
  - `RT_USING_DEVICE=y`
  - `RT_USING_FINSH=y`
  - `FINSH_USING_MSH=y`
  - `FINSH_USING_MSH_ONLY=y`
  - `RT_USING_PIN=y`
  - 如使用硬件定时器：`RT_USING_HWTIMER=y`

### 上电与基本使用

- 上电后串口（115200 8N1）出现 msh 提示符；输入 `help` 可见命令列表。
- 基本流程：
  1) `sw_reset` 清零
  2) `sw_start` 开始计时（OLED 时间走动，`PC13` 闪烁）
  3) 等数秒 → `sw_lap` 记录一圈（OLED 底部显示最近一圈毫秒值）
  4) `sw_stop` 暂停（`PB0` 常亮）
  5) `sw_status` 查看统计；`sw_csv on 200` 导出 CSV
  6) `sw_beep on/off` 控制蜂鸣提示
  7) `sw_light on/off` 控制光敏联动

### 接线图（文字版）

- OLED: 3V3→VCC, GND→GND, PB8→SCL, PB9→SDA
- CH340: PA9→RX, PA10→TX, 共地
- LED: PC13/PB0/PB1→电阻→LED→GND
- 蜂鸣器: PB12→SIG, 3V3→VCC, GND→GND（低电平响）
- 光敏: 3V3→VCC, GND→GND, PB13←DO, PA0←AO(可选)

### 引脚-外设对照表

| 功能 | MCU 引脚 | 备注 |
|------|----------|------|
| SWDIO | `PA13` | ST-Link |
| SWCLK | `PA14` | ST-Link |
| 串口 TX | `PA9` | 到 CH340_RX |
| 串口 RX | `PA10` | 从 CH340_TX |
| OLED SCL | `PB8` | I2C 时钟（开漏+上拉） |
| OLED SDA | `PB9` | I2C 数据（开漏+上拉） |
| LED 运行 | `PC13` | 闪烁指示 |
| LED 暂停 | `PB0` | 常亮指示 |
| LED 错误 | `PB1` | 预留 |
| 蜂鸣器 SIG | `PB12` | 有源、低电平触发 |
| 光敏 DO | `PB13` | 数字输入，上拉 |
| 光敏 AO | `PA0` | 模拟输入（可选） |

### ASCII 连接示意（简化）

```
        +---------------- STM32F103C8T6 ----------------+
        |                                                |
 ST-LINK| SWDIO(PA13)  SWCLK(PA14)                      |
 CH340  | RX<-PA9   TX->PA10                            |
        |                                                |
 OLED   | VCC-3V3  GND-GND  SCL-PB8  SDA-PB9            |
        |                                                |
 LEDs   | PC13--R-->|--GND    PB0--R-->|--GND    PB1--R-->|--GND
        |                                                |
 BUZZER | VCC-3V3  GND-GND  SIG-PB12 (低电平响)         |
        |                                                |
 LDR    | VCC-3V3  GND-GND  DO-PB13  AO-PA0(可选)       |
        +-----------------------------------------------+
```

### 最小连线清单（快速复现）

- 供电与串口：
  - 3.3V→板卡 VCC，GND 共地
  - CH340：PA9→RX，PA10→TX，GND 共地（115200 8N1）
- OLED（必接）：
  - VCC→3.3V，GND→GND，SCL→PB8，SDA→PB9（需上拉，若模块无上拉请加 4.7k）
- 指示灯（可选，建议接 1 颗）：
  - 运行灯：PC13 → 330Ω~1k 电阻 → LED → GND
- 蜂鸣器（可选）：
  - 有源蜂鸣器 SIG→PB12，VCC→3.3V，GND→GND（低电平响）

最小功能验证步骤：上电后串口 `sw_start` → OLED 时间走动；若接 PC13 LED 则闪烁；若接蜂鸣器且 `sw_beep on`，命令触发短促提示音。

### 快速功能验证清单（串口命令）

1) 基础计时：
   - `sw_reset` → `sw_start` 等3~5s → `sw_lap` → 再等2~3s → `sw_stop` → `sw_status`
2) 蜂鸣器：
   - `sw_beep on` 后执行 `sw_start/sw_lap/sw_stop` 各一次，应有短促提示音；`sw_beep off` 关闭
3) OLED 页面与刷新：
   - `sw_oled_rate 80` 调刷新；`sw_page laps` 查看圈速页；`sw_laps_next/prev` 翻页；`sw_clear_laps` 清空圈速；`sw_page main` 返回
4) CSV 输出：
   - `sw_timefmt ms` → `sw_csv_header on` → `sw_csv on 200`；观察输出若干行后 `sw_timefmt human` 切换为 mm:ss.mmm；`sw_csv off` 停止
5) 光敏联动（若接 DO=PB13）：
   - `sw_light on`；遮挡/放开应在约150ms内切换为静音+降帧/恢复；如逻辑相反用 `sw_light_invert on`

---

## 7. 可能遇到的问题与规避

- HAL/StdPeriph/RT-Thread pin API 混用：建议统一一套接口（优先 RT-Thread pin 或 HAL），并建立适配层；注意头文件冲突
- OLED 不亮/花屏：检查 PB8/PB9 开漏与上拉、电源 3.3V、I2C 地址（0x3C/0x3D）、线长与干扰
- CH340 电平：确保 3.3V 逻辑；波特率与串口配置匹配（默认 115200 8N1）
- 蜂鸣器低电平触发：初始化默认拉高；避免开机自鸣
- 光敏 DO 抖动：需要去抖/迟滞；AO 接 ADC 时请串电阻与 RC 滤波
- 计时抖动：避免在计时线程执行 OLED 刷新/printf；必要时启用 hwtimer
- GPIO 复用冲突：核对引脚是否与 JTAG/SWD/USART 冲突，必要时改管脚

---

## 9. 常见接线故障快速排查表

| 现象 | 快速排查 | 进一步处理 |
|------|----------|------------|
| OLED 全黑不亮 | 确认 3.3V 供电、GND 共地；检查 PB8/PB9 是否接反 | 为开漏+上拉：若模块无上拉，加 4.7k 到 3.3V；检查地址 0x3C；线短于 20cm |
| OLED 花屏/闪烁 | 线太长或干扰 | 加强上拉、缩短线缆、降低刷新频率；检查供电纹波 |
| 串口无输出 | 波特率 115200 8N1；PA9→RX、PA10←TX 是否反接 | 检查 CH340 驱动；更换 USB 线；确认共地 |
| LED 不亮 | LED 极性/限流电阻值；是否低电平点亮 | 在 `indicator_led.c` 里反向输出电平或换引脚 |
| 蜂鸣器无声 | PB12 是否接到 SIG，模块是否有源且低电平触发 | `sw_beep on` 打开；测量 PB12 电平是否拉低 |
| `sw_csv` 无输出 | 已 `sw_csv on` 且 period>10ms | 确认串口流量；必要时调大 period |
| `sw_lap` 显示异常 | 复位后首次 lap 即为启动至今用时 | 暂停期间不累计；超过 20 圈会覆盖最早记录 |

## 8. 变更记录与维护约定（每次修改实时更新）

- 2025-10-25 v0.1
  - 建立项目文档：需求/可行性/架构/里程碑/测试/msh 命令/接线/问题
  - 约定后续每次功能变更在此处追加版本与要点

- 2025-10-25 v0.2
  - 新增核心计时服务 `applications/stopwatch.c/.h`，含线程与互斥保护
  - 实现基础 msh 命令：`sw.start|stop|reset|lap|status|csv`（并提供下划线备选命令）
  - `applications/main.c` 集成初始化；CSV 通过软定时器周期输出
  - 临时屏蔽 `qu_dong/*` 参考驱动的编译（保留源码作参考），避免 `main.h/stm32f10x.h` 依赖导致的构建失败；后续将以 RT-Thread pin/HAL 重构集成
  - 为保持可编译，`qu_dong/Buzzer/*` 与 `qu_dong/LightSensor/*` 暂提供空实现桩函数（不做任何硬件操作）
  - 修复编译宏兼容性：将 `RT_UNUSED(x)` 替换为标准 `(void)x`，并将带点的别名 `sw.start` 改为 `sw_start`（msh 导出不支持别名中含点）

- 2025-10-26 v0.3
  - 新增 LED 指示模块 `applications/indicator_led.c/.h`，映射：`PC13` 运行闪烁、`PB0` 暂停常亮、`PB1` 错误预留
  - 在 `applications/main.c` 初始化 `indicator_led_init()`，随秒表状态自动更新

- 2025-10-26 v0.4
  - 新增蜂鸣器模块 `applications/notifier_buzzer.c/.h`，默认 `PB12` 低电平响
  - `sw_start/stop/reset/lap` 命令触发短促提示音；新增 `sw_beep on|off` 控制
  - 在 `applications/main.c` 初始化 `notifier_buzzer_init()`

- 2025-10-26 v0.5
  - 集成 OLED UI：`applications/ui_oled.c/.h`，主界面显示当前时间与最近一圈
  - 启用软 I2C1：`board.h` 定义 `BSP_USING_I2C1`，SCL=`PB8`、SDA=`PB9`
  - `main.c` 初始化 `ui_oled_init()`，100ms 刷新
  - 解除 `qu_dong/OLED/OLED.c` 禁用，并适配为 RT-Thread `rt_pin_*` 位操作（PB8/PB9 开漏），以提供 `OLED_*` 符号

- 2025-10-26 v0.6
  - 增加“引脚-外设对照表”和“ASCII 连接示意”到使用说明

- 2025-10-26 v0.7
  - 新增“最小连线清单（快速复现）”，便于快速搭建与验证

- 2025-10-26 v0.8
  - 新增“常见接线故障快速排查表”

- 2025-10-26 v0.9
  - 新增光敏联动：`sw_light on|off`，黑暗→静音、OLED 刷新 300ms；亮度恢复→10ms（可通过 `sw_oled_rate` 覆盖）

- 2025-10-26 v0.10
  - 新增光敏极性反转命令：`sw_light_invert on|off`，用于适配不同 DO 逻辑

- 2025-10-26 v0.11
  - 新增命令 `sw_oled_rate <ms>`，可在明亮环境手动缩短刷新周期（最低建议 50ms）

- 2025-10-26 v0.12
  - UI 增强：新增圈速列表页（`sw_page laps`）、清空圈速（`sw_clear_laps`）

- 2025-10-26 v0.13
  - UI 增强：圈速页翻页（`sw_laps_prev/next`），底部显示 min/max/avg 统计

- 2025-10-26 v0.14
  - CSV 增强：`sw_csv_header on|off`、`sw_timefmt human|ms` 支持

- 2025-10-26 v0.15
  - 文档：补充默认 I2C 地址（0x78）与地址切换说明；新增“快速功能验证清单”

- 2025-10-26 v0.16
  - OLED：时间显示从 mm:ss.mmm 调整为 mm:ss.cc（两位厘秒）
  - OLED 刷新：默认 10ms，`sw_oled_rate` 最小 10ms（如抖动建议 20ms）
  - CLI：`sw_lap` 输出调整为 mm:SS:cc（分两位:秒两位:厘秒两位）
  - Laps 底部统计：改回毫秒单位（m/M/a 为 ms），以保证显示完整

- 2025-10-26 v0.17
  - 文档：新增目录 `技术文档/`，包含《数据手册与参考手册》《原理图与硬件设计图》《软件API与开发文档》草稿
  - 新增：《详细使用说明书》（Markdown，可导出 PDF）
  - 完善：《详细使用说明书》第5节，逐条命令作用/参数/回显/注意事项

- 2025-10-27 v0.19
  - ROM 优化：禁用未用驱动（ETH/SDIO/USB/QSPI/PWM/ADC/SPI/RTC/WDT），裁剪 OLED 未用函数
  - OLED：主界面局部刷新与“仅变化刷新”策略，进一步降低传输量

- 2025-10-27 v0.20
  - OLED：回滚为“固定矩形每帧局部刷新”（主时间与最近一圈），保留 Lap 标签常驻；综合流畅度与实现复杂度
  - 文档：新增 `技术文档/简历-嵌入式-模板.doc`、`技术文档/HR问答-秒表项目.doc`

---

附：参考驱动目录（仅作时序与引脚参考，RT-Thread 下将做适配）
- `qu_dong/OLED/*`（PB8/PB9 I2C 软件时序）
- `qu_dong/Buzzer/*`（PB12 低电平触发）
- `qu_dong/LightSensor/*`（PB13 上拉输入）
