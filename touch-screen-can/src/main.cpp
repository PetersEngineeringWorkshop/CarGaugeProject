#include <Adafruit_ILI9341.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "can_receiver.h"
#include "demo_mode.h"
#include "diagnostics.h"
#include "gauge_selection.h"
#include "gauge_style.h"
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
constexpr int detailTop = 127;
constexpr int barTop = 195;
constexpr int barHeight = 14;
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
CanReceiver can;
LinkGenericDashDecoder decoder;
DemoMode dataMode;
GaugeSelection gauges;
TouchCalibration calibration;
TouchCalibration candidate;
TouchTap taps;

enum class Page { Gauge, Menu, Calibrate };
Page page = Page::Calibrate;
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

void pushReading(int x, int y) {
    // Send the finished pixels together so a live update never paints a blank field first.
    tft.startWrite();
    tft.setAddrWindow(x, y, readingWidth, readingHeight);
    tft.writePixels(readingCanvas.getBuffer(), readingWidth * readingHeight);
    tft.endWrite();
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

void updateReading(Side side, uint32_t now, bool force) {
    const Gauge gauge = gauges.selected(side);
    const auto reading = engineReading(gauge, dataMode.readings(), now);
    auto &previous = painted[static_cast<unsigned>(side)];
    const int x = readingLeft(side);
    const bool valid = reading.state == engine::State::Valid;
    const auto style = gaugeStyle::forReading(gauge, reading);
    const uint16_t accent = valid ? colors::rgb565[style.accent] : muted;
    const uint16_t valueColor = valid ? white : muted;
    const char *value = valid ? reading.text : "--";
    const char *units = valid ? reading.units : "";
    const char *status = style.status;
    if (!valid) {
        status = "WAITING FOR ECU";
        if (!can.ready()) status = "CAN OFFLINE";
        else if (reading.state == engine::State::Stale) status = "STALE DATA";
        else if (reading.state == engine::State::Invalid) status = "INVALID DATA";
    }

    if (force || strcmp(previous.value, value) || previous.valueColor != valueColor) {
        readingCanvas.fillScreen(background);
        const auto size = strlen(value) <= 4 ? 6 : 4;
        centerText(readingCanvas, value, readingWidth / 2, (readingHeight - 8 * size) / 2,
                   size, valueColor);
        pushReading(x, valueTop);
        strcpy(previous.value, value);
        previous.valueColor = valueColor;
    }

    if (force || strcmp(previous.units, units) || strcmp(previous.status, status) ||
        previous.accent != accent) {
        readingCanvas.fillScreen(background);
        if (valid) {
            centerText(readingCanvas, units, readingWidth / 2, 6, 2, accent);
            centerText(readingCanvas, status, readingWidth / 2, 34, 1, muted);
        } else {
            centerText(readingCanvas, status, readingWidth / 2, 24, 1, muted);
        }
        pushReading(x, detailTop);
        previous.units = units;
        previous.status = status;
    }
    const int bar = valid ? int(gaugeStyle::position(style, reading.value) * readingWidth) : 0;
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
        centerText(shortLabel(gauges.selected(side)), center, 28, 2, cyan);
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
    logLine("Touch Screen CAN v%s\n", TOUCH_CAN_VERSION);
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
        if (calibration.valid()) showGauge();
        else calibrate();
    }
}

void loop() {
    can.service(decoder, dataMode.live);
    const uint32_t now = millis();
    dataMode.service(now);
    if (touchReady && readingCanvas.getBuffer() && now - polledAt >= 12) {
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
                TOUCH_CAN_VERSION, hardware::width, hardware::height, touchReady,
                page == Page::Gauge ? "gauge" : page == Page::Menu ? "menu" : "calibrate",
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
