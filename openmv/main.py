"""
智能抽油烟机 - OpenMV 视觉协处理器主程序
运行于: OpenMV Cam H7 Plus (STM32H743VI)
功能: 烟雾检测 / 人员检测 / 手势识别 / UART通信

模型文件需放置于SD卡根目录:
  - smoke_detect.tflite   (FOMO 烟雾检测)
  - person_detect.tflite  (FOMO 人员检测)
  - gesture_model.tflite  (MobileNetV1 手势识别)
"""

import sensor
import image
import time
import tf
import pyb
import ustruct
from machine import UART

# ============================================================
# 配置区
# ============================================================
UART_BAUD = 921600
SMOKE_CONFIDENCE_THRESHOLD = 0.6
PERSON_CONFIDENCE_THRESHOLD = 0.5
GESTURE_CONFIDENCE_THRESHOLD = 0.6
FRAME_SKIP_SMOKE = 1       # 每帧检测烟雾
FRAME_SKIP_PERSON = 5      # 每5帧检测人员
FRAME_SKIP_GESTURE = 3     # 每3帧检测手势
PERSON_ABSENT_FRAMES = 15  # 连续N帧无人判定为离开

# ============================================================
# UART 通信协议
# ============================================================
CMD_HEARTBEAT  = 0x00
CMD_SMOKE      = 0x10
CMD_PERSON     = 0x11
CMD_GESTURE    = 0x12
CMD_FUSION     = 0x1F

class OpenMVProtocol:
    """与主控STM32H562的UART通信协议"""

    def __init__(self, uart):
        self.uart = uart
        self.frame_buf = bytearray(256)

    def _crc16(self, data):
        """CRC16-Modbus"""
        crc = 0xFFFF
        for b in data:
            crc ^= b
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc

    def send_frame(self, cmd, payload=b''):
        """发送数据帧"""
        frame = self.frame_buf
        frame[0] = 0xAA
        frame[1] = 0x55
        frame[2] = cmd
        frame[3] = len(payload)
        idx = 4
        for i in range(len(payload)):
            frame[idx + i] = payload[i]

        crc = self._crc16(memoryview(frame)[2:4 + len(payload)])
        frame[4 + len(payload)] = crc & 0xFF
        frame[5 + len(payload)] = (crc >> 8) & 0xFF

        self.uart.write(memoryview(frame)[:6 + len(payload)])

    def send_smoke_data(self, detected, level, area_px):
        payload = ustruct.pack('>BBH', detected, level, area_px)
        self.send_frame(CMD_SMOKE, payload)

    def send_person_data(self, detected, count, bbox, confidence):
        payload = ustruct.pack('>BBHHHHB',
            detected, count,
            bbox[0], bbox[1], bbox[2], bbox[3],
            confidence)
        self.send_frame(CMD_PERSON, payload)

    def send_gesture_data(self, gesture_id, confidence):
        payload = ustruct.pack('>BB', gesture_id, confidence)
        self.send_frame(CMD_GESTURE, payload)

    def process_command(self):
        """处理来自主控的命令"""
        if self.uart.any() < 6:
            return None
        data = self.uart.read(self.uart.any())
        if not data:
            return None

        for i in range(len(data) - 5):
            if data[i] == 0xAA and data[i + 1] == 0x55:
                cmd = data[i + 2]
                length = data[i + 3]
                if i + 6 + length <= len(data):
                    return cmd, data[i + 4:i + 4 + length]
        return None

# ============================================================
# 视觉检测模块
# ============================================================

class SmokeDetector:
    """FOMO 烟雾检测器"""

    def __init__(self, model_path='smoke_detect.tflite'):
        self.net = None
        self.model_path = model_path
        self.loaded = False

    def load(self):
        try:
            self.net = tf.load(self.model_path, load_to_fb=True)
            self.loaded = True
            return True
        except Exception as e:
            print("SmokeDetector load failed:", e)
            return False

    def detect(self, img):
        """返回: (detected, smoke_level, area_pixels)"""
        if not self.loaded:
            return (0, 0, 0)

        results = self.net.classify(img)
        if not results:
            return (0, 0, 0)

        best = results[0]
        confidence = best.output()[best.output().argmax()]

        if confidence < SMOKE_CONFIDENCE_THRESHOLD:
            return (0, 0, 0)

        rect = best.rect()
        area = rect[2] * rect[3]

        # 烟雾等级: 根据检测面积映射到 0-100
        smoke_level = min(100, int(area * 100 / (img.width() * img.height())))
        return (1, smoke_level, area)


class PersonDetector:
    """FOMO + MobileNetV1 人员检测器"""

    def __init__(self, model_path='person_detect.tflite'):
        self.net = None
        self.model_path = model_path
        self.loaded = False
        self.absent_count = 0

    def load(self):
        try:
            self.net = tf.load(self.model_path, load_to_fb=True)
            self.loaded = True
            return True
        except Exception as e:
            print("PersonDetector load failed:", e)
            return False

    def detect(self, img):
        """返回: (detected, count, bbox_xywh, confidence)"""
        if not self.loaded:
            return (0, 0, (0, 0, 0, 0), 0)

        objects = self.net.classify(img)
        if not objects:
            self.absent_count += 1
            return (0, 0, (0, 0, 0, 0), 0)

        persons = [obj for obj in objects if obj.output().argmax() == 0]
        if not persons:
            self.absent_count += 1
            return (0, 0, (0, 0, 0, 0), 0)

        self.absent_count = 0
        best = max(persons, key=lambda o: o.output()[o.output().argmax()])
        rect = best.rect()
        confidence = int(best.output()[best.output().argmax()] * 100)

        return (1, len(persons),
                (rect[0], rect[1], rect[2], rect[3]), confidence)


class GestureRecognizer:
    """MobileNetV1 手势识别器"""

    GESTURE_MAP = {
        0: "none",
        1: "swipe_up",
        2: "swipe_down",
        3: "swipe_left",
        4: "swipe_right",
        5: "hold"
    }

    def __init__(self, model_path='gesture_model.tflite'):
        self.net = None
        self.model_path = model_path
        self.loaded = False

    def load(self):
        try:
            self.net = tf.load(self.model_path, load_to_fb=True)
            self.loaded = True
            return True
        except Exception as e:
            print("GestureRecognizer load failed:", e)
            return False

    def recognize(self, img):
        """返回: (gesture_id, confidence)"""
        if not self.loaded:
            return (0, 0)

        results = self.net.classify(img)
        if not results:
            return (0, 0)

        best = results[0]
        class_id = best.output().argmax()
        confidence = int(best.output()[class_id] * 100)

        if confidence < GESTURE_CONFIDENCE_THRESHOLD * 100:
            return (0, 0)

        return (class_id, confidence)

# ============================================================
# 主程序
# ============================================================

def main():
    print("=== Smart Hood OpenMV v1.0 ===")
    print("Initializing...")

    # 初始化摄像头
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)  # 320x240
    sensor.skip_frames(30)
    sensor.set_auto_gain(True)
    sensor.set_auto_whitebal(True)

    # 初始化UART
    uart = UART(3, UART_BAUD)
    proto = OpenMVProtocol(uart)

    # 初始化检测器
    smoke_detector = SmokeDetector()
    person_detector = PersonDetector()
    gesture_recognizer = GestureRecognizer()

    # 加载模型
    print("Loading models...")
    smoke_ok = smoke_detector.load()
    person_ok = person_detector.load()
    gesture_ok = gesture_recognizer.load()
    print("Smoke:%s Person:%s Gesture:%s" % (smoke_ok, person_ok, gesture_ok))

    # 发送就绪信号
    proto.send_frame(CMD_HEARTBEAT, b'\x01')

    frame_count = 0
    clock = time.clock()
    last_heartbeat = time.ticks_ms()

    print("Running...")
    while True:
        clock.tick()
        frame_count += 1

        img = sensor.snapshot()

        # --- 烟雾检测 (每帧) ---
        if smoke_ok and frame_count % FRAME_SKIP_SMOKE == 0:
            detected, level, area = smoke_detector.detect(img)
            if detected:
                proto.send_smoke_data(detected, level, area)

        # --- 人员检测 (每5帧) ---
        if person_ok and frame_count % FRAME_SKIP_PERSON == 0:
            detected, count, bbox, conf = person_detector.detect(img)
            proto.send_person_data(detected, count, bbox, conf)

        # --- 手势识别 (每3帧) ---
        if gesture_ok and frame_count % FRAME_SKIP_GESTURE == 0:
            gesture_id, conf = gesture_recognizer.recognize(img)
            if gesture_id != 0:
                proto.send_gesture_data(gesture_id, conf)

        # --- 心跳 (每秒) ---
        if time.ticks_diff(time.ticks_ms(), last_heartbeat) > 1000:
            proto.send_frame(CMD_HEARTBEAT, b'\x00')
            last_heartbeat = time.ticks_ms()

        # --- 处理主控命令 ---
        result = proto.process_command()
        if result:
            cmd, payload = result
            if cmd == 0x22:  # ENABLE_MODEL
                pass  # 动态启用/禁用模型
            elif cmd == 0x23:  # DISABLE_MODEL
                pass
            elif cmd == 0xFF:  # RESET
                import machine
                machine.reset()

        # 调试输出
        if frame_count % 100 == 0:
            print("FPS: %.2f" % clock.fps())

    print("Exit")

# ============================================================
if __name__ == "__main__":
    main()
