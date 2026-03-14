// =============================================================
//  MenuScreen.h – hlavní menu
//
//  Položky: Historie / Diagnostika / Nastavení / Instalace / Zpět
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru (scroll pokud více než MENU_VISIBLE)
//    CENTER   – vstup do podmenu
//    LEFT     – zpět
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
    Screen      target;
    bool        arrow;
};

namespace MenuScreen {

    static const MenuItem _items[] = {
        { "Historie",    SCREEN_HISTORY,    true  },
        { "Diagnostika", SCREEN_DIAGNOSTIC, true  },
        { "Nastaveni",   SCREEN_SETTING,    true  },
        { "Instalace",   SCREEN_PASSWORD,   true  },
        //{ "Rezerva",     SCREEN_NONE,       false },
        //{ "Rezerva",     SCREEN_NONE,       false },
        //{ "Rezerva",     SCREEN_NONE,       false },
    };
    static const uint8_t _itemCount    = sizeof(_items) / sizeof(_items[0]);
    static const uint8_t MENU_VISIBLE  = 4;    // počet viditelných položek

    static uint8_t _cursor       = 0;  // absolutní index aktivní položky
    static uint8_t _scrollOffset = 0;  // index první viditelné položky

    #define MENU_ROW_H   34
    #define MENU_START_Y (CONTENT_Y + 22)

    // ---------------------------------------------------------
    //  Nakresli jednu položku na pozici slot (0–3)
    // ---------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t slot, uint8_t idx, bool active) {
        int16_t y = MENU_START_Y + slot * MENU_ROW_H;

        if (active) {
            tft.fillRect(8, y, 304, MENU_ROW_H, t->header);
            tft.fillRect(8, y, 3, MENU_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, MENU_ROW_H, t->bg);
        }

        tft.setFont(&fonts::Font4);
        tft.setTextColor(active ? t->accent : 0x7BEF);
        tft.setCursor(20, y + 8);
        tft.print(_items[idx].label);

        if (_items[idx].arrow) {
            tft.setFont(&fonts::Font2);
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(300, y + 12);
            tft.print(">");
        }

       /*
        // Scrollbar indikátor – tečka vpravo pokud je více položek
        if (_itemCount > MENU_VISIBLE) {
            uint8_t dotY = MENU_START_Y + (_cursor * (MENU_VISIBLE * MENU_ROW_H - 6)) / (_itemCount - 1);
            tft.fillRect(314, MENU_START_Y, 4, MENU_VISIBLE * MENU_ROW_H, t->bg);
            tft.fillRect(314, MENU_START_Y, 2, MENU_VISIBLE * MENU_ROW_H, t->dim);
            tft.fillRect(313, dotY, 4, 6, t->accent);
        }
        */
    }

    // ---------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ---------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        uint8_t visCount = min(_itemCount, MENU_VISIBLE);
        for (uint8_t slot = 0; slot < visCount; slot++) {
            uint8_t idx = _scrollOffset + slot;
            _drawItem(t, slot, idx, idx == _cursor);
        }
    }

    // ---------------------------------------------------------
    //  Nakresli celé menu
    // ---------------------------------------------------------
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);

        Header::draw(t, dt, apState, staState, invState, alarm);
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);  // prázdná spodní lišta

        // Nadpis
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setCursor(16, CONTENT_Y + 4);
        tft.print("MENU");
        tft.drawFastHLine(16, CONTENT_Y + 18, 288, t->dim);

        // Položky
        _drawAllItems(t);

        // Nápověda
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
                    _cursor--;
                    // Scroll nahoru pokud kurzor vyšel z viditelné oblasti
                    if (_cursor < _scrollOffset) {
                        _scrollOffset--;
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, _cursor - _scrollOffset + 1, _cursor + 1, false);
                        _drawItem(t, _cursor - _scrollOffset,     _cursor,     true);
                    }
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < _itemCount - 1) {
                    _cursor++;
                    // Scroll dolů pokud kurzor vyšel z viditelné oblasti
                    if (_cursor >= _scrollOffset + MENU_VISIBLE) {
                        _scrollOffset++;
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, _cursor - _scrollOffset - 1, _cursor - 1, false);
                        _drawItem(t, _cursor - _scrollOffset,     _cursor,     true);
                    }
                }
                return SCREEN_NONE;

            case SW_CENTER: {
                Screen target = _items[_cursor].target;
                if (target == SCREEN_NONE) return SCREEN_MAIN;
                return target;
            }

            case SW_LEFT:
                return SCREEN_MAIN;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _scrollOffset = 0;
    }

} // namespace MenuScreen