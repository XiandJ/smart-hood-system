"""
SmartHood 智能抽油烟机上位机
启动: python main.py
"""
import sys
import os

# 确保项目根目录在 path 中
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PyQt5.QtWidgets import QApplication
from core.ws_client import WSClient
from core.data_store import DataStore
from utils.logger import SensorLogger
from ui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("SmartHood Monitor")
    app.setStyle("Fusion")  # 跨平台统一风格

    # 核心组件
    ws_client = WSClient()
    data_store = DataStore()
    logger = SensorLogger(log_dir="logs")

    # 主窗口
    window = MainWindow(ws_client, data_store, logger)
    window.show()

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
