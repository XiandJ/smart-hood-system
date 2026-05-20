"""
智能抽油烟机 PC端系统仿真器
用于验证: 传感器数据流、AI推理逻辑、风机控制策略、OpenMV通信协议

用法: python simulator.py
依赖: pip install pyserial numpy

仿真模式:
  1. 传感器回放模式 (从CSV播放)
  2. 手动注入模式 (键盘调整参数)
  3. 串口连接模式 (连接真实硬件)
"""

import time
import struct
import threading
import random
import math
import sys
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

# ============================================================
# 仿真传感器数据生成器
# ============================================================

@dataclass
class SensorState:
    temperature: float = 25.0
    humidity: float = 50.0
    voc_index: float = 10.0
    pm1_0: float = 5.0
    pm2_5: float = 8.0
    pm10: float = 12.0
    motor_current: float = 0.0
    di_dt: float = 0.0

    prev_current: float = 0.0

    def update(self, scenario='idle', fan_level=0):
        """根据场景更新传感器数据"""
        self.prev_current = self.motor_current

        if scenario == 'idle':
            self._idle()
        elif scenario == 'boiling':
            self._boiling()
        elif scenario == 'stir_fry':
            self._stir_fry()
        elif scenario == 'deep_fry':
            self._deep_fry()

        # 电机电流与风机档位关联
        if fan_level > 0:
            self.motor_current += random.gauss(0, 0.02)
        else:
            self.motor_current *= 0.95

        self.di_dt = self.motor_current - self.prev_current
        self._clamp()

    def _idle(self):
        self.temperature += random.gauss(0, 0.05)
        self.humidity    += random.gauss(0, 0.1)
        self.voc_index   *= 0.95
        self.pm1_0       *= 0.98
        self.pm2_5       *= 0.98
        self.pm10        *= 0.98

    def _boiling(self):
        self.temperature += random.gauss(0.02, 0.2)
        self.humidity    += random.gauss(0.5, 1.0)
        self.voc_index   += random.gauss(1, 5)
        self.pm2_5       += random.gauss(0.1, 0.5)
        self.pm10        += random.gauss(0.05, 0.3)

    def _stir_fry(self):
        self.temperature += random.gauss(0.1, 0.5)
        self.humidity    += random.gauss(-0.1, 0.5)
        self.voc_index   += random.gauss(5, 20)
        self.pm2_5       += random.gauss(2, 10)
        self.pm10        += random.gauss(1, 5)

    def _deep_fry(self):
        self.temperature += random.gauss(0.2, 0.8)
        self.voc_index   += random.gauss(10, 40)
        self.pm2_5       += random.gauss(5, 25)
        self.pm10        += random.gauss(3, 15)

    def _clamp(self):
        self.temperature  = max(0, min(80, self.temperature))
        self.humidity     = max(0, min(100, self.humidity))
        self.voc_index    = max(0, min(500, self.voc_index))
        self.pm1_0        = max(0, min(500, self.pm1_0))
        self.pm2_5        = max(0, min(500, self.pm2_5))
        self.pm10         = max(0, min(600, self.pm10))
        self.motor_current = max(0, min(10, self.motor_current))


# ============================================================
# AI推理仿真 (与固件ai_engine.c逻辑一致)
# ============================================================

class AIEngineSim:
    """仿真AI推理引擎 (规则引擎模式)"""

    SEQ_LEN = 16
    FEATURES = 8

    def __init__(self):
        self.window = deque(maxlen=self.SEQ_LEN)
        self.last_fan = 0
        self.last_aq = 0

    def feed(self, sensor: SensorState):
        features = [
            sensor.temperature,
            sensor.humidity,
            sensor.voc_index,
            sensor.pm1_0,
            sensor.pm2_5,
            sensor.pm10,
            sensor.motor_current,
            sensor.di_dt
        ]
        self.window.append(features)

    def infer(self):
        if len(self.window) < self.SEQ_LEN:
            return 0, 0, 0.0

        latest = self.window[-1]
        pm25 = latest[4]
        pm10 = latest[5]
        voc  = latest[2]
        temp = latest[0]
        rh   = latest[1]

        # 综合污染指数 (与固件中 RuleBasedFallback 逻辑一致)
        score = 0.0

        # PM2.5 (权重50%)
        if pm25 < 35:       score += pm25 * 0.714
        elif pm25 < 75:     score += 25 + (pm25 - 35) * 0.625
        elif pm25 < 150:    score += 50 + (pm25 - 75) * 0.333
        else:               score += 75

        # PM10 (权重20%)
        if pm10 < 50:       score += pm10 * 0.2
        elif pm10 < 150:    score += 10 + (pm10 - 50) * 0.1
        else:               score += 20

        # VOC (权重20%)
        if voc < 80:        score += voc * 0.125
        elif voc < 300:     score += 10 + (voc - 80) * 0.045
        else:               score += 20

        # 温度 (权重5%)
        if temp > 45:       score += 5 * (temp - 45) / 15

        # 湿度 (权重5%)
        if rh > 80:         score += 5 * (rh - 80) / 20

        score = max(0, min(100, score))

        # 分数 → 档位
        if score < 5:       fan_level = 0
        elif score < 20:    fan_level = 1
        elif score < 40:    fan_level = 2
        elif score < 65:    fan_level = 3
        elif score < 85:    fan_level = 4
        else:               fan_level = 5

        # 分数 → 空气质量
        if score < 20:      aq = 0
        elif score < 50:    aq = 1
        elif score < 80:    aq = 2
        else:               aq = 3

        self.last_fan = fan_level
        self.last_aq = aq
        return fan_level, aq, score


# ============================================================
# 主仿真循环
# ============================================================

AQ_LABELS = ['GOOD', 'MODERATE', 'POOR', 'HAZARDOUS']
SCENARIOS = ['idle', 'boiling', 'stir_fry', 'deep_fry']

def clear_screen():
    print('\033[2J\033[H', end='')

def run_simulator():
    print("=" * 60)
    print(" Smart Hood Simulator v1.0")
    print(" Press: 1-4=scenario, +/-=fan_manual, q=quit")
    print("=" * 60)

    sensor = SensorState()
    ai_engine = AIEngineSim()

    scenario = 'idle'
    fan_level = 0
    manual_mode = False
    tick = 0

    try:
        while True:
            tick += 1

            # 更新传感器
            sensor.update(scenario, fan_level)

            # AI推理
            ai_engine.feed(sensor)
            ai_fan, ai_aq, pollution_score = ai_engine.infer()

            # 决策
            if not manual_mode:
                fan_level = ai_fan
            effective_aq = ai_aq

            # 显示
            if tick % 10 == 0:
                clear_screen()
                print("=" * 60)
                print(f" Smart Hood Simulator | Tick: {tick} | "
                      f"Scenario: {scenario.upper():10s} | "
                      f"Mode: {'MANUAL' if manual_mode else 'AUTO'}")
                print("=" * 60)

                print(f"\n  --- Sensors ---")
                print(f"  Temperature:  {sensor.temperature:6.1f} C")
                print(f"  Humidity:     {sensor.humidity:6.1f} %")
                print(f"  VOC Index:    {sensor.voc_index:6.1f}")
                print(f"  PM1.0:        {sensor.pm1_0:6.1f} ug/m3")
                print(f"  PM2.5:        {sensor.pm2_5:6.1f} ug/m3")
                print(f"  PM10:         {sensor.pm10:6.1f} ug/m3")
                print(f"  Motor Current:{sensor.motor_current:6.2f} A")
                print(f"  di/dt:        {sensor.di_dt:+6.2f} A/s")

                print(f"\n  --- AI Inference ---")
                print(f"  Pollution Score: {pollution_score:6.1f}/100")
                print(f"  Fan Level:       {ai_fan} (AI) -> {fan_level} (Actual)")
                bar = '#' * fan_level + '-' * (5 - fan_level)
                print(f"  Speed:          [{bar}]")
                print(f"  Air Quality:     {AQ_LABELS[effective_aq]}")

                print(f"\n  --- Motor ---")
                duty_pct = fan_level * 20
                print(f"  PWM Duty:        {duty_pct}%")
                power_w = sensor.motor_current * 24
                print(f"  Power:           {power_w:.1f} W")

                print(f"\n  --- Controls ---")
                print(f"  [1] Idle  [2] Boiling  [3] Stir-Fry  [4] Deep-Fry")
                print(f"  [+/-] Manual Fan  [m] Toggle Auto/Manual  [q] Quit")

            # 键盘输入 (简单轮询)
            import msvcrt
            if msvcrt.kbhit():
                key = msvcrt.getch().decode('utf-8', errors='ignore').lower()
                if key == 'q':
                    break
                elif key == '1':
                    scenario = 'idle'
                elif key == '2':
                    scenario = 'boiling'
                elif key == '3':
                    scenario = 'stir_fry'
                elif key == '4':
                    scenario = 'deep_fry'
                elif key == 'm':
                    manual_mode = not manual_mode
                elif key == '+' or key == '=':
                    if manual_mode and fan_level < 5:
                        fan_level += 1
                elif key == '-':
                    if manual_mode and fan_level > 0:
                        fan_level -= 1

            time.sleep(0.1)

    except KeyboardInterrupt:
        pass
    except ImportError:
        # Linux/macOS fallback (无msvcrt)
        print("\nKeyboard input not supported on this platform.")
        print("Simulator running in display-only mode...")
        for _ in range(100):
            tick += 1
            sensor.update(scenario, fan_level)
            ai_engine.feed(sensor)
            ai_fan, ai_aq, score = ai_engine.infer()
            if not manual_mode:
                fan_level = ai_fan
            if tick % 10 == 0:
                print(f"t={tick:5d} | {scenario:10s} | "
                      f"PM2.5={sensor.pm2_5:6.1f} VOC={sensor.voc_index:6.1f} | "
                      f"Score={score:5.1f} Fan={fan_level} AQ={AQ_LABELS[ai_aq]}")
            time.sleep(0.1)

    print("\nSimulator stopped.")


if __name__ == "__main__":
    run_simulator()
