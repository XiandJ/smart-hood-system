#ifndef CONFIG_H
#define CONFIG_H

// ==================== WiFi 配置 ====================
#define WIFI_SSID       "SmartHood-AP"
#define WIFI_PASSWORD   "12345678"
#define WIFI_AP_SSID    "ESP32-SmartHood"
#define WIFI_AP_PASS    "smarthood123"

// ==================== UART 引脚 ====================
// UART1: OpenMV H7 图像数据
#define UART_CAM_RX     4
#define UART_CAM_TX     5
#define UART_CAM_BAUD   921600
#define UART_CAM_NUM    1

// UART2: STM32H5 传感器数据
#define UART_SENS_RX    16
#define UART_SENS_TX    17
#define UART_SENS_BAUD  115200
#define UART_SENS_NUM   2

// ==================== 帧协议 ====================
#define FRAME_HEADER_1  0xAA
#define FRAME_HEADER_2  0x55
#define FRAME_MAX_LEN   2048
#define FRAME_CAM_MAX   8192    // 图像帧最大 8KB (QVGA JPEG ~5-8KB)

// ==================== WebSocket ====================
#define WS_PORT         81
#define HTTP_PORT       80

// ==================== 任务栈大小 ====================
#define STACK_SENSOR    4096
#define STACK_CAMERA    8192
#define STACK_NETWORK   16384
#define STACK_WEB       4096

// ==================== 优先级 ====================
#define PRIO_SENSOR     3
#define PRIO_CAMERA     4
#define PRIO_NETWORK    2
#define PRIO_WEB        1

#endif
