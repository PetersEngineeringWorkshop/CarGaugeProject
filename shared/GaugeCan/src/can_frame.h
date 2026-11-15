#pragma once
#include <cstdint>
struct CanFrame {
    uint32_t id=0;
    uint8_t dlc=0, data[8]{};
    bool extended=false, rtr=false;
};

struct CanFrameFilter {
    uint32_t id;
    bool extended;

    bool matches(const CanFrame &frame) const {
        return frame.id == id && frame.extended == extended && !frame.rtr;
    }
};
