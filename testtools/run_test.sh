#!/bin/bash
# RT-Thread Stopwatch 自动化测试 - Git Bash 启动脚本

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   RT-Thread Stopwatch 自动化测试系统                         ║"
echo "║   版本: 1.0                                                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# 检查Python
echo -e "${BLUE}[1/4]${NC} 检查 Python 环境..."
if command -v python3 &> /dev/null; then
    PYTHON_CMD=python3
    echo -e "${GREEN}✓${NC} Python3: $(python3 --version)"
elif command -v python &> /dev/null; then
    PYTHON_CMD=python
    echo -e "${GREEN}✓${NC} Python: $(python --version)"
else
    echo -e "${RED}✗${NC} 未找到 Python"
    echo "请安装 Python: https://www.python.org/downloads/"
    read -p "按回车键退出..."
    exit 1
fi
echo ""

# 检查 pyserial
echo -e "${BLUE}[2/4]${NC} 检查依赖模块..."
if $PYTHON_CMD -c "import serial" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} pyserial 已安装"
else
    echo -e "${YELLOW}!${NC} pyserial 未安装，正在安装..."
    $PYTHON_CMD -m pip install pyserial
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} pyserial 安装成功"
    else
        echo -e "${RED}✗${NC} 安装失败"
        exit 1
    fi
fi
echo ""

# 检查测试脚本
echo -e "${BLUE}[3/4]${NC} 检查测试脚本..."
if [ -f "stopwatch_autotest.py" ]; then
    echo -e "${GREEN}✓${NC} 测试脚本已就绪"
else
    echo -e "${RED}✗${NC} 找不到 stopwatch_autotest.py"
    exit 1
fi
echo ""

# 配置测试参数
echo -e "${BLUE}[4/4]${NC} 配置测试参数"
echo "════════════════════════════════════════════════════════════════"
echo ""

# 询问串口号
read -p "串口号 [默认: COM5]: " PORT
PORT=${PORT:-COM5}

# 询问测试项目
echo ""
echo "选择测试项目:"
echo "  1. 完整测试 (全部项目，约10分钟)"
echo "  2. 快速测试 (计时+CSV，约2分钟)"
echo "  3. 计时精度测试"
echo "  4. CSV周期测试"
echo "  5. 自定义"
echo ""
read -p "请选择 [1]: " TEST_CHOICE
TEST_CHOICE=${TEST_CHOICE:-1}

case $TEST_CHOICE in
    1)
        TESTS="all"
        TIMING_DURATION=60
        CSV_DURATION=15
        LONGRUN_DURATION=5
        ;;
    2)
        TESTS="timing csv"
        TIMING_DURATION=30
        CSV_DURATION=10
        LONGRUN_DURATION=0
        ;;
    3)
        TESTS="timing"
        read -p "测试时长(秒) [60]: " TIMING_DURATION
        TIMING_DURATION=${TIMING_DURATION:-60}
        CSV_DURATION=0
        LONGRUN_DURATION=0
        ;;
    4)
        TESTS="csv"
        read -p "CSV周期(ms) [100]: " CSV_PERIOD
        CSV_PERIOD=${CSV_PERIOD:-100}
        read -p "采样时长(秒) [15]: " CSV_DURATION
        CSV_DURATION=${CSV_DURATION:-15}
        TIMING_DURATION=0
        LONGRUN_DURATION=0
        ;;
    5)
        echo ""
        echo "可选项目: timing lap csv cmd longrun"
        read -p "输入测试项目(空格分隔): " TESTS
        read -p "计时测试时长(秒) [60]: " TIMING_DURATION
        TIMING_DURATION=${TIMING_DURATION:-60}
        read -p "CSV采样时长(秒) [15]: " CSV_DURATION
        CSV_DURATION=${CSV_DURATION:-15}
        read -p "长运行时长(分钟) [5]: " LONGRUN_DURATION
        LONGRUN_DURATION=${LONGRUN_DURATION:-5}
        ;;
    *)
        echo "无效选择，使用默认配置"
        TESTS="all"
        TIMING_DURATION=60
        CSV_DURATION=15
        LONGRUN_DURATION=5
        ;;
esac

# 生成输出文件名
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_FILE="../test_results_${TIMESTAMP}.json"

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "📋 测试配置"
echo "════════════════════════════════════════════════════════════════"
echo "串口号: $PORT"
echo "测试项目: $TESTS"
echo "输出文件: $OUTPUT_FILE"
echo "════════════════════════════════════════════════════════════════"
echo ""
read -p "按回车键开始测试，或 Ctrl+C 取消..."
echo ""

# 构建命令
CMD="$PYTHON_CMD stopwatch_autotest.py --port $PORT --baud 115200"
CMD="$CMD --tests $TESTS"

if [ "$TIMING_DURATION" -gt 0 ]; then
    CMD="$CMD --timing-duration $TIMING_DURATION"
fi

if [ "$CSV_DURATION" -gt 0 ]; then
    CMD="$CMD --csv-duration $CSV_DURATION"
fi

if [ "$LONGRUN_DURATION" -gt 0 ]; then
    CMD="$CMD --longrun-duration $LONGRUN_DURATION"
fi

CMD="$CMD --output $OUTPUT_FILE"

# 运行测试
echo "🚀 启动测试..."
echo "════════════════════════════════════════════════════════════════"
echo ""

eval $CMD

echo ""
echo "════════════════════════════════════════════════════════════════"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} 测试完成"
else
    echo -e "${RED}✗${NC} 测试失败"
fi
echo "════════════════════════════════════════════════════════════════"
echo ""

read -p "按回车键退出..."




