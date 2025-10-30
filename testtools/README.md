# RT-Thread Stopwatch 自动化测试工具

本工具包提供完整的自动化测试方案，通过串口与开发板通信，自动执行所有测试项目并生成真实的测试数据。

---

## 📋 测试项目清单

| 测试项目 | 测试内容 | 预期结果 | 简历指标 |
|---------|---------|---------|---------|
| **1. 计时精度** | 测量设备与主机时间误差 | < 5 ms/分钟 | 计时误差 < 5 ms/分钟 |
| **2. 圈速功能** | 记录20圈并统计min/max/avg | 容量20圈 | 圈速容量 20 |
| **3. CSV周期** | 分析CSV输出间隔稳定性 | 平均误差<10%，抖动<15% | CSV 周期 ≥100 ms |
| **4. 命令响应** | 测试所有msh命令响应时间 | 全部响应成功 | - |
| **5. 长时间运行** | 连续运行5分钟检查漂移 | 漂移<500ms，无错误 | - |

---

## 🚀 快速开始

### 方法1：使用启动脚本（推荐）⭐

**在 Git Bash 中：**

```bash
# 1. 进入testtools目录
cd /d/RT-ThreadStudio/workspace/6/testtools

# 2. 赋予执行权限（首次）
chmod +x run_test.sh

# 3. 运行脚本
./run_test.sh
```

脚本会自动：
- ✅ 检查Python环境
- ✅ 安装pyserial（如需要）
- ✅ 交互式配置测试参数
- ✅ 执行测试并生成报告

---

### 方法2：直接运行Python脚本

```bash
# 完整测试（约10分钟）
python stopwatch_autotest.py \
    --port COM5 \
    --tests all \
    --output ../test_results.json

# 快速测试（仅计时+CSV，约2分钟）
python stopwatch_autotest.py \
    --port COM5 \
    --tests timing csv \
    --timing-duration 30 \
    --csv-duration 10 \
    --output ../test_results_quick.json

# 单项测试
python stopwatch_autotest.py \
    --port COM5 \
    --tests timing \
    --timing-duration 60 \
    --output ../test_timing.json
```

---

## 📊 命令参数说明

### 必需参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `--port` | 串口号 | `COM5` 或 `/dev/ttyUSB0` |

### 可选参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--baud` | 115200 | 串口波特率 |
| `--tests` | all | 测试项目: `timing` `lap` `csv` `cmd` `longrun` `all` |
| `--timing-duration` | 60 | 计时精度测试时长(秒) |
| `--lap-count` | 20 | 圈速测试数量 |
| `--csv-period` | 100 | CSV测试周期(ms) |
| `--csv-duration` | 15 | CSV测试时长(秒) |
| `--longrun-duration` | 5 | 长时间运行测试(分钟) |
| `--output` | - | 输出JSON报告文件路径 |

---

## 📈 测试输出示例

### 终端输出

```
════════════════════════════════════════════════════════════
测试 1: 计时精度测试 (60秒)
════════════════════════════════════════════════════════════
[INFO] 开始计时，目标时长: 60 秒...
[结果] 设备时间: 60023 ms
[结果] 主机时间: 60015 ms
[结果] 误差: +8 ms (+133.3 ppm)
[结果] 标准: < 83.3 ppm (5ms/min)
[结果] ❌ 失败

════════════════════════════════════════════════════════════
测试 2: 圈速功能测试 (20圈)
════════════════════════════════════════════════════════════
[INFO] 记录 20 圈速，每圈间隔 0.5-2 秒...
[进度] 已记录 5/20 圈
[进度] 已记录 10/20 圈
[进度] 已记录 15/20 圈
[进度] 已记录 20/20 圈
[结果] 记录圈数: 20/20
[结果] 最快圈: 520 ms
[结果] 最慢圈: 1560 ms
[结果] 平均圈: 1040 ms
[结果] 容量测试: ✅ 通过

════════════════════════════════════════════════════════════
测试 3: CSV周期稳定性 (100ms, 15秒)
════════════════════════════════════════════════════════════
[INFO] 采集CSV数据 15 秒...
[结果] 样本数: 150
[结果] 平均间隔: 100.8 ms (目标: 100 ms)
[结果] 标准差: 2.1 ms
[结果] 范围: 98 - 105 ms
[结果] 抖动率: 2.1%
[结果] 丢包数: 0/149
[结果] ✅ 通过

════════════════════════════════════════════════════════════
📊 测试报告汇总
════════════════════════════════════════════════════════════

测试时间: 2025-10-29 14:30:25
串口配置: COM5 @ 115200 bps

✅ 通过: 4/5 项测试
📈 通过率: 80.0%

════════════════════════════════════════════════════════════
📝 简历数据建议
════════════════════════════════════════════════════════════

计时误差: 133.3 ppm (< 1分钟内 8 ms)
圈速容量: 20 圈
CSV周期: ≥100 ms (抖动 2.1%)

💾 报告已保存: ../test_results.json
```

### JSON报告内容

```json
{
  "timestamp": "2025-10-29 14:30:25",
  "port": "COM5",
  "baudrate": 115200,
  "timing_accuracy": {
    "target_duration_s": 60,
    "device_time_ms": 60023,
    "host_time_ms": 60015,
    "error_ms": 8,
    "error_ppm": 133.3,
    "passed": false
  },
  "lap_test": {
    "lap_count": 20,
    "min_lap_ms": 520,
    "max_lap_ms": 1560,
    "avg_lap_ms": 1040.0,
    "capacity_test_passed": true
  },
  "csv_stability": {
    "target_period_ms": 100,
    "sample_count": 150,
    "mean_interval_ms": 100.8,
    "std_dev_ms": 2.1,
    "jitter_percent": 2.1,
    "packet_loss_count": 0,
    "passed": true
  }
}
```

---

## 🔧 准备工作

### 1. 硬件准备

- ✅ 开发板已连接到电脑（USB线）
- ✅ 开发板已上电并运行固件
- ✅ 串口驱动已安装（CH340）
- ✅ 确认串口号（设备管理器）

### 2. 软件环境

**安装Python：**
```bash
# 检查Python版本
python --version
# 或
python3 --version
```

**安装pyserial：**
```bash
pip install pyserial
# 或
python -m pip install pyserial
```

### 3. 开发板准备

**确保固件正常运行：**
- 串口能够进入msh命令行
- OLED正常显示
- 执行 `sw_status` 有正确回显

---

## 📝 测试流程详解

### 测试1：计时精度

```
1. 发送 sw_reset 复位
2. 发送 sw_start 开始计时
3. 主机等待60秒（可配置）
4. 发送 sw_stop 停止计时
5. 发送 sw_status 获取设备时间
6. 对比设备时间与主机时间，计算误差
7. 判断是否 < 5ms/分钟（83.3 ppm）
```

### 测试2：圈速功能

```
1. 发送 sw_reset 复位
2. 发送 sw_start 开始计时
3. 循环20次：
   - 等待0.5-1.5秒
   - 发送 sw_lap 记录圈速
4. 发送 sw_stop 停止计时
5. 发送 sw_status 获取统计数据
6. 验证记录数量、min/max/avg
```

### 测试3：CSV周期稳定性

```
1. 发送 sw_reset 复位
2. 发送 sw_start 开始计时
3. 发送 sw_timefmt ms 设置格式
4. 发送 sw_csv_header on 开启表头
5. 发送 sw_csv on 100 开始CSV输出
6. 采集15秒（可配置）的时间戳数据
7. 发送 sw_csv off 停止输出
8. 计算间隔的均值/标准差/抖动率
9. 判断是否符合稳定性标准
```

---

## ⚠️ 常见问题

### Q1: 串口连接失败

**检查：**
- 设备管理器中确认COM口号
- 关闭其他串口工具（SecureCRT、Xshell）
- 重新插拔USB线
- 确认CH340驱动已安装

### Q2: 找不到 pyserial

**解决：**
```bash
pip install pyserial
# 或
python -m pip install pyserial
```

### Q3: 测试中断/超时

**原因：**
- 开发板未运行或死机
- 串口被占用
- 波特率不匹配

**解决：**
- 重启开发板
- 关闭其他串口程序
- 确认波特率为115200

### Q4: CSV采样不足

**原因：**
- CSV周期太长，采样时间太短
- 串口接收缓冲溢出

**解决：**
- 增加 `--csv-duration`
- 减小 `--csv-period`
- 确保串口线质量良好

### Q5: Git Bash 路径问题

**Windows路径转换：**
```bash
# Windows路径: D:\RT-ThreadStudio\workspace\6\testtools
# Git Bash路径: /d/RT-ThreadStudio/workspace/6/testtools

# 如果路径有空格或中文，用引号
cd "/d/RT-ThreadStudio/workspace/6/testtools"
```

---

## 📊 测试配置建议

### 快速验证（2分钟）

```bash
./run_test.sh
# 选择 "2. 快速测试"
# 或
python stopwatch_autotest.py --port COM5 --tests timing csv \
    --timing-duration 30 --csv-duration 10
```

### 完整测试（10分钟）

```bash
./run_test.sh
# 选择 "1. 完整测试"
# 或
python stopwatch_autotest.py --port COM5 --tests all \
    --timing-duration 60 --csv-duration 15 --longrun-duration 5
```

### 单项深度测试

**计时精度（5分钟）：**
```bash
python stopwatch_autotest.py --port COM5 --tests timing \
    --timing-duration 300
```

**CSV稳定性（多周期）：**
```bash
# 测试50ms周期
python stopwatch_autotest.py --port COM5 --tests csv \
    --csv-period 50 --csv-duration 20

# 测试100ms周期
python stopwatch_autotest.py --port COM5 --tests csv \
    --csv-period 100 --csv-duration 20

# 测试200ms周期
python stopwatch_autotest.py --port COM5 --tests csv \
    --csv-period 200 --csv-duration 20
```

---

## 📁 文件说明

```
testtools/
├── stopwatch_autotest.py    # 主测试脚本（Python）
├── run_test.sh               # Git Bash启动脚本
├── README.md                 # 本文档
└── (测试报告会生成在上级目录)
```

---

## 🎯 测试数据用途

### 1. 填写简历

根据测试结果更新简历数据：

```
• 结果：计时误差 < X ms/分钟；圈速容量 20；
  CSV 周期 ≥100 ms；...
```

### 2. 准备面试

- 保存JSON报告作为证据
- 准备测试过程说明
- 能够解释测试方法和判断标准

### 3. 项目文档

- 将测试报告添加到项目文档
- 记录性能指标变化
- 跟踪优化效果

---

## 🔄 测试流程图

```
开始
  ↓
连接串口 (COM5 @ 115200)
  ↓
┌─────────────────┐
│ 测试1: 计时精度  │ → 设备时间 vs 主机时间
└─────────────────┘
  ↓
┌─────────────────┐
│ 测试2: 圈速功能  │ → 记录20圈，统计min/max/avg
└─────────────────┘
  ↓
┌─────────────────┐
│ 测试3: CSV周期   │ → 采集数据，分析间隔稳定性
└─────────────────┘
  ↓
┌─────────────────┐
│ 测试4: 命令响应  │ → 测试所有msh命令
└─────────────────┘
  ↓
┌─────────────────┐
│ 测试5: 长时间运行│ → 5分钟持续运行，检查漂移
└─────────────────┘
  ↓
生成测试报告 (JSON + 终端输出)
  ↓
结束
```

---

## 📞 技术支持

如遇问题：
1. 检查硬件连接和驱动
2. 查看脚本输出的错误提示
3. 阅读常见问题章节
4. 检查开发板固件是否正常运行

---

**祝测试顺利！获得真实可靠的性能数据。** 🎉




