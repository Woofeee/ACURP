// =============================================================
//  MenuScreen.h – hlavní menu
//
//  Položky: Historie / Diagnostika / Nastavení / Instalace / Zpět
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru
//    CENTER   – vstup do podmenu
//    LEFT     – zpět (= Zpět položka)
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"
#include "Header.h"
#include "ScreenManager.h"
#include "FiveWaySwitch.h"
#include "SolarData.h"
#include "PCF85063A.h"

// Definice položek menu
struct MenuItem {
    const char* label;
    Screen      target;     // cílový screen při výběru
    bool        arrow;      // zobrazit šipku ▶ (podmenu)
};

namespace MenuScreen {

    static const MenuItem _items[] = {
        { "Historie",    SCREEN_HISTORY,    true  },
        { "Diagnostika", SCREEN_DIAGNOSTIC, true  },
        { "Nastaveni",   SCREEN_SETTING,    true  },
        { "Instalace",   SCREEN_PASSWORD,   true  },
        { "Zpet",        SCREEN_NONE,       false },
    };
    static const uint8_t _itemCount = sizeof(_items) / sizeof(_items[0]);

    static uint8_t _cursor = 0;     // index aktivní položky

    // Výška řádku menu
    #define MENU_ROW_H   34
    #define MENU_START_Y (CONTENT_Y + 22)  // pod nadpisem

    // ---------------------------------------------------------
    //  Nakresli jednu položku menu
    // ---------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t idx, bool active) {
        int16_t y = MENU_START_Y + idx * MENU_ROW_H;

        // Pozadí
        if (active) {
            tft.fillRect(8, y, 304, MENU_ROW_H, t->header);
            // Cyan pruh vlevo
            tft.fillRect(8, y, 3, MENU_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, MENU_ROW_H, t->bg);
        }

        // Text
        tft.setFont(&fonts::Font4);
        tft.setTextColor(active ? t->accent : 0x7BEF);  // cyan nebo šedá
        tft.setCursor(20, y + 8);
        tft.print(_items[idx].label);

        // Šipka vpravo
        if (_items[idx].arrow) {
            tft.setFont(&fonts::Font2);
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(300, y + 12);
            tft.print(">");
        }
    }

    // ---------------------------------------------------------
    //  Nakresli celé menu
    // ---------------------------------------------------------
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);

        // Záhlaví + spodní lišta
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);

        // Nadpis
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setCursor(16, CONTENT_Y + 6);
        tft.print("MENU");
        tft.drawFastHLine(16, CONTENT_Y + 18, 288, t->dim);

        // Položky
        for (uint8_t i = 0; i < _itemCount; i++) {
            _drawItem(t, i, i == _cursor);
        }

        // Nápověda ve spodní liště
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DOWN  CENTER vstup  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Aktualizace záhlaví
    // ---------------------------------------------------------
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    // ---------------------------------------------------------
    //  Obsluha vstupu
    // ---------------------------------------------------------
    Screen handleInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    uint8_t prev = _cursor;
                    _cursor--;
                    _drawItem(t, prev, false);
                    _drawItem(t, _cursor, true);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < _itemCount - 1) {
                    uint8_t prev = _cursor;
                    _cursor++;
                    _drawItem(t, prev, false);
                    _drawItem(t, _cursor, true);
                }
                return SCREEN_NONE;

            case SW_CENTER: {
                Screen target = _items[_cursor].target;
                if (target == SCREEN_NONE) {
                    // Zpět položka
                    return SCREEN_MAIN;
                }
                return target;
            }

            case SW_LEFT:
                return SCREEN_MAIN;

            default:
                return SCREEN_NONE;
        }
    }

    // Reset kurzoru při vstupu do menu
    void reset() { _cursor = 0; }

} // namespace MenuScreen
