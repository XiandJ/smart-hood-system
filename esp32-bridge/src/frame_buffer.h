#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <Arduino.h>
#include "config.h"

// 线程安全的图像帧环形缓冲区
// 生产者: camera_task (Core 0)
// 消费者: network_task (Core 1)

#define FRAME_RING_SIZE  4   // 环形缓冲区帧数

struct CameraFrame {
    uint8_t  data[FRAME_CAM_MAX];
    uint16_t length;
    uint32_t timestamp;
};

class FrameBuffer {
public:
    FrameBuffer() : _head(0), _tail(0), _count(0) {
        _mutex = xSemaphoreCreateMutex();
    }

    // 写入一帧图像 (生产者调用)
    bool push(const uint8_t *data, uint16_t len) {
        if (len == 0 || len > FRAME_CAM_MAX) return false;

        xSemaphoreTake(_mutex, portMAX_DELAY);

        // 缓冲区满则丢弃最旧的一帧
        if (_count >= FRAME_RING_SIZE) {
            _tail = (_tail + 1) % FRAME_RING_SIZE;
            _count--;
        }

        memcpy(_frames[_head].data, data, len);
        _frames[_head].length = len;
        _frames[_head].timestamp = millis();
        _head = (_head + 1) % FRAME_RING_SIZE;
        _count++;

        xSemaphoreGive(_mutex);
        return true;
    }

    // 读取一帧图像 (消费者调用)
    bool pop(CameraFrame &out) {
        xSemaphoreTake(_mutex, portMAX_DELAY);

        if (_count == 0) {
            xSemaphoreGive(_mutex);
            return false;
        }

        memcpy(&out, &_frames[_tail], sizeof(CameraFrame));
        _tail = (_tail + 1) % FRAME_RING_SIZE;
        _count--;

        xSemaphoreGive(_mutex);
        return true;
    }

    bool available() { return _count > 0; }

private:
    CameraFrame  _frames[FRAME_RING_SIZE];
    uint8_t      _head;
    uint8_t      _tail;
    uint8_t      _count;
    SemaphoreHandle_t _mutex;
};

// 全局帧缓冲区实例
extern FrameBuffer g_frameBuffer;

#endif
