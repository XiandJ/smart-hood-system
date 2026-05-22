from PyQt5.QtWidgets import QFrame, QVBoxLayout, QHBoxLayout, QLabel
from PyQt5.QtGui import QPixmap, QImage
from PyQt5.QtCore import Qt, pyqtSlot


class CameraPanel(QFrame):
    """摄像头实时画面面板"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("sensorCard")
        self._build_ui()
        self._connected = False

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)

        # 标题行
        header = QHBoxLayout()
        self.title = QLabel("OpenMV 摄像头")
        self.title.setObjectName("cardTitle")
        self.title.setStyleSheet("font-size: 13px; color: #e94560; font-weight: bold;")
        header.addWidget(self.title)
        header.addStretch()
        self.res_label = QLabel("")
        self.res_label.setStyleSheet("color: #556; font-size: 11px;")
        header.addWidget(self.res_label)
        layout.addLayout(header)

        # 图像显示
        self.image_label = QLabel()
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setMinimumSize(320, 240)
        self.image_label.setStyleSheet(
            "background-color: #0a0a1a; border-radius: 8px;"
            "color: #445; font-size: 16px;"
        )
        self._show_placeholder()
        layout.addWidget(self.image_label, stretch=1)

    def _show_placeholder(self):
        self.image_label.setText(
            "\n\n  ○  等待摄像头连接..."
        )

    @pyqtSlot(QImage)
    def on_camera_frame(self, img: QImage):
        """收到摄像头图像帧"""
        label_w = self.image_label.width()
        label_h = self.image_label.height()
        scaled = img.scaled(label_w, label_h, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.image_label.setPixmap(QPixmap.fromImage(scaled))
        self.res_label.setText(f"{img.width()}x{img.height()}")

    def set_connected(self, connected: bool):
        self._connected = connected
        if connected:
            self.setStyleSheet(
                "#sensorCard { border: 2px solid #00cc66; }"
            )
        else:
            self.setStyleSheet("")
            self.image_label.clear()
            self._show_placeholder()
            self.res_label.setText("")
