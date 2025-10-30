#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RT-Thread Stopwatch 项目 - 完整自动化测试套件
测试项目：计时精度、圈速功能、CSV周期、命令响应、长时间稳定性
"""

import argparse
import json
import re
import statistics
import sys
import time
from dataclasses import dataclass, asdict, field
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Dict, Any

try:
    import serial
except ImportError:
    print("错误: 缺少 pyserial 模块")
    print("请安装: pip install pyserial")
    sys.exit(1)


@dataclass
class TimingAccuracyResult:
    """计时精度测试结果"""
    target_duration_s: int
    device_time_ms: int
    host_time_ms: int
    error_ms: int
    error_ppm: float  # parts per million
    passed: bool


@dataclass
class LapTestResult:
    """圈速测试结果"""
    lap_count: int
    lap_times_ms: List[int]
    min_lap_ms: int
    max_lap_ms: int
    avg_lap_ms: float
    capacity_test_passed: bool


@dataclass
class CSVStabilityResult:
    """CSV周期稳定性测试结果"""
    target_period_ms: int
    sample_count: int
    mean_interval_ms: float
    std_dev_ms: float
    min_interval_ms: int
    max_interval_ms: int
    jitter_percent: float
    packet_loss_count: int
    passed: bool


@dataclass
class CommandResponseResult:
    """命令响应测试结果"""
    command: str
    response_time_ms: float
    success: bool
    output: str


@dataclass
class LongRunResult:
    """长时间运行测试结果"""
    duration_minutes: int
    final_time_ms: int
    expected_time_ms: int
    drift_ms: int
    error_count: int
    passed: bool


@dataclass
class TestReport:
    """完整测试报告"""
    timestamp: str
    port: str
    baudrate: int
    timing_accuracy: Optional[TimingAccuracyResult] = None
    lap_test: Optional[LapTestResult] = None
    csv_stability: Optional[CSVStabilityResult] = None
    command_tests: List[CommandResponseResult] = field(default_factory=list)
    long_run: Optional[LongRunResult] = None
    summary: Dict[str, Any] = field(default_factory=dict)


class StopwatchTester:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 3.0):
        """初始化测试器"""
        print(f"[INFO] 连接串口: {port} @ {baudrate} bps")
        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=8,
                parity='N',
                stopbits=1,
                timeout=timeout
            )
            time.sleep(1)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print(f"[OK] 串口已连接\n")
        except Exception as e:
            print(f"[ERROR] 串口连接失败: {e}")
            sys.exit(1)

    def close(self):
        """关闭串口"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[INFO] 串口已关闭")

    def send_command(self, cmd: str, wait_prompt: bool = True, timeout: float = 2.0) -> List[str]:
        """发送命令并读取响应"""
        self.ser.write(f"{cmd}\n".encode())
        self.ser.flush()
        
        if not wait_prompt:
            return []
        
        lines = []
        start_time = time.time()
        buffer = ""
        
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting > 0:
                chunk = self.ser.read(self.ser.in_waiting).decode(errors='ignore')
                buffer += chunk
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if line:
                        lines.append(line)
                    if 'msh >' in line:
                        return lines
            time.sleep(0.01)
        
        return lines

    def parse_status_output(self, lines: List[str]) -> Dict[str, Any]:
        """解析 sw_status 输出"""
        result = {}
        
        for line in lines:
            # state: RUNNING
            if line.startswith('state:'):
                result['state'] = line.split(':')[1].strip()
            
            # total: 5023 ms (00:05.023)
            match = re.search(r'total:\s+(\d+)\s+ms', line)
            if match:
                result['total_ms'] = int(match.group(1))
            
            # laps: 5
            match = re.search(r'laps:\s+(\d+)', line)
            if match:
                result['lap_count'] = int(match.group(1))
            
            # min/max/avg
            match = re.search(r'min:\s+(\d+)\s+ms', line)
            if match:
                result['min_ms'] = int(match.group(1))
            
            match = re.search(r'max:\s+(\d+)\s+ms', line)
            if match:
                result['max_ms'] = int(match.group(1))
            
            match = re.search(r'avg:\s+(\d+)\s+ms', line)
            if match:
                result['avg_ms'] = int(match.group(1))
        
        return result

    # ==================== 测试1: 计时精度 ====================
    def test_timing_accuracy(self, duration_s: int = 60) -> TimingAccuracyResult:
        """测试计时精度（误差 < 5ms/分钟）
        
        改进的测试方法：
        1. 先reset并获取初始时间（应为0）
        2. 立即start（不等待响应以减少延迟）
        3. 等待指定时长
        4. 立即stop（不等待响应）
        5. 获取最终时间
        
        使用设备内部时间作为参考，避免串口延迟影响
        """
        print(f"\n{'='*60}")
        print(f"测试 1: 计时精度测试 ({duration_s}秒)")
        print(f"{'='*60}")
        
        # 复位并确认初始状态
        print(f"[INFO] 复位秒表...")
        self.send_command("sw_reset")
        time.sleep(0.5)  # 增加等待时间确保复位完成
        
        # 清空接收缓冲区
        self.ser.reset_input_buffer()
        
        # 启动计时
        print(f"[INFO] 启动秒表...")
        self.send_command("sw_start")
        time.sleep(0.3)  # 确保命令执行完成
        
        # 清空缓冲区，避免之前的数据干扰
        self.ser.reset_input_buffer()
        
        # 记录主机开始时间
        host_start = time.time()
        
        print(f"[INFO] 等待 {duration_s} 秒...")
        time.sleep(duration_s)
        
        # 记录主机结束时间
        host_end = time.time()
        
        # 停止计时
        print(f"[INFO] 停止秒表...")
        self.send_command("sw_stop")
        time.sleep(0.3)
        
        # 获取状态
        print(f"[INFO] 读取设备时间...")
        status_lines = self.send_command("sw_status")
        
        # 调试：打印原始输出
        print(f"[DEBUG] sw_status 输出行数: {len(status_lines)}")
        for i, line in enumerate(status_lines[:10]):  # 只打印前10行
            print(f"[DEBUG] Line {i}: {line}")
        
        host_time_ms = int((host_end - host_start) * 1000)
        status = self.parse_status_output(status_lines)
        device_time_ms = status.get('total_ms', 0)
        
        error_ms = device_time_ms - host_time_ms
        error_ppm = (error_ms / host_time_ms) * 1_000_000 if host_time_ms > 0 else 0
        
        # 误差标准: < 5ms/分钟 = 83.3 ppm
        target_error_ppm = 83.3
        passed = abs(error_ppm) < target_error_ppm
        
        print(f"[结果] 设备时间: {device_time_ms} ms")
        print(f"[结果] 主机时间: {host_time_ms} ms")
        print(f"[结果] 误差: {error_ms:+d} ms ({error_ppm:+.1f} ppm)")
        print(f"[结果] 标准: < {target_error_ppm:.1f} ppm (5ms/min)")
        print(f"[结果] {'✅ 通过' if passed else '❌ 失败'}")
        
        return TimingAccuracyResult(
            target_duration_s=duration_s,
            device_time_ms=device_time_ms,
            host_time_ms=host_time_ms,
            error_ms=error_ms,
            error_ppm=error_ppm,
            passed=passed
        )

    # ==================== 测试2: 圈速功能 ====================
    def test_lap_functionality(self, lap_count: int = 20) -> LapTestResult:
        """测试圈速功能（容量20圈）"""
        print(f"\n{'='*60}")
        print(f"测试 2: 圈速功能测试 ({lap_count}圈)")
        print(f"{'='*60}")
        
        self.send_command("sw_reset")
        self.send_command("sw_start")
        
        print(f"[INFO] 记录 {lap_count} 圈速，每圈间隔 0.5-2 秒...")
        
        for i in range(lap_count):
            time.sleep(0.5 + (i % 3) * 0.5)  # 0.5s, 1s, 1.5s 循环
            self.send_command("sw_lap")
            if (i + 1) % 5 == 0:
                print(f"[进度] 已记录 {i+1}/{lap_count} 圈")
        
        self.send_command("sw_stop")
        status_lines = self.send_command("sw_status")
        status = self.parse_status_output(status_lines)
        
        recorded_count = status.get('lap_count', 0)
        min_lap = status.get('min_ms', 0)
        max_lap = status.get('max_ms', 0)
        avg_lap = status.get('avg_ms', 0)
        
        capacity_passed = (recorded_count == lap_count)
        
        print(f"[结果] 记录圈数: {recorded_count}/{lap_count}")
        print(f"[结果] 最快圈: {min_lap} ms")
        print(f"[结果] 最慢圈: {max_lap} ms")
        print(f"[结果] 平均圈: {avg_lap} ms")
        print(f"[结果] 容量测试: {'✅ 通过' if capacity_passed else '❌ 失败'}")
        
        return LapTestResult(
            lap_count=recorded_count,
            lap_times_ms=[],  # 简化版不逐个解析
            min_lap_ms=min_lap,
            max_lap_ms=max_lap,
            avg_lap_ms=float(avg_lap),
            capacity_test_passed=capacity_passed
        )

    # ==================== 测试3: CSV周期稳定性 ====================
    def test_csv_stability(self, period_ms: int = 100, duration_s: int = 15) -> CSVStabilityResult:
        """测试CSV周期稳定性（周期 ≥100ms）"""
        print(f"\n{'='*60}")
        print(f"测试 3: CSV周期稳定性 ({period_ms}ms, {duration_s}秒)")
        print(f"{'='*60}")
        
        self.send_command("sw_reset")
        self.send_command("sw_start")
        self.send_command("sw_timefmt ms")
        self.send_command("sw_csv_header on")
        self.send_command(f"sw_csv on {period_ms}")
        
        print(f"[INFO] 采集CSV数据 {duration_s} 秒...")
        
        timestamps = []
        start_time = time.time()
        header_seen = False
        buffer = ""
        
        while (time.time() - start_time) < duration_s:
            if self.ser.in_waiting > 0:
                chunk = self.ser.read(self.ser.in_waiting).decode(errors='ignore')
                buffer += chunk
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    if not line or 'msh >' in line:
                        continue
                    
                    if not header_seen and line.startswith('t,'):
                        header_seen = True
                        continue
                    
                    parts = line.split(',')
                    if len(parts) >= 4:
                        try:
                            t_ms = int(parts[0])
                            timestamps.append(t_ms)
                        except ValueError:
                            continue
            
            time.sleep(0.01)
        
        self.send_command("sw_csv off")
        self.send_command("sw_stop")
        
        if len(timestamps) < 3:
            print(f"[错误] CSV采样不足: {len(timestamps)} 条")
            return CSVStabilityResult(
                target_period_ms=period_ms,
                sample_count=0,
                mean_interval_ms=0,
                std_dev_ms=0,
                min_interval_ms=0,
                max_interval_ms=0,
                jitter_percent=0,
                packet_loss_count=0,
                passed=False
            )
        
        intervals = [timestamps[i+1] - timestamps[i] for i in range(len(timestamps) - 1)]
        mean_interval = statistics.mean(intervals)
        std_dev = statistics.stdev(intervals) if len(intervals) > 1 else 0
        min_interval = min(intervals)
        max_interval = max(intervals)
        jitter_pct = (std_dev / period_ms) * 100 if period_ms > 0 else 0
        packet_loss = sum(1 for x in intervals if x > period_ms * 1.5)
        
        # 通过标准: 平均误差<10%, 抖动<15%, 丢包<5%
        passed = (
            abs(mean_interval - period_ms) / period_ms < 0.10 and
            jitter_pct < 15 and
            packet_loss / len(intervals) < 0.05
        )
        
        print(f"[结果] 样本数: {len(timestamps)}")
        print(f"[结果] 平均间隔: {mean_interval:.2f} ms (目标: {period_ms} ms)")
        print(f"[结果] 标准差: {std_dev:.2f} ms")
        print(f"[结果] 范围: {min_interval} - {max_interval} ms")
        print(f"[结果] 抖动率: {jitter_pct:.2f}%")
        print(f"[结果] 丢包数: {packet_loss}/{len(intervals)}")
        print(f"[结果] {'✅ 通过' if passed else '❌ 失败'}")
        
        return CSVStabilityResult(
            target_period_ms=period_ms,
            sample_count=len(timestamps),
            mean_interval_ms=mean_interval,
            std_dev_ms=std_dev,
            min_interval_ms=min_interval,
            max_interval_ms=max_interval,
            jitter_percent=jitter_pct,
            packet_loss_count=packet_loss,
            passed=passed
        )

    # ==================== 测试4: 命令响应 ====================
    def test_command_response(self) -> List[CommandResponseResult]:
        """测试命令响应速度"""
        print(f"\n{'='*60}")
        print(f"测试 4: 命令响应测试")
        print(f"{'='*60}")
        
        commands = [
            "sw_reset",
            "sw_start",
            "sw_stop",
            "sw_lap",
            "sw_status",
            "sw_page main",
            "sw_page laps",
            "sw_beep on",
            "sw_beep off"
        ]
        
        results = []
        
        for cmd in commands:
            start_time = time.time()
            lines = self.send_command(cmd, timeout=1.0)
            response_time = (time.time() - start_time) * 1000
            
            success = len(lines) > 0
            output = '\n'.join(lines[:3])  # 只保留前3行
            
            results.append(CommandResponseResult(
                command=cmd,
                response_time_ms=response_time,
                success=success,
                output=output
            ))
            
            print(f"[测试] {cmd:20s} - {response_time:.1f} ms - {'✅' if success else '❌'}")
        
        return results

    # ==================== 测试5: 长时间运行 ====================
    def test_long_run(self, duration_minutes: int = 5) -> LongRunResult:
        """测试长时间运行稳定性"""
        print(f"\n{'='*60}")
        print(f"测试 5: 长时间运行测试 ({duration_minutes}分钟)")
        print(f"{'='*60}")
        
        self.send_command("sw_reset")
        self.send_command("sw_start")
        
        expected_ms = duration_minutes * 60 * 1000
        error_count = 0
        
        print(f"[INFO] 运行 {duration_minutes} 分钟，每分钟检查一次...")
        
        for minute in range(1, duration_minutes + 1):
            time.sleep(60)
            
            # 检查状态
            status_lines = self.send_command("sw_status")
            status = self.parse_status_output(status_lines)
            current_time = status.get('total_ms', 0)
            expected_time = minute * 60 * 1000
            drift = current_time - expected_time
            
            print(f"[{minute}/{duration_minutes}分钟] 时间: {current_time} ms, 漂移: {drift:+d} ms")
            
            if 'error' in '\n'.join(status_lines).lower():
                error_count += 1
        
        self.send_command("sw_stop")
        final_status_lines = self.send_command("sw_status")
        final_status = self.parse_status_output(final_status_lines)
        final_time_ms = final_status.get('total_ms', 0)
        drift_ms = final_time_ms - expected_ms
        
        # 通过标准: 最终漂移 < 500ms, 无错误
        passed = abs(drift_ms) < 500 and error_count == 0
        
        print(f"[结果] 最终时间: {final_time_ms} ms")
        print(f"[结果] 预期时间: {expected_ms} ms")
        print(f"[结果] 总漂移: {drift_ms:+d} ms")
        print(f"[结果] 错误次数: {error_count}")
        print(f"[结果] {'✅ 通过' if passed else '❌ 失败'}")
        
        return LongRunResult(
            duration_minutes=duration_minutes,
            final_time_ms=final_time_ms,
            expected_time_ms=expected_ms,
            drift_ms=drift_ms,
            error_count=error_count,
            passed=passed
        )


def generate_report(report: TestReport, output_file: Optional[Path] = None):
    """生成测试报告"""
    print(f"\n{'='*60}")
    print(f"📊 测试报告汇总")
    print(f"{'='*60}\n")
    
    print(f"测试时间: {report.timestamp}")
    print(f"串口配置: {report.port} @ {report.baudrate} bps\n")
    
    # 统计通过率
    total_tests = 0
    passed_tests = 0
    
    if report.timing_accuracy:
        total_tests += 1
        if report.timing_accuracy.passed:
            passed_tests += 1
    
    if report.lap_test:
        total_tests += 1
        if report.lap_test.capacity_test_passed:
            passed_tests += 1
    
    if report.csv_stability:
        total_tests += 1
        if report.csv_stability.passed:
            passed_tests += 1
    
    if report.long_run:
        total_tests += 1
        if report.long_run.passed:
            passed_tests += 1
    
    print(f"✅ 通过: {passed_tests}/{total_tests} 项测试")
    print(f"📈 通过率: {passed_tests/total_tests*100:.1f}%\n")
    
    # 简历数据建议
    print(f"{'='*60}")
    print(f"📝 简历数据建议")
    print(f"{'='*60}\n")
    
    if report.timing_accuracy:
        print(f"计时误差: {abs(report.timing_accuracy.error_ppm):.1f} ppm " +
              f"(< {report.timing_accuracy.target_duration_s/60:.0f}分钟内 {abs(report.timing_accuracy.error_ms)} ms)")
    
    if report.lap_test:
        print(f"圈速容量: {report.lap_test.lap_count} 圈")
    
    if report.csv_stability:
        print(f"CSV周期: ≥{report.csv_stability.target_period_ms} ms " +
              f"(抖动 {report.csv_stability.jitter_percent:.1f}%)")
    
    # 保存JSON
    if output_file:
        report_dict = asdict(report)
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(report_dict, f, ensure_ascii=False, indent=2)
        print(f"\n💾 报告已保存: {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='RT-Thread Stopwatch 完整自动化测试套件',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('--port', required=True, help='串口号 (如 COM5 或 /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='波特率 (默认: 115200)')
    parser.add_argument('--tests', nargs='+', 
                       choices=['timing', 'lap', 'csv', 'cmd', 'longrun', 'all'],
                       default=['all'],
                       help='要执行的测试项目')
    parser.add_argument('--timing-duration', type=int, default=60, 
                       help='计时精度测试时长(秒), 默认60')
    parser.add_argument('--lap-count', type=int, default=20, 
                       help='圈速测试数量, 默认20')
    parser.add_argument('--csv-period', type=int, default=100, 
                       help='CSV测试周期(ms), 默认100')
    parser.add_argument('--csv-duration', type=int, default=15, 
                       help='CSV测试时长(秒), 默认15')
    parser.add_argument('--longrun-duration', type=int, default=5, 
                       help='长时间运行测试(分钟), 默认5')
    parser.add_argument('--output', type=Path, help='输出报告文件(JSON)')
    
    args = parser.parse_args()
    
    # 初始化
    print(f"\n{'='*60}")
    print(f"RT-Thread Stopwatch 自动化测试")
    print(f"{'='*60}\n")
    
    tester = StopwatchTester(args.port, args.baud)
    report = TestReport(
        timestamp=datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        port=args.port,
        baudrate=args.baud
    )
    
    tests_to_run = args.tests
    if 'all' in tests_to_run:
        tests_to_run = ['timing', 'lap', 'csv', 'cmd', 'longrun']
    
    try:
        # 执行测试
        if 'timing' in tests_to_run:
            report.timing_accuracy = tester.test_timing_accuracy(args.timing_duration)
        
        if 'lap' in tests_to_run:
            report.lap_test = tester.test_lap_functionality(args.lap_count)
        
        if 'csv' in tests_to_run:
            report.csv_stability = tester.test_csv_stability(args.csv_period, args.csv_duration)
        
        if 'cmd' in tests_to_run:
            report.command_tests = tester.test_command_response()
        
        if 'longrun' in tests_to_run:
            report.long_run = tester.test_long_run(args.longrun_duration)
        
        # 生成报告
        generate_report(report, args.output)
        
    except KeyboardInterrupt:
        print("\n\n[警告] 测试被用户中断")
    except Exception as e:
        print(f"\n[错误] {e}")
        import traceback
        traceback.print_exc()
    finally:
        tester.close()


if __name__ == '__main__':
    main()

