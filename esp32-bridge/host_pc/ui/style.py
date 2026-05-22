# 全局暗色主题样式表
DARK_STYLE = """
QMainWindow {
    background-color: #0f0f23;
}

QWidget {
    color: #e0e0e0;
    font-family: "Microsoft YaHei", "Segoe UI", Arial;
    font-size: 13px;
}

/* 顶部栏 */
QFrame#topBar {
    background-color: #1a1a2e;
    border-bottom: 2px solid #e94560;
    padding: 4px 12px;
}

/* 输入框 */
QLineEdit {
    background-color: #16213e;
    border: 1px solid #334;
    border-radius: 6px;
    padding: 6px 10px;
    color: #fff;
    font-size: 14px;
}
QLineEdit:focus {
    border: 1px solid #e94560;
}

/* 普通按钮 */
QPushButton {
    background-color: #16213e;
    border: 1px solid #334;
    border-radius: 6px;
    padding: 6px 16px;
    color: #e0e0e0;
    font-size: 13px;
}
QPushButton:hover {
    background-color: #1a2744;
    border: 1px solid #e94560;
}
QPushButton:pressed {
    background-color: #e94560;
    color: #fff;
}
QPushButton:disabled {
    background-color: #111;
    color: #555;
}

/* 传感器卡片 */
QFrame#sensorCard {
    background-color: #16213e;
    border-radius: 10px;
    border: 1px solid #223;
}

QLabel#cardTitle {
    color: #8892b0;
    font-size: 12px;
}
QLabel#cardValue {
    color: #e94560;
    font-size: 28px;
    font-weight: bold;
}
QLabel#cardUnit {
    color: #8892b0;
    font-size: 13px;
}

/* 报警卡片 */
QFrame#sensorCard[alarm="true"] {
    border: 2px solid #ff4444;
    background-color: #2a1020;
}

/* 挡位按钮 */
QPushButton#fanBtn {
    min-width: 50px;
    min-height: 50px;
    border-radius: 25px;
    font-size: 16px;
    font-weight: bold;
    background-color: #16213e;
    border: 2px solid #334;
}
QPushButton#fanBtn:hover {
    border: 2px solid #e94560;
}
QPushButton#fanBtn[active="true"] {
    background-color: #e94560;
    color: #fff;
    border: 2px solid #ff6b81;
}

/* 紧急停止 */
QPushButton#stopBtn {
    background-color: #cc0000;
    color: #fff;
    border: 2px solid #ff3333;
    border-radius: 25px;
    min-width: 60px;
    min-height: 60px;
    font-size: 14px;
    font-weight: bold;
}
QPushButton#stopBtn:hover {
    background-color: #ff0000;
}

/* 状态栏 */
QStatusBar {
    background-color: #1a1a2e;
    color: #8892b0;
    border-top: 1px solid #223;
}

/* 连接状态灯 */
QLabel#statusDot {
    min-width: 12px;
    max-width: 12px;
    min-height: 12px;
    max-height: 12px;
    border-radius: 6px;
}
"""

# 报警阈值
ALARM_THRESHOLDS = {
    'pm25': 75,   # PM2.5 > 75 μg/m³ 报警
    'voc': 100,   # VOC > 100 报警
    'temp': 50,   # 温度 > 50°C 报警
    'curr': 3.0,  # 电流 > 3A 报警
}

# 曲线颜色
CURVE_COLORS = {
    'temp': '#e94560',   # 红
    'hum':  '#00d2ff',   # 蓝
    'voc':  '#ffd700',   # 黄
    'pm25': '#ff8c00',   # 橙
    'curr': '#00ff88',   # 绿
}

# 传感器显示名称
SENSOR_NAMES = {
    'temp': '温度',
    'hum':  '湿度',
    'voc':  'VOC',
    'pm25': 'PM2.5',
    'curr': '电流',
}

# 传感器单位
SENSOR_UNITS = {
    'temp': '°C',
    'hum':  '%',
    'voc':  '',
    'pm25': 'μg/m³',
    'curr': 'A',
}

# 系统状态枚举
SYSTEM_STATES = {
    0: '关机',
    1: '待机',
    2: '运行中',
    3: '冷却中',
    4: '报警',
}
