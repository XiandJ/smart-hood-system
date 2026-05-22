import time
from PyQt5.QtWidgets import QFrame, QVBoxLayout, QHBoxLayout, QLabel, QGridLayout
from PyQt5.QtCore import Qt, pyqtSlot
import pyqtgraph as pg

from .style import ALARM_THRESHOLDS, CURVE_COLORS, SENSOR_NAMES, SENSOR_UNITS, SYSTEM_STATES


class SensorCard(QFrame):
    """单个传感器仪表卡片"""

    def __init__(self, key: str, parent=None):
        super().__init__(parent)
        self.key = key
        self.setObjectName("sensorCard")
        self._build_ui()
        self._is_alarm = False

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 8, 12, 8)

        self.title = QLabel(SENSOR_NAMES[self.key])
        self.title.setObjectName("cardTitle")

        val_row = QHBoxLayout()
        self.value = QLabel("--")
        self.value.setObjectName("cardValue")
        self.unit = QLabel(SENSOR_UNITS[self.key])
        self.unit.setObjectName("cardUnit")
        val_row.addWidget(self.value)
        val_row.addWidget(self.unit)
        val_row.addStretch()

        layout.addWidget(self.title)
        layout.addLayout(val_row)

    @pyqtSlot(float)
    def update_value(self, val: float):
        if self.key == 'curr':
            txt = f"{val:.2f}"
        elif isinstance(val, float):
            txt = f"{val:.1f}"
        else:
            txt = str(val)
        self.value.setText(txt)

        # 报警检测
        threshold = ALARM_THRESHOLDS.get(self.key)
        if threshold is not None and val > threshold:
            if not self._is_alarm:
                self._is_alarm = True
                self.setStyleSheet(
                    "#sensorCard { border: 2px solid #ff4444; background-color: #2a1020; }"
                )
                self.value.setStyleSheet("color: #ff4444; font-size: 28px; font-weight: bold;")
        else:
            if self._is_alarm:
                self._is_alarm = False
                self.setStyleSheet("")
                self.value.setStyleSheet("")


class SensorChart(pg.PlotWidget):
    """传感器波形图 - 自动滚动时间窗口"""

    # 各传感器的显示范围，用于归一化到百分比
    SENSOR_RANGE = {
        'temp': (0, 60),
        'hum':  (0, 100),
        'voc':  (0, 300),
        'pm25': (0, 200),
        'curr': (0, 5),
    }

    WINDOW_SEC = 120  # 显示窗口：2分钟

    def __init__(self, data_store, parent=None):
        super().__init__(parent)
        self._store = data_store
        self._curves = {}
        self._setup_plot()

    def _setup_plot(self):
        self.setBackground('#0f0f23')
        self.showGrid(x=True, y=True, alpha=0.15)
        self.setLabel('left', '数值 (%)')
        self.setLabel('bottom', '时间', units='s')
        self.setYRange(0, 105, padding=0)
        self.setMouseEnabled(x=False, y=False)

        # 添加图例
        legend = self.addLegend(offset=(10, 10))
        legend.setBrush(pg.mkBrush('#16213e'))
        legend.setPen(pg.mkPen('#334'))

        for key, color in CURVE_COLORS.items():
            pen = pg.mkPen(color=color, width=2)
            name = f"{SENSOR_NAMES[key]} ({SENSOR_UNITS[key]})"
            curve = self.plot(pen=pen, name=name)
            self._curves[key] = curve

        self._t0 = None

    def _normalize(self, key: str, val: float) -> float:
        """将传感器值归一化到 0-100"""
        lo, hi = self.SENSOR_RANGE.get(key, (0, 100))
        if hi == lo:
            return 0
        return max(0, min(100, (val - lo) / (hi - lo) * 100))

    def refresh(self):
        """刷新所有曲线，自动滚动时间窗口"""
        latest_x = 0

        for key, curve in self._curves.items():
            times, vals = self._store.get_series(key)
            if not times:
                continue
            if self._t0 is None:
                self._t0 = times[0]
            x = [t - self._t0 for t in times]
            y = [self._normalize(key, v) for v in vals]
            curve.setData(x, y)
            if x:
                latest_x = max(latest_x, x[-1])

        # 自动滚动：X轴始终显示最近 WINDOW_SEC 秒
        if latest_x > self.WINDOW_SEC:
            self.setXRange(latest_x - self.WINDOW_SEC, latest_x, padding=0)
        else:
            self.setXRange(0, self.WINDOW_SEC, padding=0)


class SensorPanel(QFrame):
    """传感器面板：卡片 + 波形图"""

    def __init__(self, data_store, parent=None):
        super().__init__(parent)
        self._store = data_store
        self._cards = {}
        self._build_ui()

        from PyQt5.QtCore import QTimer
        self._timer = QTimer()
        self._timer.timeout.connect(self._refresh_chart)
        self._timer.start(200)

    def _build_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(8)

        # ---- 仪表卡片网格: 2行3列 ----
        cards_layout = QGridLayout()
        cards_layout.setSpacing(8)

        keys = ['temp', 'hum', 'voc', 'pm25', 'curr', 'state']
        for i, key in enumerate(keys):
            if key == 'state':
                card = StateCard()
            else:
                card = SensorCard(key)
            self._cards[key] = card
            cards_layout.addWidget(card, i // 3, i % 3)

        main_layout.addLayout(cards_layout)

        # ---- 波形图 ----
        self.chart = SensorChart(self._store)
        main_layout.addWidget(self.chart, stretch=1)

    @pyqtSlot(dict)
    def on_sensor_data(self, data: dict):
        """收到传感器数据时更新卡片"""
        for key, card in self._cards.items():
            if key == 'state':
                card.update_value(data.get('state', 0), data.get('ai', 0))
            else:
                card.update_value(float(data.get(key, 0)))

    def _refresh_chart(self):
        self.chart.refresh()


class StateCard(QFrame):
    """系统状态卡片"""

    STATE_COLORS = {
        0: '#555555',   # 关机 - 灰
        1: '#00cc66',   # 待机 - 绿
        2: '#e94560',   # 运行中 - 红
        3: '#00d2ff',   # 冷却中 - 蓝
        4: '#ff4444',   # 报警 - 亮红
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("sensorCard")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 8, 12, 8)

        title = QLabel("系统状态")
        title.setObjectName("cardTitle")
        layout.addWidget(title)

        self.state_label = QLabel("--")
        self.state_label.setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #555555;"
        )
        layout.addWidget(self.state_label)

        self.ai_label = QLabel("AI: --")
        self.ai_label.setStyleSheet("font-size: 12px; color: #8892b0;")
        layout.addWidget(self.ai_label)

    def update_value(self, state: int, ai: int):
        name = SYSTEM_STATES.get(state, f"未知({state})")
        color = self.STATE_COLORS.get(state, '#555555')
        self.state_label.setText(name)
        self.state_label.setStyleSheet(
            f"font-size: 20px; font-weight: bold; color: {color};"
        )
        ai_names = {0: '正常', 1: '轻度油烟', 2: '重度油烟', 3: '危险'}
        self.ai_label.setText(f"AI: {ai_names.get(ai, f'未知({ai})')}")
