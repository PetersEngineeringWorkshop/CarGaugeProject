#pragma once

#include <cstdint>
#include "gauge_types.h"

namespace instrumentConfig {
inline constexpr bool showHondaSplash = true;
inline constexpr uint32_t rpmIdle = 900;
inline constexpr uint32_t rpmLimit = 6500;
static_assert(rpmIdle > 0 && rpmIdle < rpmLimit, "Idle RPM must be between zero and the RPM limit");
inline constexpr uint32_t canBitrate = 500000;
inline constexpr uint32_t canId = 1000;
inline constexpr bool canExtended = false;
#ifdef TOUCH_CAN_LISTEN_ONLY
inline constexpr bool canAck = false;
#else
inline constexpr bool canAck = true;
#endif
inline constexpr bool enableOil = false;
inline constexpr bool enableFuel = false;
inline constexpr bool enableLambda = false;
}

namespace gaugeConfig {
inline constexpr uint32_t staleMs = 1500;
inline constexpr bool temperatureF = true;
inline constexpr bool pressurePsi = true;
inline constexpr bool showAfr = false;
inline constexpr float stoichiometricAfr = 14.7f;
inline constexpr float coolantWarmC = 60;
inline constexpr float coolantHighC = 100;
inline constexpr float coolantOverheatC = 110;

constexpr float temperature(float c) { return c * 1.8f + 32; }
constexpr float pressure(float kpa) { return kpa / 6.894757f; }

struct Range { float minimum, maximum; };

constexpr Range scale(Gauge gauge) {
    switch (gauge) {
        case Gauge::Coolant: return {temperature(0), temperature(130)};
        case Gauge::Iat: return {temperature(0), temperature(80)};
        case Gauge::Rpm: return {0, 8000};
        case Gauge::Boost: return {pressure(-100), pressure(200)};
        case Gauge::Battery: return {8, 16};
        default: return {0, 1};
    }
}
}
