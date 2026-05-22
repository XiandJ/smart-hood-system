#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <Arduino.h>
#include "config.h"

#define FRAME_RING_SIZE  2

struct CameraFrame {
    uint8_t  data[FRAME_CAM_MAX];
    uint16_t length;
    uint32_t timestamp;
};

class FrameBuffer {
public:
    FrameBuffer();
    bool push(const uint8_t *data, uint16_t len);
    bool pop(CameraFrame &out);
    bool available();
private:
    CameraFrame  _frames[FRAME_RING_SIZE];
    uint8_t      _head;
    uint8_t      _tail;
    uint8_t      _count;
    SemaphoreHandle_t _mutex;
};

extern FrameBuffer g_frameBuffer;

#endif
