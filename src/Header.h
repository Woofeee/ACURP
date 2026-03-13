// =============================================================
//  Header.h – sdílené záhlaví a spodní lišta
//
//  Záhlaví (26px nahoře) a spodní lišta (26px dole) jsou
//  společné pro všechny screeny kromě Boot a Logo.
//
//  Záhlaví: čas | ●AP ●STA ●INV | ⚠
//  Spodní:  10 indikátorů výstupů relé
//
//  Použití:
//    Header::draw(theme, data);    // první vykreslení
//    Header::update(theme, data);  // aktualizace (každou sekundu)
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"
#include "SolarData.h"
#include "PCF85063A.h"

// Rozměry záhlaví a spodní lišty
#define HDR_H       26      // výška záhlaví [px]
#define FTR_H       26      // výška spodní lišty [px]
#define HDR_Y        0      // Y pozice záhlaví
#define FTR_Y      214      // Y pozice spodní lišty (240-26)
#define CONTENT_Y   26      // Y začátek obsahu
#define CONTENT_H  188      // výška obsahu (240-26-26)

// Stavy puntíků
#define DOT_OFF     0       // šedá – vypnuto
#define DOT_ERROR   1       // červená – chyba / čeká
#define DOT_OK      2       // zelená – vše OK

// Indikátory výstupů – stavy
#define OUT_IDLE    0       // prázdná – neaktivní
#define OUT_HEATING 1       // červená – ohřívá se (teče proud)
#define OUT_DONE    2       // zelená  – nahřátý (termostat off)


namespace Header {

    // Blikání alarmu – interní
    static uint32_t _lastBlinkMs  = 0;
    static bool     _alarmVisible = true;

    // ---------------------------------------------------------
    //  Interní: nakresli/smaž alarm trojúhelník
    //  MUSÍ být deklarováno před draw() a update() !
    // ---------------------------------------------------------
    static void _drawAlarm(const Theme* t, bool visible) {
        if (visible) {
            tft.fillTriangle(304, 6, 312, 20, 296, 20, t->header);
            tft.drawTriangle(304, 6, 312, 20, 296, 20, t->err);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(t->err);
            tft.setCursor(301, 10);
            tft.print("!");
        } else {
            tft.fillRect(294, 4, 20, 18, t->header);
        }
    }

    // ---------------------------------------------------------
    //  Vrátí barvu puntíku podle stavu
    // ---------------------------------------------------------
    static uint16_t _dotColor(const Theme* t, uint8_t state) {
        switch (state) {
            case DOT_OK:    return t->ok;
            case DOT_ERROR: return t->err;
            default:        return t->dim;
        }
    }

    // ---------------------------------------------------------
    //  Vrátí barvu indikátoru výstupu
    // ---------------------------------------------------------
    static uint16_t _outColor(const Theme* t, uint8_t state) {
        switch (state) {
            case OUT_HEATING: return t->err;
            case OUT_DONE:    return t->ok;
            default:          return 0x0000; // průhledná = jen obrys
        }
    }

    // ---------------------------------------------------------
    //  Nakresli záhlaví – volej při draw() screenu
    // ---------------------------------------------------------
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm) {
        // Pozadí záhlaví
        tft.fillRect(0, HDR_Y, 320, HDR_H, t->header);

        // Čas
        tft.setFont(&fonts::DejaVu24);
        tft.setTextColor(t->accent);
        tft.setCursor(8, 2);
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
        tft.print(buf);

        // Puntík AP
        tft.fillCircle(100, 12, 6, _dotColor(t, apState));
        tft.setTextColor(t->dim);
        tft.setCursor(110, 2);
        tft.print("AP");

        // Puntík STA
        tft.fillCircle(160, 12, 6, _dotColor(t, staState));
        tft.setCursor(170, 2);
        tft.print("STA");

        // Puntík INV
        tft.fillCircle(235, 12, 6, _dotColor(t, invState));
        tft.setCursor(245, 2);
        tft.print("INV");

        // Alarm trojúhelník
        if (alarm) {
            _alarmVisible = true;
            _drawAlarm(t, true);
        }
    }

    // ---------------------------------------------------------
    //  Nakresli spodní lištu – volej při draw() screenu
    // ---------------------------------------------------------
    void drawFooter(const Theme* t, const SolarData& d) {
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);

        for (uint8_t i = 0; i < 10; i++) {
            int16_t x = 8 + i * 19;
            int16_t y = FTR_Y + 6;

            uint8_t state = OUT_IDLE;
            if (d.relayOn[i]) {
                state = d.relayHeating[i] ? OUT_HEATING : OUT_DONE;
            }

            uint16_t col = _outColor(t, state);
            if (state == OUT_IDLE) {
                // Jen obrys
                tft.drawRect(x, y, 16, 14, t->dim);
            } else {
                tft.fillRect(x, y, 16, 14, col);
            }
        }
    }

    // ---------------------------------------------------------
    //  Aktualizuj záhlaví – volej každou sekundu
    //  Překreslí jen části které se mění (čas, puntíky, alarm)
    // ---------------------------------------------------------
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm) {
        // Čas – překresli oblast
        tft.fillRect(8, 5, 55, 16, t->header);
        tft.setFont(&fonts::DejaVu24);
        tft.setTextColor(t->accent);
        tft.setCursor(8, 2);
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
        tft.print(buf);

        // Puntíky
        tft.fillCircle(100, 12, 6, _dotColor(t, apState));
        tft.fillCircle(160, 12, 6, _dotColor(t, staState));
        tft.fillCircle(235, 12, 6, _dotColor(t, invState));

        // Alarm – blikání 500ms
        if (alarm) {
            uint32_t now = millis();
            if (now - _lastBlinkMs >= 500) {
                _lastBlinkMs = now;
                _alarmVisible = !_alarmVisible;
                _drawAlarm(t, _alarmVisible);
            }
        } else {
            // Smazat alarm ikonu
            tft.fillRect(295, 4, 18, 18, t->header);
        }
    }

    // ---------------------------------------------------------
    //  Aktualizuj spodní lištu – volej při změně stavu relé
    // ---------------------------------------------------------
    void updateFooter(const Theme* t, const SolarData& d) {
        drawFooter(t, d);
    }

} // namespace Header
