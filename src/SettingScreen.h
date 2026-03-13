// =============================================================
//  SettingScreen.h – nastavení
//
//  Sekce Date/Time:
//    Datum (DD.MM.YYYY)
//    Čas (HH:MM)
//    NTP sync (on/off)
//
//  Sekce Displej:
//    Téma (Dark / Industrial / Light)
//    Timeout podsvícení [min]
//    Backlight [%]   ← scrolluje se k ní
//
//  Navigace:
//    UP/DOWN – pohyb kurzoru
//    CENTER  – vstup do editace hodnoty
//    LEFT    – zpět do MENU (nebo zruš editaci)
//
//  Editace: UP/DOWN mění hodnotu, CENTER potvrdí, LEFT zruší
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

namespace SettingScreen {

    // ---------------------------------------------------------
    //  Definice položek nastavení
    // ---------------------------------------------------------
    enum ItemId : uint8_t {
        ITEM_DATE = 0,
        ITEM_TIME,
        ITEM_NTP,
        ITEM_THEME,
        ITEM_TIMEOUT,
        ITEM_BACKLIGHT,
        ITEM_BACK,
        ITEM_COUNT
    };

    struct SettingItem {
        const char* section;    // nullptr = pokračování sekce
        const char* label;
        char        value[16];
    };

    static SettingItem _items[ITEM_COUNT] = {
        { "Datum / Cas",    "Datum",          "01.01.2025" },
        { nullptr,          "Cas",            "12:00"      },
        { nullptr,          "NTP sync",       "on"         },
        { "Displej",        "Tema",           "Dark"       },
        { nullptr,          "Timeout",        "5 min"      },
        { nullptr,          "Backlight",      "80 %"       },
        { nullptr,          "Zpet",           ""           },
    };

    static uint8_t _cursor   = 0;
    static bool    _editing  = false;
    static int16_t _scrollY  = 0;  // scroll offset (budoucnost)

    // Výška jednoho řádku
    #define SET_ROW_H    26
    #define SET_START_Y  (CONTENT_Y + 8)

    // ---------------------------------------------------------
    //  Nakresli jednu položku
    // ---------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t idx) {
        SettingItem& item = _items[idx];
        int16_t y = SET_START_Y + idx * SET_ROW_H;

        bool active  = (idx == _cursor);
        bool editing = (active && _editing);
        bool isBack  = (idx == ITEM_BACK);

        // Sekce header
        if (item.section != nullptr) {
            tft.setFont(&fonts::Font2);
            tft.setTextColor(t->dim);
            tft.setCursor(16, y);
            tft.print(item.section);
            y += 14;
            tft.drawFastHLine(16, y, 288, t->dim);
            y += 2;
        }

        // Pozadí aktivní položky
        if (active) {
            tft.fillRect(8, y, 304, SET_ROW_H, t->header);
            tft.fillRect(8, y, 3, SET_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, SET_ROW_H, t->bg);
        }

        // Label
        tft.setFont(&fonts::Font2);
        tft.setTextColor(isBack ? t->dim :
                         active  ? t->accent : t->text);
        tft.setCursor(20, y + 7);
        tft.print(item.label);

        // Hodnota zarovnaná vpravo
        if (strlen(item.value) > 0) {
            tft.setTextColor(editing ? t->accent : t->text);
            tft.setTextDatum(middle_right);
            tft.drawString(item.value, 308, y + 12);
            tft.setTextDatum(top_left);
        }
    }

    // ---------------------------------------------------------
    //  Nakresli celé nastavení
    // ---------------------------------------------------------
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);

        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            _drawItem(t, i);
        }
    }

    // ---------------------------------------------------------
    //  Aktualizuj jen záhlaví
    // ---------------------------------------------------------
    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    // ---------------------------------------------------------
    //  Editace hodnot – závisí na položce
    //  TODO: napojit na RTC, ThemeManager, DisplayConfig
    // ---------------------------------------------------------
    static void _editUp(uint8_t idx) {
        // Zjednodušené – reálná editace bude specifická pro každý item
        switch ((ItemId)idx) {
            case ITEM_NTP:
                strncpy(_items[idx].value,
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on", 16);
                break;
            case ITEM_THEME:
                // Přepínej: Dark → Industrial → Light → Dark
                if      (strcmp(_items[idx].value, "Dark") == 0)
                    strncpy(_items[idx].value, "Industrial", 16);
                else if (strcmp(_items[idx].value, "Industrial") == 0)
                    strncpy(_items[idx].value, "Light", 16);
                else
                    strncpy(_items[idx].value, "Dark", 16);
                break;
            case ITEM_BACKLIGHT: {
                int val = atoi(_items[idx].value);
                val = min(val + 10, 100);
                snprintf(_items[idx].value, 16, "%d %%", val);
                break;
            }
            case ITEM_TIMEOUT: {
                int val = atoi(_items[idx].value);
                val = min(val + 1, 60);
                snprintf(_items[idx].value, 16, "%d min", val);
                break;
            }
            default: break;
        }
    }

    static void _editDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_NTP:
                strncpy(_items[idx].value,
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on", 16);
                break;
            case ITEM_THEME:
                if      (strcmp(_items[idx].value, "Light") == 0)
                    strncpy(_items[idx].value, "Industrial", 16);
                else if (strcmp(_items[idx].value, "Industrial") == 0)
                    strncpy(_items[idx].value, "Dark", 16);
                else
                    strncpy(_items[idx].value, "Light", 16);
                break;
            case ITEM_BACKLIGHT: {
                int val = atoi(_items[idx].value);
                val = max(val - 10, 10);
                snprintf(_items[idx].value, 16, "%d %%", val);
                break;
            }
            case ITEM_TIMEOUT: {
                int val = atoi(_items[idx].value);
                val = max(val - 1, 1);
                snprintf(_items[idx].value, 16, "%d min", val);
                break;
            }
            default: break;
        }
    }

    // ---------------------------------------------------------
    //  Obsluha vstupu
    // ---------------------------------------------------------
    Screen handleInput(const Theme* t, SwButton btn) {
        if (_editing) {
            switch (btn) {
                case SW_UP:
                    _editUp(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_DOWN:
                    _editDown(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_CENTER:
                    // Uložit a ukončit editaci
                    _editing = false;
                    // TODO: uložit do FRAM / RTC
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_LEFT:
                    // Zrušit editaci
                    _editing = false;
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                default:
                    return SCREEN_NONE;
            }
        }

        // Normální navigace
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    uint8_t prev = _cursor--;
                    _drawItem(t, prev);
                    _drawItem(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    uint8_t prev = _cursor++;
                    _drawItem(t, prev);
                    _drawItem(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_CENTER:
                if (_cursor == ITEM_BACK) return SCREEN_MENU;
                if (_cursor == ITEM_DATE || _cursor == ITEM_TIME) {
                    // TODO: přejít na dedikovaný datetime editor screen
                    return SCREEN_NONE;
                }
                _editing = true;
                _drawItem(t, _cursor);
                return SCREEN_NONE;

            case SW_LEFT:
                return SCREEN_MENU;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor  = 0;
        _editing = false;
    }

} // namespace SettingScreen
