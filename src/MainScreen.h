// =============================================================
//  screens/MainScreen.h – hlavní obrazovka s hodinami
//
//  Použití:
//    MainScreen::begin(theme, version);  // jednou při přepnutí
//    MainScreen::update(theme, dt, staOk, apOk, rtcValid);  // každou sekundu
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"
#include "PCF85063A.h"

namespace MainScreen {

    static LGFX_Sprite _sprClock(&tft);
    static LGFX_Sprite _sprDate(&tft);

    void begin(const Theme* t, const char* version) {
        tft.fillScreen(t->bg);

        // Záhlaví
        tft.fillRect(0, 0, 320, 24, t->header);
        tft.setFont(&fonts::FreeSansBold9pt7b);
        tft.setTextColor(t->text);
        tft.setCursor(8, 5);
        tft.print("Solar HMI");
        tft.drawFastHLine(0, 24, 320, t->dim);

        // Dolní lišta
        tft.setFont(&fonts::FreeSans9pt7b);
        tft.setTextColor(t->dim);
        tft.setCursor(8, 232);
        tft.print(version);

        // Sprites
        _sprClock.createSprite(300, 58);
        _sprDate.createSprite(200, 22);
    }

    void update(const Theme* t, const DateTime& dt,
                bool staOk, bool apOk, bool rtcValid) {
        // Čas
        _sprClock.fillScreen(t->bg);
        _sprClock.setFont(&fonts::FreeSansBold18pt7b);
        _sprClock.setTextColor(t->text);
        _sprClock.setTextDatum(middle_center);
        char tBuf[9];
        snprintf(tBuf, sizeof(tBuf), "%02d:%02d:%02d",
                 dt.hour, dt.minute, dt.second);
        _sprClock.drawString(tBuf, 150, 29);
        _sprClock.pushSprite(10, 80);

        // Datum
        _sprDate.fillScreen(t->bg);
        _sprDate.setFont(&fonts::FreeSans9pt7b);
        _sprDate.setTextColor(t->dim);
        _sprDate.setTextDatum(middle_center);
        char dBuf[11];
        snprintf(dBuf, sizeof(dBuf), "%02d.%02d.%04d",
                 dt.day, dt.month, dt.year);
        _sprDate.drawString(dBuf, 100, 11);
        _sprDate.pushSprite(60, 148);

        // Stavové tečky v záhlaví vpravo
        // AP tečka
        tft.fillCircle(294, 12, 5, apOk ? t->ok : t->dim);
        // STA tečka
        tft.fillCircle(308, 12, 5, staOk ? t->accent : t->dim);

        // NTP varování
        tft.fillRect(0, 170, 320, 16, t->bg);
        if (!rtcValid) {
            tft.setFont(&fonts::FreeSans9pt7b);
            tft.setTextColor(t->warn);
            tft.setCursor(8, 182);
            tft.print("! Cekam na NTP sync...");
        }
    }

} // namespace MainScreen