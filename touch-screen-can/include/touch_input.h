#pragma once

#include <cstdint>

struct Point {
    int16_t x;
    int16_t y;
};

inline constexpr Point touchTargets[] = {{24, 24}, {295, 24}, {24, 215}};

struct TouchCalibration {
    Point raw[3]{};

    int32_t determinant() const {
        return int32_t(raw[1].x - raw[0].x) * (raw[2].y - raw[0].y)
             - int32_t(raw[1].y - raw[0].y) * (raw[2].x - raw[0].x);
    }

    bool valid() const {
        for (Point p : raw) {
            if (p.x < 0 || p.x > 4095 || p.y < 0 || p.y > 4095) return false;
        }
        const auto area = determinant();
        return area > 500000 || area < -500000;
    }

    Point screen(Point p) const {
        const int32_t dx = p.x - raw[0].x;
        const int32_t dy = p.y - raw[0].y;
        const int32_t area = determinant();
        const int32_t x = touchTargets[0].x + int64_t(touchTargets[1].x - touchTargets[0].x)
            * (dx * (raw[2].y - raw[0].y) - dy * (raw[2].x - raw[0].x)) / area;
        const int32_t y = touchTargets[0].y + int64_t(touchTargets[2].y - touchTargets[0].y)
            * (dy * (raw[1].x - raw[0].x) - dx * (raw[1].y - raw[0].y)) / area;
        return {static_cast<int16_t>(x), static_cast<int16_t>(y)};
    }
};

// Emit one point on release. A few stable samples reject pressure glitches.
class TouchTap {
public:
    bool sample(bool pressed, Point point, Point &tap) {
        if (pressed) {
            releaseSamples = 0;
            if (!downSamples) origin = point;
            if (downSamples < 3) ++downSamples;
            const int dx = point.x - origin.x;
            const int dy = point.y - origin.y;
            if (dx < -250 || dx > 250 || dy < -250 || dy > 250) moved = true;
            return false;
        }
        if (!downSamples || ++releaseSamples < 3) return false;
        const bool tapped = downSamples == 3 && !moved;
        downSamples = releaseSamples = 0;
        moved = false;
        tap = origin;
        return tapped;
    }

private:
    Point origin{};
    uint8_t downSamples = 0;
    uint8_t releaseSamples = 0;
    bool moved = false;
};
