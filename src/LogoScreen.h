// =============================================================
//  screens/LogoScreen.h – logo obrazovka
//
//  Zatím placeholder – černá obrazovka s názvem.
//  TODO: načíst PNG z LittleFS a zobrazit přes tft.drawPng()
//
//  Použití:
//    LogoScreen::draw(theme, version);
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"

namespace LogoScreen {

    void draw(const Theme* t, const char* version) {
        tft.fillScreen(t->bg);

        // TODO: tft.drawPng("/logo.png", 0, 0, 320, 240);

        // Placeholder – název a verze uprostřed
        tft.setTextDatum(middle_center);
        tft.setFont(&fonts::FreeSansBold18pt7b);
        tft.setTextColor(t->accent);
        tft.drawString("Solar HMI", 160, 100);
        tft.setFont(&fonts::FreeSans9pt7b);
        tft.setTextColor(t->dim);
        tft.drawString(version, 160, 140);
        tft.setTextDatum(top_left);
    }

} // namespace LogoScreen
