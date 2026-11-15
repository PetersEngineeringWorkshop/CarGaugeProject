#include "display.h"
#include <SPI.h>
namespace {
constexpr auto &palette=colors::rgb565;
}
Display::Display() : IndexedFramebuffer(palette), tft(hardware::cs,hardware::dc,hardware::rst) {}
void Display::begin() {
    SPI.begin(hardware::sclk,-1,hardware::mosi,hardware::cs);
    tft.begin(hardware::spiHz);
    tft.setRotation(0);
    tft.fillScreen(palette[colors::Background]);
    invalidate();
}
void Display::flushBatch() {
    IndexedFramebuffer::flushBatch(8, tft);
}
