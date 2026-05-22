import time
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QFrame, QLabel, QLineEdit, QPushButton, QStatusBar,
    QSplitter, QShortcut, QFileDialog
)
from PyQt5.QtCore import Qt, pyqtSlot, QTimer
from PyQt5.QtGui import QKeySequence, QColor

from .sensor_panel import SensorPanel
from .camera_panel import CameraPanel
from .control_panel import ControlPanel
from .style import DARK_STYLE


class MainWindow(QMainWindow):
    """主窗口"""

    def __init__(self, ws_client, data_store, logger):
        super().__init__()
        self._ws = ws_client
        self._store = data_store
        self._logger = logger
        self._cam_frame_count = 0
        self._sens_frame_count = 0
        self._last_fps_time = time.time()

        self.setWindowTitle("SmartHood 智能抽油烟机监控系统")
        self.setMinimumSize(1200, 700)
        self.setStyleSheet(DARK_STYLE)

        self._build_ui()
        self._connect_signals()
        self._setup_shortcuts()

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(8, 0, 8, 0)
        root.setSpacing(0)

        # ---- 顶栏 ----
        top_bar = self._build_top_bar()
        root.addWidget(top_bar)

        # ---- 主体分割 ----
        splitter = QSplitter(Qt.Horizontal)

        # 左侧：传感器面板
        self.sensor_panel = SensorPanel(self._store)
        splitter.addWidget(self.sensor_panel)

        # 右侧：摄像头 + 控制
        right_frame = QFrame()
        right_layout = QVBoxLayout(right_frame)
        right_layout.setContentsMargins(8, 0, 0, 0)
        right_layout.setSpacing(8)

        self.camera_panel = CameraPanel()
        self.control_panel = ControlPanel()
        right_layout.addWidget(self.camera_panel, stretch=2)
        right_layout.addWidget(self.control_panel, stretch=1)

        splitter.addWidget(right_frame)
        splitter.setSizes([700, 500])

        root.addWidget(splitter, stretch=1)

        # ---- 底栏 ----
        self._build_status_bar()

    def _build_top_bar(self):
        bar = QFrame()
        bar.setObjectName("topBar")
        bar.setFixedHeight(52)
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(12, 0, 12, 0)

        # Logo + 标题
        title = QLabel("SmartHood")
        title.setStyleSheet("font-size: 20px; font-weight: bold; color: #e94560;")
        layout.addWidget(title)

        subtitle = QLabel("智能抽油烟机监控")
        subtitle.setStyleSheet("font-size: 12px; color: #8892b0; margin-top: 4px;")
        layout.addWidget(subtitle)

        layout.addSpacing(24)

        # 连接状态指示
        self.status_dot = QLabel()
        self.status_dot.setObjectName("statusDot")
        self.status_dot.setStyleSheet(
            "background-color: #ff4444; border-radius: 6px;"
            "min-width:12px; max-width:12px; min-height:12px; max-height:12px;"
        )
        layout.addWidget(self.status_dot)

        self.status_label = QLabel("未连接")
        self.status_label.setStyleSheet("color: #ff4444; font-size: 13px;")
        layout.addWidget(self.status_label)

        layout.addSpacing(16)

        # IP 输入 + 连接
        ip_label = QLabel("ESP32 IP:")
        ip_label.setStyleSheet("color: #8892b0;")
        layout.addWidget(ip_label)
        self.ip_input = QLineEdit("192.168.4.1")
        self.ip_input.setFixedWidth(140)
        self.ip_input.setPlaceholderText("192.168.x.x")
        layout.addWidget(self.ip_input)

        self.connect_btn = QPushButton("连接")
        self.connect_btn.setFixedWidth(70)
        self.connect_btn.setStyleSheet(
            "background-color: #e94560; color: #fff; border: none;"
            "border-radius: 6px; padding: 6px 16px; font-weight: bold;"
        )
        self.connect_btn.clicked.connect(self._on_connect_click)
        layout.addWidget(self.connect_btn)

        layout.addStretch()

        # 导出按钮
        export_btn = QPushButton("导出数据")
        export_btn.clicked.connect(self._on_export_click)
        layout.addWidget(export_btn)

        return bar

    def _build_status_bar(self):
        status_bar = QStatusBar()
        self.setStatusBar(status_bar)

        self.sens_fps_label = QLabel("传感器: 0 Hz")
        self.cam_fps_label = QLabel("图像: 0 fps")
        self.data_count_label = QLabel("数据点: 0")

        status_bar.addPermanentWidget(self.sens_fps_label)
        status_bar.addPermanentWidget(self.cam_fps_label)
        status_bar.addPermanentWidget(self.data_count_label)

        # 帧率统计定时器
        self.fps_timer = QTimer()
        self.fps_timer.timeout.connect(self._update_fps)
        self.fps_timer.start(1000)

    def _connect_signals(self):
        # WS → UI
        self._ws.sensor_received.connect(self._on_sensor)
        self._ws.camera_received.connect(self._on_camera)
        self._ws.connected.connect(self._on_connected)
        self._ws.disconnected.connect(self._on_disconnected)
        self._ws.error.connect(self._on_error)

        # WS → DataStore
        self._ws.sensor_received.connect(self._store.push)

        # WS → Logger
        self._ws.sensor_received.connect(self._logger.log)

        # Control → WS
        self.control_panel.fan_changed.connect(self._ws.send_control)

    def _setup_shortcuts(self):
        QShortcut(QKeySequence("F11"), self, self._toggle_fullscreen)
        QShortcut(QKeySequence("Escape"), self, self._exit_fullscreen)

    def _toggle_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    def _exit_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()

    # ---- WebSocket 事件 ----
    @pyqtSlot()
    def _on_connect_click(self):
        if self._ws.is_connected:
            self._ws.disconnect()
            self.connect_btn.setText("连接")
        else:
            host = self.ip_input.text().strip()
            if host:
                self.connect_btn.setText("连接中...")
                self.connect_btn.setEnabled(False)
                self._ws.connect_to(host)

    @pyqtSlot()
    def _on_connected(self):
        self.status_dot.setStyleSheet(
            "background-color: #00cc66; border-radius: 6px;"
            "min-width:12px; max-width:12px; min-height:12px; max-height:12px;"
        )
        self.status_label.setText("已连接")
        self.status_label.setStyleSheet("color: #00cc66; font-size: 13px;")
        self.connect_btn.setText("断开")
        self.connect_btn.setEnabled(True)
        self.connect_btn.setStyleSheet(
            "background-color: #334; color: #e94560; border: 1px solid #e94560;"
            "border-radius: 6px; padding: 6px 16px; font-weight: bold;"
        )
        self.camera_panel.set_connected(True)

    @pyqtSlot()
    def _on_disconnected(self):
        self.status_dot.setStyleSheet(
            "background-color: #ff4444; border-radius: 6px;"
            "min-width:12px; max-width:12px; min-height:12px; max-height:12px;"
        )
        self.status_label.setText("已断开")
        self.status_label.setStyleSheet("color: #ff4444; font-size: 13px;")
        self.connect_btn.setText("连接")
        self.connect_btn.setEnabled(True)
        self.connect_btn.setStyleSheet(
            "background-color: #e94560; color: #fff; border: none;"
            "border-radius: 6px; padding: 6px 16px; font-weight: bold;"
        )
        self.camera_panel.set_connected(False)

    @pyqtSlot(str)
    def _on_error(self, msg: str):
        self.status_label.setText(f"错误: {msg}")

    @pyqtSlot(dict)
    def _on_sensor(self, data: dict):
        self.sensor_panel.on_sensor_data(data)
        self.control_panel.on_fan_update(data.get('fan', 0))
        self._sens_frame_count += 1

    @pyqtSlot('QImage')
    def _on_camera(self, img):
        self.camera_panel.on_camera_frame(img)
        self._cam_frame_count += 1

    def _update_fps(self):
        now = time.time()
        dt = now - self._last_fps_time
        if dt > 0:
            sens_fps = self._sens_frame_count / dt
            cam_fps = self._cam_frame_count / dt
            self.sens_fps_label.setText(f"传感器: {sens_fps:.1f} Hz")
            self.cam_fps_label.setText(f"图像: {cam_fps:.1f} fps")
        self._sens_frame_count = 0
        self._cam_frame_count = 0
        self._last_fps_time = now

        # 数据点统计
        n = len(self._store.temp)
        self.data_count_label.setText(f"数据点: {n}")

    def _on_export_click(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "导出数据", "sensor_data.csv", "CSV Files (*.csv)"
        )
        if path:
            import csv
            with open(path, 'w', newline='', encoding='utf-8') as f:
                writer = csv.writer(f)
                writer.writerow(['time', 'temp', 'hum', 'voc', 'pm25', 'curr'])
                times, _ = self._store.get_series('temp')
                for i, t in enumerate(times):
                    row = [f"{t:.1f}"]
                    for key in ['temp', 'hum', 'voc', 'pm25', 'curr']:
                        _, vals = self._store.get_series(key)
                        row.append(f"{vals[i]:.2f}" if i < len(vals) else "")
                    writer.writerow(row)

    def closeEvent(self, event):
        self._ws.disconnect()
        self._logger.close()
        event.accept()
