/*
 * ESP32-SmartHood Bridge
 * 功能: 接收 STM32H5 传感器数据 + OpenMV 图像帧
 *       通过 WiFi WebSocket 转发给上位机
 *
 * 硬件: ESP32-WROOM-32E
 * UART1 (GPIO4/5):  接收 OpenMV H7 JPEG 图像帧
 * UART2 (GPIO16/17): 接收 STM32H5 传感器数据
 * WiFi: STA模式(优先) -> AP模式(自动降级)
 * WebSocket: 端口 81, 推送 JSON + 二进制图像
 */

#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "uart_sensor.h"
#include "uart_camera.h"
#include "web_server.h"

// ==================== FreeRTOS 任务句柄 ====================
TaskHandle_t _taskSensor  = NULL;
TaskHandle_t _taskCamera  = NULL;
TaskHandle_t _taskNetwork = NULL;

// ==================== 传感器采集任务 (Core 0) ====================
void sensorTask(void *pv) {
    Serial.println("[TASK] 传感器任务启动");
    for (;;) {
        uart_sensor_update();
        vTaskDelay(pdMS_TO_TICKS(5));  // 200Hz 轮询
    }
}

// ==================== 摄像头采集任务 (Core 0) ====================
void cameraTask(void *pv) {
    Serial.println("[TASK] 摄像头任务启动");
    for (;;) {
        uart_camera_update();
        vTaskDelay(pdMS_TO_TICKS(2));  // 500Hz 轮询
    }
}

// ==================== 网络推送任务 (Core 1) ====================
void networkTask(void *pv) {
    Serial.println("[TASK] 网络任务启动");
    for (;;) {
        g_wifiManager.update();
        web_server_update();
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz
    }
}

// ==================== 系统状态串口打印 (调试用) ====================
uint32_t _lastStatusPrint = 0;

void printStatus() {
    uint32_t now = millis();
    if (now - _lastStatusPrint < 3000) return;
    _lastStatusPrint = now;

    Serial.printf("[STATUS] WiFi:%s IP:%s 传感器:%s 帧缓冲:%d\n",
        g_wifiManager.isConnected() ? "OK" : "DISCONNECTED",
        g_wifiManager.getIP().toString().c_str(),
        g_sensorData.valid ? "VALID" : "NO DATA",
        g_frameBuffer.available() ? 1 : 0
    );

    if (g_sensorData.valid) {
        xSemaphoreTake(g_sensorMutex, portMAX_DELAY);
        Serial.printf("  温度=%.1f°C 湿度=%.1f%% VOC=%d PM2.5=%d 电流=%.2fA 风机=%d档\n",
            g_sensorData.temperature, g_sensorData.humidity,
            g_sensorData.voc, g_sensorData.pm25,
            g_sensorData.current, g_sensorData.fan_level
        );
        xSemaphoreGive(g_sensorMutex);
    }
}

// ==================== setup ====================
void setup() {
    // 调试串口
    Serial.begin(115200);
    Serial.println("\n========== ESP32 SmartHood Bridge ==========");
    Serial.printf("固件版本: 1.0.0\n");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // 初始化硬件串口
    uart_sensor_begin();
    uart_camera_begin();

    // 初始化 WiFi (先尝试 STA, 失败自动切换 AP)
    g_wifiManager.begin(WIFI_MODE_STA);

    // 启动 WebSocket + HTTP
    web_server_begin();

    // 创建 FreeRTOS 任务
    // Core 0: 数据采集 (传感器 + 摄像头)
    xTaskCreatePinnedToCore(sensorTask, "sensor", STACK_SENSOR,  NULL, PRIO_SENSOR,  &_taskSensor,  0);
    xTaskCreatePinnedToCore(cameraTask, "camera", STACK_CAMERA,  NULL, PRIO_CAMERA,  &_taskCamera,  0);
    // Core 1: 网络通信
    xTaskCreatePinnedToCore(networkTask, "network", STACK_NETWORK, NULL, PRIO_NETWORK, &_taskNetwork, 1);

    Serial.println("========== 系统初始化完成 ==========\n");
}

// ==================== loop ====================
// Arduino loop 在 Core 1 上运行，用于低优先级任务
void loop() {
    printStatus();
    delay(100);
}
