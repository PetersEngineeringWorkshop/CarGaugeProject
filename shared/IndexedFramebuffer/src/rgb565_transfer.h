#pragma once

#include <stdint.h>

// The driver completes the transfer before the caller reuses the pixel buffer.
template <typename Driver>
void writeRgb565Rect(Driver &panel, int x, int y, int width, int height,
                     uint16_t *pixels) {
    panel.startWrite();
    panel.setAddrWindow(x, y, width, height);
    panel.writePixels(pixels, width * height, true);
    panel.endWrite();
}
