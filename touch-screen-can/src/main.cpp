#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "can_receiver.h"
#include "demo_mode.h"
#include "diagnostics.h"
#include "gauge_selection.h"
#include "gauge_style.h"
#include "rgb565_transfer.h"
#include "touch_hardware.h"
#include "touch_input.h"

#include <cstring>

namespace {
constexpr uint16_t background = 0x0841;
constexpr uint16_t panel = 0x10a3;
constexpr uint16_t muted = 0x9cf3;
constexpr uint16_t white = 0xffff;
constexpr uint16_t cyan = 0x5e7f;
constexpr uint16_t orange = colors::rgb565[colors::EngineOrange];
constexpr uint16_t track = 0x2946;
constexpr int readingWidth = 144;
constexpr int readingHeight = 56;
constexpr int valueTop = 63;
constexpr int detailTop = valueTop + readingHeight;
constexpr int barTop = 195;
constexpr int barHeight = 18;
constexpr int columnWidth = hardware::width / 2;
constexpr Side sides[] = {Side::Left, Side::Right};
constexpr int menuTop = 38;
constexpr int menuRowHeight = 29;
constexpr int menuBottom = menuTop + gaugeCount * menuRowHeight;
constexpr int menuChoiceWidth = 82;
constexpr int menuLeftX = 148;
constexpr int menuRightX = menuLeftX + menuChoiceWidth;
constexpr int menuRightEdge = menuRightX + menuChoiceWidth;

int readingLeft(Side side) {
    return static_cast<int>(side) * columnWidth + (columnWidth - readingWidth) / 2;
}

const char *shortLabel(Gauge gauge) {
    switch (gauge) {
        case Gauge::Coolant: return "COOLANT";
        case Gauge::Rpm: return "RPM";
        case Gauge::Boost: return "BOOST";
        case Gauge::Battery: return "BATTERY";
        case Gauge::Iat: return "INTAKE AIR";
        default: return "";
    }
}

Adafruit_ILI9341 tft(hardware::tftCs, hardware::dc, hardware::reset);
XPT2046_Touchscreen touch(hardware::touchCs);
GFXcanvas16 readingCanvas(readingWidth, readingHeight);
Preferences settings;
LinkGenericDashDecoder decoder({
    {instrumentConfig::canId, instrumentConfig::canExtended},
    instrumentConfig::enableOil, instrumentConfig::enableFuel, instrumentConfig::enableLambda
});
CanReceiver can({hardware::canTx, hardware::canRx, instrumentConfig::canBitrate,
                 instrumentConfig::canAck}, decoder.stream(), logLine);
DemoMode dataMode;
GaugeSelection gauges;
TouchCalibration calibration;
TouchCalibration candidate;
TouchTap taps;

enum class Page { Splash, Gauge, Menu, Calibrate };
Page page = Page::Calibrate;
constexpr uint32_t splashHoldMs = 850;
constexpr uint32_t splashStepMs = 30;
constexpr uint8_t splashSteps = 16;
uint32_t splashStarted = 0;
uint8_t splashStep = 0;
unsigned calibrationStep = 0;
bool settingsOpen = false;
bool settingsDirty = false;
bool touchReady = false;
uint32_t settingsAt = 0;
uint32_t settingsDelay = 500;
uint32_t polledAt = 0;
uint32_t refreshedAt = 0;
uint32_t reportedAt = 0;
struct ReadingPaint {
    char value[sizeof(EngineReading::text)]{};
    const char *units = "";
    const char *status = "";
    uint16_t valueColor = background;
    uint16_t statusColor = background;
    int bar = -1;
    uint16_t accent = background;
};
ReadingPaint painted[2];
uint32_t tapCount = 0;

void centerText(Adafruit_GFX &display, const char *text, int16_t x, int16_t y,
                uint8_t size, uint16_t color) {
    display.setTextSize(size);
    display.setTextColor(color);
    display.setCursor(x - int16_t(strlen(text) * 6 * size / 2), y);
    display.print(text);
}

void centerText(const char *text, int16_t x, int16_t y, uint8_t size, uint16_t color) {
    centerText(tft, text, x, y, size, color);
}

void centerSans(Adafruit_GFX &display, const GFXfont &font, const char *text,
                int16_t center, int16_t baseline, uint16_t color, bool degree = false) {
    if (!*text) return;
    display.setFont(&font);
    display.setTextSize(1);
    display.setTextColor(color);
    int16_t left, top;
    uint16_t width, height;
    display.getTextBounds(text, 0, 0, &left, &top, &width, &height);
    constexpr int degreeWidth = 7;
    const int16_t x = center - (width + (degree ? degreeWidth : 0)) / 2;
    // The bundled FreeSans fonts cover ASCII, so draw the degree mark separately.
    if (degree) display.drawCircle(x + 2, baseline + top + 2, 2, color);
    display.setCursor(x + (degree ? degreeWidth : 0) - left, baseline);
    display.print(text);
    display.setFont(nullptr);
}

void pushReading(int x, int y) {
    // Send the finished pixels together so a live update never paints a blank field first.
    writeRgb565Rect(tft, x, y, readingWidth, readingHeight, readingCanvas.getBuffer());
}

void drawValue(const char *value, uint16_t color) {
    readingCanvas.fillScreen(background);
    readingCanvas.setTextSize(1);
    readingCanvas.setTextColor(color);
    readingCanvas.setFont(&FreeSansBold24pt7b);

    int16_t left, top;
    uint16_t width, height;
    readingCanvas.getTextBounds(value, 0, 0, &left, &top, &width, &height);
    if (width > readingWidth - 4) {
        readingCanvas.setFont(&FreeSansBold18pt7b);
        readingCanvas.getTextBounds(value, 0, 0, &left, &top, &width, &height);
    }
    const int16_t x = (readingWidth - int(width)) / 2 - left;
    // Center on the height of a digit so the baseline also stays put for "--".
    readingCanvas.getTextBounds("0", 0, 0, &left, &top, &width, &height);
    const int16_t baseline = (readingHeight - int(height)) / 2 - top;
    readingCanvas.setCursor(x, baseline);
    readingCanvas.print(value);
    readingCanvas.setFont(nullptr);
}

void button(int x, int w, const char *label, uint16_t color = white) {
    tft.fillRoundRect(x, 192, w, 40, 6, panel);
    centerText(label, x + w / 2, 204, 2, color);
}

void clearScreen() {
    // Bound the time between CAN queue drains during a full-screen redraw.
    for (int y = 0; y < hardware::height; y += 40) {
        tft.fillRect(0, y, hardware::width, 40, background);
        can.service(decoder, dataMode.live);
    }
}

constexpr uint16_t blendColor(uint16_t from, uint16_t to, float fraction) {
    const auto channel = [=](unsigned shift, unsigned mask) {
        const int start = (from >> shift) & mask;
        const int end = (to >> shift) & mask;
        return unsigned(start + (end - start) * fraction + 0.5f) << shift;
    };
    return channel(11, 31) | channel(5, 63) | channel(0, 31);
}

constexpr uint16_t rpmBarColor(float rpm) {
    constexpr auto blue = colors::rgb565[colors::EngineBlue];
    constexpr auto green = colors::rgb565[colors::EngineGreen];
    constexpr auto yellow = colors::rgb565[colors::Yellow];
    constexpr auto red = colors::rgb565[colors::EngineRed];
    if (rpm <= 0) return blue;
    if (rpm >= instrumentConfig::rpmLimit) return red;
    if (rpm <= instrumentConfig::rpmIdle) {
        return blendColor(blue, green, rpm / instrumentConfig::rpmIdle);
    }
    const float load = (rpm - instrumentConfig::rpmIdle) /
                       (instrumentConfig::rpmLimit - instrumentConfig::rpmIdle);
    if (load <= 0.5f) return blendColor(green, yellow, load * 2);
    return blendColor(yellow, red, (load - 0.5f) * 2);
}

void updateReading(Side side, uint32_t now, bool force) {
    const Gauge gauge = gauges.selected(side);
    const auto reading = engineReading(gauge, dataMode.readings(), now);
    auto &previous = painted[static_cast<unsigned>(side)];
    const int x = readingLeft(side);
    const bool valid = reading.state == engine::State::Valid;
    const auto style = gaugeStyle::forReading(gauge, reading);
    const bool rpmWarning = valid && gauge == Gauge::Rpm &&
        reading.value >= instrumentConfig::rpmLimit;
    uint16_t accent = muted;
    if (valid) accent = gauge == Gauge::Rpm ? rpmBarColor(reading.value) : colors::rgb565[style.accent];
    const uint16_t valueColor = valid ? white : muted;
    const char *value = valid ? reading.text : "--";
    const char *units = valid ? reading.units : "";
    const bool coolantWarning = valid && gauge == Gauge::Coolant &&
        (style.accent == colors::EngineOrange || style.accent == colors::EngineRed);
    const uint16_t statusColor = (coolantWarning || rpmWarning) ? accent : muted;
    const char *status = "";
    if (coolantWarning) status = style.status;
    if (rpmWarning) status = "LIMIT";
    if (!valid) {
        status = "WAITING FOR ECU";
        if (!can.ready()) status = "CAN OFFLINE";
        else if (reading.state == engine::State::Stale) status = "STALE DATA";
        else if (reading.state == engine::State::Invalid) status = "INVALID DATA";
    }

    if (force || strcmp(previous.value, value) || previous.valueColor != valueColor) {
        drawValue(value, valueColor);
        pushReading(x, valueTop);
        strcpy(previous.value, value);
        previous.valueColor = valueColor;
    }

    if (force || strcmp(previous.units, units) || strcmp(previous.status, status) ||
        previous.statusColor != statusColor) {
        readingCanvas.fillScreen(background);
        centerSans(readingCanvas, FreeSans9pt7b, units, readingWidth / 2, 14, white,
                   gauge == Gauge::Coolant || gauge == Gauge::Iat);
        if (!valid && can.ready() && reading.state == engine::State::Never) {
            centerSans(readingCanvas, FreeSansBold9pt7b, "WAITING FOR",
                       readingWidth / 2, 32, muted);
            centerSans(readingCanvas, FreeSansBold9pt7b, "ECU",
                       readingWidth / 2, 48, muted);
        } else {
            centerSans(readingCanvas, FreeSansBold9pt7b, status, readingWidth / 2, 43, statusColor);
        }
        pushReading(x, detailTop);
        previous.units = units;
        previous.status = status;
        previous.statusColor = statusColor;
    }
    int bar = valid ? int(gaugeStyle::position(style, reading.value) * readingWidth) : 0;
    if (valid && gauge == Gauge::Rpm && bar == 0) bar = 2;
    if (force || accent != previous.accent) {
        if (bar) tft.fillRect(x, barTop, bar, barHeight, accent);
        if (bar < readingWidth) {
            tft.fillRect(x + bar, barTop, readingWidth - bar, barHeight, track);
        }
    } else if (bar > previous.bar) {
        tft.fillRect(x + previous.bar, barTop, bar - previous.bar, barHeight, accent);
    } else if (bar < previous.bar) {
        tft.fillRect(x + bar, barTop, previous.bar - bar, barHeight, track);
    }

    previous.bar = bar;
    previous.accent = accent;
}

void updateReadings(bool force = false) {
    const uint32_t now = millis();
    for (Side side : sides) updateReading(side, now, force);
}

void drawDataSource() {
    tft.fillRect(112, 224, 96, 8, background);
    if (dataMode.enabled()) centerText("DEMO / SIMULATED", 160, 224, 1, orange);
}

void showGauge() {
    page = Page::Gauge;
    clearScreen();
    tft.drawFastVLine(columnWidth, 20, barTop + barHeight - 20, track);
    for (Side side : sides) {
        const int center = readingLeft(side) + readingWidth / 2;
        centerSans(tft, FreeSansBold9pt7b, shortLabel(gauges.selected(side)),
                   center, 45, cyan);
    }
    updateReadings(true);
    drawDataSource();
}

void drawMenuChoice(int x, int y, bool selected) {
    tft.fillRoundRect(x + 3, y + 2, menuChoiceWidth - 6, 23, 4, selected ? 0x21ce : track);
    centerText(selected ? "X" : "-", x + menuChoiceWidth / 2, y + 6, 2, selected ? cyan : muted);
}

void drawMenuRow(unsigned i) {
    const int y = menuTop + menuRowHeight * i;
    const auto gauge = gaugeOrder[i];
    tft.fillRoundRect(8, y, 304, 27, 4, panel);
    tft.setTextSize(2);
    tft.setTextColor(white);
    tft.setCursor(16, y + 6);
    tft.print(gauge == Gauge::Iat ? "IAT" : shortLabel(gauge));
    drawMenuChoice(menuLeftX, y, gauge == gauges.selected(Side::Left));
    drawMenuChoice(menuRightX, y, gauge == gauges.selected(Side::Right));
}

void drawMenuRows() {
    for (unsigned i = 0; i < gaugeCount; ++i) {
        drawMenuRow(i);
        can.service(decoder, dataMode.live);
    }
}

void drawDemoButton() {
    button(132, 100, dataMode.enabled() ? "DEMO ON" : "DEMO OFF",
           dataMode.enabled() ? orange : white);
}

void showMenu() {
    page = Page::Menu;
    clearScreen();
    centerText("GAUGE", 56, 14, 2, cyan);
    centerText("LEFT", menuLeftX + menuChoiceWidth / 2, 14, 2, cyan);
    centerText("RIGHT", menuRightX + menuChoiceWidth / 2, 14, 2, cyan);
    drawMenuRows();
    button(8, 118, "CALIBRATE");
    drawDemoButton();
    button(238, 74, "DONE");
}

const char *dataSource() {
    return dataMode.enabled() ? "DEMO" : "LIVE";
}

void toggleDemo() {
    dataMode.toggle(millis());
    if (page == Page::Gauge) {
        drawDataSource();
        updateReadings();
    } else if (page == Page::Menu) {
        drawDemoButton();
    }
    logLine("DATA MODE source=%s\n", dataSource());
}

void drawCalibration(bool retry = false) {
    page = Page::Calibrate;
    clearScreen();
    centerText("TOUCH CALIBRATION", 160, 70, 2, cyan);
    centerText("Tap and release the cross", 160, 104, 2, white);
    char instruction[24];
    snprintf(instruction, sizeof(instruction), "Target %u of 3", calibrationStep + 1);
    centerText(instruction, 160, 136, 2, muted);
    if (retry) centerText("Try again with a stylus", 160, 164, 1, muted);
    const auto p = touchTargets[calibrationStep];
    tft.drawCircle(p.x, p.y, 10, cyan);
    tft.drawFastHLine(p.x - 14, p.y, 29, white);
    tft.drawFastVLine(p.x, p.y - 14, 29, white);
}

void calibrate() {
    calibrationStep = 0;
    candidate = {};
    drawCalibration();
}

void showInitialPage() {
    if (calibration.valid()) showGauge();
    else calibrate();
}

void drawHondaMark(uint16_t color) {
    constexpr int x = (hardware::width - 114) / 2;
    constexpr int y = (hardware::height - 94) / 2;
    for (int i = 0; i < 5; ++i) {
        tft.drawRoundRect(x + i, y + i, 114 - 2 * i, 94 - 2 * i, 20 - i, color);
    }
    // Flared uprights and a wide crossbar, based on the CR-V-era badge.
    tft.fillTriangle(x + 24, y + 15, x + 43, y + 14, x + 47, y + 79, color);
    tft.fillTriangle(x + 24, y + 15, x + 47, y + 79, x + 34, y + 79, color);
    tft.fillTriangle(x + 90, y + 15, x + 71, y + 14, x + 67, y + 79, color);
    tft.fillTriangle(x + 90, y + 15, x + 67, y + 79, x + 80, y + 79, color);
    tft.fillRoundRect(x + 37, y + 42, 40, 14, 4, color);
}

void startSplash() {
    page = Page::Splash;
    clearScreen();
    drawHondaMark(white);
    splashStarted = millis();
}

void serviceSplash(uint32_t now) {
    const uint32_t elapsed = now - splashStarted;
    if (elapsed < splashHoldMs) return;
    const uint32_t step = (elapsed - splashHoldMs) / splashStepMs;
    if (step >= splashSteps) {
        showInitialPage();
        return;
    }
    if (step == splashStep) return;
    splashStep = step;
    const unsigned remaining = splashSteps - splashStep;
    const auto fade = [remaining](unsigned start, unsigned end) {
        return end + (start - end) * remaining / splashSteps;
    };
    const auto color = uint16_t((fade(31, background >> 11) << 11) |
        (fade(63, (background >> 5) & 63) << 5) | fade(31, background & 31));
    drawHondaMark(color);
}

void settingsChanged() {
    settingsDirty = true;
    settingsAt = millis();
    settingsDelay = 500;
}

void handleTap(Point raw) {
    ++tapCount;
    if (page == Page::Calibrate) {
        candidate.raw[calibrationStep++] = raw;
        logLine("TOUCH calibration target=%u raw_x=%d raw_y=%d\n", calibrationStep, raw.x, raw.y);
        if (calibrationStep < 3) {
            drawCalibration();
            return;
        }
        if (!candidate.valid()) {
            calibrationStep = 0;
            drawCalibration(true);
            return;
        }
        calibration = candidate;
        const bool saved = settingsOpen &&
            settings.putBytes("calibration-v1", &calibration, sizeof(calibration)) == sizeof(calibration);
        logLine("TOUCH calibrated saved=%d\n", saved);
        showGauge();
        return;
    }

    if (page == Page::Gauge) {
        showMenu();
        return;
    }

    const auto p = calibration.screen(raw);
    logLine("TOUCH x=%d y=%d\n", p.x, p.y);
    if (p.x >= menuLeftX && p.x < menuRightEdge && p.y >= menuTop && p.y < menuBottom) {
        const auto gauge = gaugeOrder[(p.y - menuTop) / menuRowHeight];
        if (gauges.select(p.x < menuRightX ? Side::Left : Side::Right, gauge)) {
            settingsChanged();
            drawMenuRows();
        }
        return;
    }

    if (p.y < 192 || p.y >= 232) return;
    if (p.x >= 8 && p.x < 126) calibrate();
    if (p.x >= 132 && p.x < 232) toggleDemo();
    if (p.x >= 238 && p.x < 312) showGauge();
}
}

void setup() {
    beginDiagnostics();
    readingCanvas.setTextWrap(false);
    logLine("Touch Screen CAN v%s\n", GAUGE_FIRMWARE_VERSION);
    settingsOpen = settings.begin("touch-can", false);
    if (settingsOpen) {
        gauges.restore(settings.getUInt("gauges-v1", gauges.packed()));
        if (settings.getBytesLength("calibration-v1") == sizeof(calibration)) {
            settings.getBytes("calibration-v1", &calibration, sizeof(calibration));
        }
    }
    pinMode(hardware::tftCs, OUTPUT);
    digitalWrite(hardware::tftCs, HIGH);
    pinMode(hardware::touchCs, OUTPUT);
    digitalWrite(hardware::touchCs, HIGH);
    touchReady = SPI.begin(hardware::sck, hardware::miso, hardware::mosi);
    if (touchReady) {
        tft.begin(hardware::spiHz);
        tft.setRotation(hardware::rotation);
        tft.setTextWrap(false);
        touchReady = touch.begin();
        touch.setRotation(hardware::rotation);
    }
    if (!touchReady) logLine("UI SPI initialization failed\n");
    if (!readingCanvas.getBuffer()) logLine("UI reading buffer allocation failed\n");
    can.begin();
    if (touchReady && readingCanvas.getBuffer()) {
        if constexpr (instrumentConfig::showHondaSplash) startSplash();
        else showInitialPage();
    }
}

void loop() {
    can.service(decoder, dataMode.live);
    const uint32_t now = millis();
    dataMode.service(now);
    if (page == Page::Splash) {
        serviceSplash(now);
    } else if (touchReady && readingCanvas.getBuffer() && now - polledAt >= 12) {
        polledAt = now;
        const auto sample = touch.getPoint();
        Point tap;
        if (taps.sample(sample.z > 0, {sample.x, sample.y}, tap)) handleTap(tap);
    }
    if (touchReady && page == Page::Gauge && now - refreshedAt >= 50) {
        refreshedAt = now;
        updateReadings();
    }
    if (settingsOpen && settingsDirty && millis() - settingsAt >= settingsDelay) {
        settingsAt = millis();
        settingsDirty = settings.putUInt("gauges-v1", gauges.packed()) != sizeof(uint32_t);
        logLine("UI gauge settings saved=%d\n", !settingsDirty);
        settingsDelay = 5000;
    }
    if (Serial.available()) {
        const int command = Serial.read();
        if (command == 'j') can.listIds();
        if (command == 'c' && touchReady && readingCanvas.getBuffer()) calibrate();
        if (command == 'd') toggleDemo();
    }
    if (now - reportedAt >= 1000) {
        reportedAt = now;
        can.report();
        logLine("UI version=%s layout=%dx%d spi_ready=%d page=%s calibrated=%d taps=%lu nvs=%d heap=%lu decoded=%lu\n",
                GAUGE_FIRMWARE_VERSION, hardware::width, hardware::height, touchReady,
                page == Page::Gauge ? "gauge" : page == Page::Menu ? "menu" :
                page == Page::Splash ? "splash" : "calibrate",
                calibration.valid(), (unsigned long)tapCount, settingsOpen,
                (unsigned long)ESP.getFreeHeap(), (unsigned long)decoder.matched);
        if (page == Page::Gauge) {
            for (Side side : sides) {
                const auto &reading = painted[static_cast<unsigned>(side)];
                logLine("GAUGE source=%s side=%s gauge=%s value=%s units=%s status=%s\n",
                        dataSource(), side == Side::Left ? "LEFT" : "RIGHT",
                        gaugeConfig::name(gauges.selected(side)), reading.value,
                        reading.units, reading.status);
            }
        }
    }
    delay(1);
}
