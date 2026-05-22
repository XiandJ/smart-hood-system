#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "config.h"
#include "uart_sensor.h"
#include "frame_buffer.h"

static WebServer      _http(HTTP_PORT);
static WebSocketsServer _ws(WS_PORT);
static uint32_t _lastSensorPush = 0;
static uint32_t _lastCamPush = 0;

// ==================== WebSocket 事件处理 ====================
void _wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] 客户端 #%u 断开\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[WS] 客户端 #%u 连接: %s\n", num, _ws.remoteIP(num).toString().c_str());
            break;
        case WStype_TEXT: {
            Serial.printf("[WS] 收到文本: %s\n", (char *)payload);
            // 解析控制命令并转发给 STM32
            JsonDocument cmd;
            DeserializationError err = deserializeJson(cmd, (char *)payload);
            if (!err && cmd["type"] == "control") {
                uint8_t fan = cmd["fan"] | 0;
                if (fan <= 5) {
                    uart_sensor_send_control(fan);
                }
            }
            break;
        }
        default:
            break;
    }
}

// ==================== 构建传感器 JSON ====================
String _buildSensorJson() {
    JsonDocument doc;
    doc["type"] = "sensor";

    xSemaphoreTake(g_sensorMutex, portMAX_DELAY);
    doc["temp"]   = g_sensorData.temperature;
    doc["hum"]    = g_sensorData.humidity;
    doc["voc"]    = g_sensorData.voc;
    doc["pm25"]   = g_sensorData.pm25;
    doc["curr"]   = g_sensorData.current;
    doc["fan"]    = g_sensorData.fan_level;
    doc["state"]  = g_sensorData.state;
    doc["ai"]     = g_sensorData.ai_result;
    doc["ts"]     = g_sensorData.last_update;
    xSemaphoreGive(g_sensorMutex);

    String json;
    serializeJson(doc, json);
    return json;
}

// ==================== HTTP 静态页面 ====================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SmartHood Monitor</title>
<style>
body{font-family:Arial;margin:0;padding:10px;background:#1a1a2e;color:#eee}
h1{text-align:center;color:#e94560}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:800px;margin:0 auto}
.card{background:#16213e;border-radius:8px;padding:15px}
.val{font-size:2em;font-weight:bold;color:#0f3460}
.val span{color:#e94560}
img{width:100%;border-radius:8px}
.cam{grid-column:1/3}
</style>
</head>
<body>
<h1>SmartHood Monitor</h1>
<div class="grid">
<div class="card">温度<br><div class="val"><span id="t">--</span> &deg;C</div></div>
<div class="card">湿度<br><div class="val"><span id="h">--</span> %</div></div>
<div class="card">VOC<br><div class="val"><span id="v">--</span></div></div>
<div class="card">PM2.5<br><div class="val"><span id="p">--</span> ug/m3</div></div>
<div class="card cam">摄像头<br><img id="cam" src="" alt="waiting..."></div>
</div>
<script>
var ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onmessage=function(e){
try{var d=JSON.parse(e.data);
if(d.type=='sensor'){document.getElementById('t').innerText=d.temp.toFixed(1);
document.getElementById('h').innerText=d.hum.toFixed(1);
document.getElementById('v').innerText=d.voc;
document.getElementById('p').innerText=d.pm25;}
}catch(err){}}
</script>
</body>
</html>
)rawliteral";

void _handleRoot() {
    _http.send_P(200, "text/html", INDEX_HTML);
}

inline void web_server_begin() {
    _http.on("/", _handleRoot);
    _http.begin();
    Serial.printf("[HTTP] Web服务器启动, 端口 %d\n", HTTP_PORT);

    _ws.begin();
    _ws.onEvent(_wsEvent);
    Serial.printf("[WS] WebSocket服务器启动, 端口 %d\n", WS_PORT);
}

inline void web_server_update() {
    _ws.loop();
    _http.handleClient();

    uint32_t now = millis();

    // 每 200ms 推送传感器数据 (5Hz)
    if (now - _lastSensorPush >= 200) {
        _lastSensorPush = now;
        if (_ws.connectedClients() > 0 && g_sensorData.valid) {
            String json = _buildSensorJson();
            _ws.broadcastTXT(json);
        }
    }

    // 每 100ms 推送图像帧 (最多 10fps)
    if (now - _lastCamPush >= 100) {
        _lastCamPush = now;
        CameraFrame frame;
        if (g_frameBuffer.pop(frame)) {
            _ws.broadcastBIN(frame.data, frame.length);
        }
    }
}

#endif
