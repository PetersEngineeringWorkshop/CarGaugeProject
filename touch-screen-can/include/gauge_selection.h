#pragma once

#include "gauge_types.h"

inline constexpr Gauge gaugeOrder[] = {
    Gauge::Coolant, Gauge::Rpm, Gauge::Boost, Gauge::Battery, Gauge::Iat
};
inline constexpr unsigned gaugeCount = sizeof(gaugeOrder) / sizeof(gaugeOrder[0]);

enum class Side : uint8_t { Left, Right };

class GaugeSelection {
public:
    Gauge selected(Side side) const { return slots[static_cast<unsigned>(side)]; }

    uint32_t packed() const {
        // Keep the existing slot bytes. The low 16 bits previously held the enabled mask.
        return (uint32_t(selected(Side::Left)) << 16) |
               (uint32_t(selected(Side::Right)) << 24);
    }

    void restore(uint32_t stored) {
        const auto left = static_cast<Gauge>((stored >> 16) & 0xff);
        const auto right = static_cast<Gauge>(stored >> 24);
        slots[0] = available(left) ? left : Gauge::Coolant;
        // The original single-gauge settings have no right slot.
        const auto fallback = slots[0] == Gauge::Rpm ? Gauge::Coolant : Gauge::Rpm;
        slots[1] = available(right) ? right : fallback;
    }

    bool select(Side side, Gauge gauge) {
        auto &slot = slots[static_cast<unsigned>(side)];
        if (!available(gauge) || slot == gauge) return false;
        slot = gauge;
        return true;
    }

private:
    static bool available(Gauge candidate) {
        for (auto gauge : gaugeOrder) if (gauge == candidate) return true;
        return false;
    }

    Gauge slots[2] = {Gauge::Coolant, Gauge::Rpm};
};
