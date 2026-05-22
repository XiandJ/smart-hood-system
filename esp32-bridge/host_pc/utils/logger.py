import csv
import time
import os
from datetime import datetime


class SensorLogger:
    """传感器数据 CSV 记录器"""

    COLUMNS = ['timestamp', 'temp', 'hum', 'voc', 'pm25', 'curr', 'fan', 'state', 'ai']

    def __init__(self, log_dir: str = "logs"):
        self._log_dir = log_dir
        self._writer = None
        self._file = None

        if not os.path.exists(log_dir):
            os.makedirs(log_dir)

        filename = datetime.now().strftime("sensor_%Y%m%d_%H%M%S.csv")
        filepath = os.path.join(log_dir, filename)
        self._file = open(filepath, 'w', newline='', encoding='utf-8')
        self._writer = csv.DictWriter(self._file, fieldnames=self.COLUMNS)
        self._writer.writeheader()
        self._file.flush()
        print(f"[LOG] 数据记录到 {filepath}")

    def log(self, data: dict):
        """记录一帧数据"""
        if self._writer is None:
            return
        row = {
            'timestamp': datetime.now().strftime("%H:%M:%S.%f")[:-3],
            'temp':  data.get('temp', 0),
            'hum':   data.get('hum', 0),
            'voc':   data.get('voc', 0),
            'pm25':  data.get('pm25', 0),
            'curr':  data.get('curr', 0),
            'fan':   data.get('fan', 0),
            'state': data.get('state', 0),
            'ai':    data.get('ai', 0),
        }
        self._writer.writerow(row)
        self._file.flush()

    def close(self):
        if self._file:
            self._file.close()
            self._file = None
            self._writer = None
