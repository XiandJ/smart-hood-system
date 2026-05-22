#include "frame_buffer.h"
#include <string.h>

FrameBuffer g_frameBuffer;

FrameBuffer::FrameBuffer() : _head(0), _tail(0), _count(0) {
    _mutex = xSemaphoreCreateMutex();
}

bool FrameBuffer::push(const uint8_t *data, uint16_t len) {
    if (len == 0 || len > FRAME_CAM_MAX) return false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
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

bool FrameBuffer::pop(CameraFrame &out) {
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

bool FrameBuffer::available() { return _count > 0; }
