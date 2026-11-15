#pragma once
#include <Adafruit_GFX.h>
#include "compass_state.h"
class CompassGauge {
public:
    explicit CompassGauge(GFXcanvas8 &surface) : canvas(surface) {}
    bool draw(CompassState state, bool force=false);
private:
    GFXcanvas8 &canvas;
    bool hasState=false;
    CompassState lastState{};
    void centered(const char *text, int y, uint8_t size, uint8_t color);
};
