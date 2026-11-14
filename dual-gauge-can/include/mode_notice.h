#pragma once
#include <Adafruit_GFX.h>
#include "gauge_palette.h"
#include <cstring>

inline void drawModeNotice(GFXcanvas8 &canvas,bool demo,bool left) {
    const char *word=demo?(left?"DEMO":"MODE"):(left?"LIVE":"DATA");
    canvas.fillScreen(colors::Black);canvas.setTextWrap(false);
    canvas.drawCircle(120,120,110,demo?colors::EngineOrange:colors::EngineGreen);
    canvas.setTextColor(demo?colors::EngineOrange:colors::EngineGreen);
    canvas.setTextSize(5);canvas.setCursor(120-int(std::strlen(word))*15,101);
    canvas.print(word);
}
