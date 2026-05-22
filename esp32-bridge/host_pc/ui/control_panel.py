from PyQt5.QtWidgets import (
    QFrame, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QSizePolicy
)
from PyQt5.QtCore import Qt, pyqtSignal, pyqtSlot


class ControlPanel(QFrame):
    """风机挡位控制面板"""

    # 信号：用户请求切换挡位
    fan_changed = pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_fan = 0
        self._btns = {}
        self._build_ui()

    def _build_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(12)

        # 标题
        title = QLabel("风机控制")
        title.setObjectName("cardTitle")
        title.setStyleSheet("font-size: 14px; color: #e94560; font-weight: bold;")
        main_layout.addWidget(title, alignment=Qt.AlignCenter)

        # 挡位按钮行
        btn_row = QHBoxLayout()
        btn_row.setSpacing(10)

        for level in range(6):
            btn = QPushButton(str(level))
            btn.setObjectName("fanBtn")
            btn.setCheckable(True)
            btn.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
            btn.clicked.connect(lambda checked, lv=level: self._on_fan_click(lv))
            self._btns[level] = btn
            btn_row.addWidget(btn)

        main_layout.addLayout(btn_row)

        # 紧急停止按钮
        self.stop_btn = QPushButton("紧急停止")
        self.stop_btn.setObjectName("stopBtn")
        self.stop_btn.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.stop_btn.clicked.connect(lambda: self._on_fan_click(0))
        main_layout.addWidget(self.stop_btn, alignment=Qt.AlignCenter)

        main_layout.addStretch()

        # 绑定快捷键
        self._setup_shortcuts()

    def _setup_shortcuts(self):
        """数字键 0-5 快捷切换挡位"""
        from PyQt5.QtWidgets import QShortcut
        from PyQt5.QtGui import QKeySequence
        for level in range(6):
            shortcut = QShortcut(QKeySequence(str(level)), self)
            shortcut.activated.connect(lambda lv=level: self._on_fan_click(lv))

    def _on_fan_click(self, level: int):
        if level != self._current_fan:
            self._current_fan = level
            self._update_buttons()
            self.fan_changed.emit(level)

    def _update_buttons(self):
        active_style = (
            "background-color: #e94560; color: #fff;"
            "border: 2px solid #ff6b81; border-radius: 25px;"
            "min-width: 50px; min-height: 50px;"
            "font-size: 16px; font-weight: bold;"
        )
        inactive_style = (
            "background-color: #16213e; color: #e0e0e0;"
            "border: 2px solid #334; border-radius: 25px;"
            "min-width: 50px; min-height: 50px;"
            "font-size: 16px; font-weight: bold;"
        )
        for lv, btn in self._btns.items():
            btn.setStyleSheet(active_style if lv == self._current_fan else inactive_style)

    @pyqtSlot(int)
    def on_fan_update(self, level: int):
        """ESP32 回传的挡位同步"""
        if level != self._current_fan:
            self._current_fan = level
            self._update_buttons()
