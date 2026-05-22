import json
import asyncio
import threading
from PIL import Image
from io import BytesIO
from PyQt5.QtCore import QObject, pyqtSignal
from PyQt5.QtGui import QImage


class WSClient(QObject):
    """WebSocket 客户端，独立线程运行"""

    # 信号
    sensor_received  = pyqtSignal(dict)       # 传感器 JSON
    camera_received  = pyqtSignal(QImage)      # 摄像头图像
    connected        = pyqtSignal()
    disconnected     = pyqtSignal()
    error            = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self._ws = None
        self._loop = None
        self._thread = None
        self._running = False
        self._host = ""
        self._port = 81

    def connect_to(self, host: str, port: int = 81):
        """启动连接"""
        self._host = host
        self._port = port
        self._running = True
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()

    def disconnect(self):
        """断开连接"""
        self._running = False
        if self._loop and self._loop.is_running():
            asyncio.run_coroutine_threadsafe(self._close_ws(), self._loop)

    def send_control(self, fan_level: int):
        """发送风机控制命令"""
        if self._loop and self._loop.is_running():
            msg = json.dumps({"type": "control", "fan": fan_level})
            asyncio.run_coroutine_threadsafe(self._send_text(msg), self._loop)

    def _run_loop(self):
        """线程主循环"""
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect_loop())

    async def _connect_loop(self):
        """断线自动重连"""
        import websockets
        retry = 0
        while self._running:
            uri = f"ws://{self._host}:{self._port}/"
            try:
                async with websockets.connect(
                    uri, ping_interval=20, open_timeout=5, close_timeout=3
                ) as ws:
                    self._ws = ws
                    retry = 0
                    self.connected.emit()
                    async for message in ws:
                        if not self._running:
                            break
                        await self._on_message(message)
            except Exception as e:
                retry += 1
                if retry <= 3:
                    self.error.emit(f"连接失败 (第{retry}次): {e}")
                elif retry == 4:
                    self.error.emit("持续重连中...")
            finally:
                self._ws = None
                self.disconnected.emit()

            if self._running:
                await asyncio.sleep(3)

    async def _on_message(self, message):
        """处理收到的消息"""
        if isinstance(message, str):
            # 文本帧 → JSON → 传感器数据
            try:
                data = json.loads(message)
                if data.get("type") == "sensor":
                    self.sensor_received.emit(data)
            except json.JSONDecodeError:
                pass
        elif isinstance(message, bytes):
            # 二进制帧 → JPEG 图像
            try:
                img = Image.open(BytesIO(message))
                img = img.convert("RGB")
                w, h = img.size
                qimg = QImage(img.tobytes(), w, h, w * 3, QImage.Format_RGB888)
                self.camera_received.emit(qimg.copy())
            except Exception:
                pass

    async def _close_ws(self):
        if self._ws:
            await self._ws.close()

    async def _send_text(self, text: str):
        if self._ws:
            await self._ws.send(text)

    @property
    def is_connected(self):
        return self._ws is not None
