import time
import json
import asyncio
import threading
from collections import deque
from PyQt5.QtCore import QObject, pyqtSignal


class DataStore(QObject):
    """传感器数据环形缓冲区，线程安全"""

    MAX_POINTS = 600  # 2分钟 @ 5Hz

    # 信号：数据更新
    updated = pyqtSignal(dict)

    def __init__(self):
        super().__init__()
        self._lock = threading.Lock()

        # 每个传感器独立 deque，存储 (timestamp, value)
        self.temp = deque(maxlen=self.MAX_POINTS)
        self.hum  = deque(maxlen=self.MAX_POINTS)
        self.voc  = deque(maxlen=self.MAX_POINTS)
        self.pm25 = deque(maxlen=self.MAX_POINTS)
        self.curr = deque(maxlen=self.MAX_POINTS)

        # 最新快照
        self._latest = {
            'temp': 0, 'hum': 0, 'voc': 0,
            'pm25': 0, 'curr': 0, 'fan': 0,
            'state': 0, 'ai': 0, 'ts': 0
        }

    def push(self, data: dict):
        """接收一帧传感器数据"""
        with self._lock:
            now = time.time()
            self.temp.append((now, data.get('temp', 0)))
            self.hum.append((now, data.get('hum', 0)))
            self.voc.append((now, data.get('voc', 0)))
            self.pm25.append((now, data.get('pm25', 0)))
            self.curr.append((now, data.get('curr', 0)))
            self._latest.update(data)

        self.updated.emit(data)

    @property
    def latest(self):
        with self._lock:
            return dict(self._latest)

    def get_series(self, name: str):
        """获取某传感器的时间序列数据"""
        with self._lock:
            buf = getattr(self, name, None)
            if buf is None:
                return [], []
            times = [t for t, _ in buf]
            vals  = [v for _, v in buf]
            return times, vals
