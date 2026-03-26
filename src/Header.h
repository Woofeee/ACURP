// =============================================================
//  Header.h – sdílené záhlaví a spodní lišta
//
//  Záhlaví (26px nahoře) a spodní lišta (26px dole) jsou
//  společné pro všechny screeny kromě Boot a Logo.
//
//  Záhlaví: čas | ●AP ●STA ●INV | ⚠
//  Spodní:  indikátory výstupů relé (jen aktivní zásobníky)
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

// Rozměry kostiček relé
#define FTR_BOX_W   16
#define FTR_BOX_H   14
#define FTR_BOX_GAP 3       // mezera mezi kostičkami

extern Config gConfig;

namespace Header {

    // Blikání alarmu – interní
    static uint32_t _lastBlinkMs  = 0;
    static bool     _alarmVisible = true;

    // Cache stavu relé pro updateFooter
    static bool _lastRelayOn[10]      = {};
    static bool _lastRelayHeating[10] = {};
    static uint8_t _lastNumBoilers    = 0;

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
            tft.setCursor(303, 8);
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
    //  Zobrazí jen gConfig.numBoilers kostiček, vycentrovaných.
    // ---------------------------------------------------------
    void drawFooter(const Theme* t, const SolarData& d) {
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);

        uint8_t n = gConfig.numBoilers;
        if (n == 0) return;
        if (n > 10) n = 10;

        // Šířka celé skupiny kostiček
        uint16_t totalW = n * FTR_BOX_W + (n - 1) * FTR_BOX_GAP;
        int16_t startX = (320 - totalW) / 2;
        int16_t y = FTR_Y + (FTR_H - FTR_BOX_H) / 2;

        for (uint8_t i = 0; i < n; i++) {
            int16_t x = startX + i * (FTR_BOX_W + FTR_BOX_GAP);

            uint8_t state = OUT_IDLE;
            if (d.relayOn[i]) {
                state = d.relayHeating[i] ? OUT_HEATING : OUT_DONE;
            }

            uint16_t col = _outColor(t, state);
            if (state == OUT_IDLE) {
                // Jen obrys
                tft.drawRect(x, y, FTR_BOX_W, FTR_BOX_H, t->dim);
            } else {
                tft.fillRect(x, y, FTR_BOX_W, FTR_BOX_H, col);
            }
        }

        // Uložit cache
        _lastNumBoilers = n;
        for (uint8_t i = 0; i < 10; i++) {
            _lastRelayOn[i]      = d.relayOn[i];
            _lastRelayHeating[i] = d.relayHeating[i];
        }
    }

    // ---------------------------------------------------------
    //  Aktualizuj záhlaví – volej každou sekundu
    //  Překreslí jen části které se mění (čas, puntíky, alarm)
    // ---------------------------------------------------------
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm) {

        // Čas – přes sprite aby neblikalo
        static LGFX_Sprite _sprTime(&tft);
        static bool _sprTimeCreated = false;
        if (!_sprTimeCreated) {
            _sprTime.createSprite(75, HDR_H);
            _sprTimeCreated = true;
        }
        _sprTime.fillScreen(t->header);
        _sprTime.setFont(&fonts::DejaVu24);
        _sprTime.setTextColor(t->accent);
        _sprTime.setCursor(0, 2);
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour, dt.minute);
        _sprTime.print(buf);
        _sprTime.pushSprite(8, 0);

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
    //  Překreslí jen pokud se stav změnil.
    // ---------------------------------------------------------
    void updateFooter(const Theme* t, const SolarData& d) {
        // Detekce změny
        bool changed = (_lastNumBoilers != gConfig.numBoilers);
        if (!changed) {
            uint8_t n = gConfig.numBoilers;
            if (n > 10) n = 10;
            for (uint8_t i = 0; i < n; i++) {
                if (d.relayOn[i] != _lastRelayOn[i] ||
                    d.relayHeating[i] != _lastRelayHeating[i]) {
                    changed = true;
                    break;
                }
            }
        }
        if (!changed) return;

        drawFooter(t, d);
    }

} // namespace Header