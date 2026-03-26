// =============================================================
//  SettingScreen.h – nastavení
//
//  Sekce Datum / Cas:
//    Datum (DD.MM.YYYY) – editovatelné pokud NTP off nebo WiFi offline
//    Čas   (HH:MM)      – editovatelné pokud NTP off nebo WiFi offline
//    NTP sync (on/off)
//
//  Sekce Displej:
//    Téma (Dark / Industrial / Light)
//    Timeout podsvícení [min]
//    Backlight [%]
//
//  Navigace:
//    UP/DOWN – pohyb kurzoru / změna hodnoty při editaci
//    RIGHT   – další pole při editaci datumu/času
//    LEFT    – zpět do MENU / zruš editaci / předchozí pole
//    CENTER  – vstup do editace / potvrzení pole / zápis do RTC
//
//  Datum/Čas readonly pokud NTP=on AND gWifiSta=true
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

// WiFi stav a RTC z main.cpp
extern volatile bool gWifiSta;
extern PCF85063A     gRTC;
extern volatile bool gNtpResync;

namespace SettingScreen {

    #define SET_VISIBLE   6
    #define SET_ROW_H     26
    #define SET_SEC_H     16
    #define SET_START_Y   (CONTENT_Y + 4)

    // ---------------------------------------------------------
    //  Definice položek
    // ---------------------------------------------------------
    enum ItemId : uint8_t {
        ITEM_DATE = 0,
        ITEM_TIME,
        ITEM_NTP,
        ITEM_THEME,
        ITEM_TIMEOUT,
        ITEM_BACKLIGHT,
        ITEM_COUNT
    };

    struct SettingItem {
        const char* section;
        const char* label;
        char        value[16];
    };

    static SettingItem _items[ITEM_COUNT] = {
        { "Datum / Cas",  "Datum",     "01.01.2025" },
        { nullptr,        "Cas",       "12:00"      },
        { nullptr,        "NTP sync",  "on"         },
        { "Displej",      "Tema",      "Dark"       },
        { nullptr,        "Timeout",   "5 min"      },
        { nullptr,        "Backlight", "80 %"       },
    };

    static uint8_t  _cursor       = 0;
    static uint8_t  _scrollOffset = 0;
    static bool     _editing      = false;

    // Stav editace datumu/času
    static uint8_t  _dtField  = 0;   // aktivní pole (datum: 0=DD,1=MM,2=YYYY  čas: 0=HH,1=MM)
    static uint8_t  _dtDay    = 1;
    static uint8_t  _dtMonth  = 1;
    static uint16_t _dtYear   = 2025;
    static uint8_t  _dtHour   = 0;
    static uint8_t  _dtMinute = 0;

    // ---------------------------------------------------------
    //  Pomocné: je datum/čas readonly?
    // ---------------------------------------------------------
    static bool _dtReadonly() {
        return (strcmp(_items[ITEM_NTP].value, "on") == 0) && gWifiSta;
    }

    // ---------------------------------------------------------
    //  Načti čas z RTC do zobrazovaných hodnot
    // ---------------------------------------------------------
    static void _loadFromRTC() {
        DateTime dt = gRTC.getTime();
        _dtDay    = dt.day;
        _dtMonth  = dt.month;
        _dtYear   = dt.year;
        _dtHour   = dt.hour;
        _dtMinute = dt.minute;
        snprintf(_items[ITEM_DATE].value, 16, "%02d.%02d.%04d",
                 dt.day, dt.month, dt.year);
        snprintf(_items[ITEM_TIME].value, 16, "%02d:%02d",
                 dt.hour, dt.minute);
    }

    // ---------------------------------------------------------
    //  Zapiš editované hodnoty do RTC
    // ---------------------------------------------------------
    static void _saveToRTC(ItemId item) {
        DateTime dt = gRTC.getTime();  // zachovej aktuální sekundy/ostatní pole
        if (item == ITEM_DATE) {
            dt.day   = _dtDay;
            dt.month = _dtMonth;
            dt.year  = _dtYear;
        } else {
            dt.hour   = _dtHour;
            dt.minute = _dtMinute;
            dt.second = 0;  // reset sekund při ručním nastavení
        }
        if (gRTC.setTime(dt)) {
            Serial.printf("[SET] RTC zapsano: %02d.%02d.%04d %02d:%02d\n",
                dt.day, dt.month, dt.year, dt.hour, dt.minute);
        } else {
            Serial.println("[SET] RTC zapis selhal!");
        }
        _loadFromRTC();
    }

    // ---------------------------------------------------------
    //  Nakresli inline editaci datumu (aktivní pole podtrženo)
    // ---------------------------------------------------------
    static void _drawDateEdit(const Theme* t, int16_t y) {
        char parts[3][8];
        snprintf(parts[0], 8, "%02d", _dtDay);
        snprintf(parts[1], 8, "%02d", _dtMonth);
        snprintf(parts[2], 8, "%04d", _dtYear);

        tft.fillRect(155, y, 153, SET_ROW_H, t->header);
        tft.setFont(&fonts::Font2);

        // X pozice polí: DD . MM . YYYY
        const int16_t xs[3] = { 160, 184, 208 };
        const int16_t ws[3] = { 16, 16, 32 };

        for (uint8_t i = 0; i < 3; i++) {
            bool active = (_dtField == i);
            tft.setTextColor(active ? t->accent : t->text);
            tft.setCursor(xs[i], y + 7);
            tft.print(parts[i]);
            // Podtržení aktivního pole
            if (active) {
                tft.fillRect(xs[i], y + SET_ROW_H - 4, ws[i], 2, t->accent);
            }
            // Tečka oddělovač
            if (i < 2) {
                tft.setTextColor(t->dim);
                tft.setCursor(xs[i] + ws[i], y + 7);
                tft.print(".");
            }
        }
    }

    // ---------------------------------------------------------
    //  Nakresli inline editaci času (aktivní pole podtrženo)
    // ---------------------------------------------------------
    static void _drawTimeEdit(const Theme* t, int16_t y) {
        char parts[2][4];
        snprintf(parts[0], 4, "%02d", _dtHour);
        snprintf(parts[1], 4, "%02d", _dtMinute);

        tft.fillRect(155, y, 153, SET_ROW_H, t->header);
        tft.setFont(&fonts::Font2);

        const int16_t xs[2] = { 190, 216 };

        for (uint8_t i = 0; i < 2; i++) {
            bool active = (_dtField == i);
            tft.setTextColor(active ? t->accent : t->text);
            tft.setCursor(xs[i], y + 7);
            tft.print(parts[i]);
            if (active) {
                tft.fillRect(xs[i], y + SET_ROW_H - 4, 16, 2, t->accent);
            }
            if (i == 0) {
                tft.setTextColor(t->dim);
                tft.setCursor(xs[i] + 16, y + 7);
                tft.print(":");
            }
        }
    }

    // ---------------------------------------------------------
    //  Nakresli jednu položku na danou Y
    // ---------------------------------------------------------
    static void _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        SettingItem& item = _items[idx];
        bool active  = (idx == _cursor);
        bool editing = (active && _editing);
        bool dtItem  = (idx == ITEM_DATE || idx == ITEM_TIME);
        bool rdonly  = dtItem && _dtReadonly();

        // Sekce header
        if (item.section != nullptr) {
            tft.fillRect(0, y, 320, SET_SEC_H, t->bg);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(t->dim);
            tft.setCursor(16, y);
            tft.print(item.section);
            y += 14;
            tft.drawFastHLine(16, y, 288, t->dim);
            y += 2;
        }

        // Pozadí
        if (active) {
            tft.fillRect(8, y, 304, SET_ROW_H, t->header);
            tft.fillRect(8, y, 3,   SET_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, SET_ROW_H, t->bg);
        }

        // Label
        tft.setFont(&fonts::Font2);
        tft.setTextColor(rdonly ? t->dim : active ? t->accent : t->text);
        tft.setCursor(20, y + 7);
        tft.print(item.label);

        // Readonly indikátor
        if (rdonly) {
            tft.setTextColor(t->dim);
            tft.setTextDatum(middle_right);
            tft.drawString("NTP", 308, y + 12);
            tft.setTextDatum(top_left);
            return;
        }

        // Hodnota – editace nebo statická
        if (editing && idx == ITEM_DATE) {
            _drawDateEdit(t, y);
        } else if (editing && idx == ITEM_TIME) {
            _drawTimeEdit(t, y);
        } else if (strlen(item.value) > 0) {
            tft.setTextColor(editing ? t->accent : t->text);
            tft.setTextDatum(middle_right);
            tft.drawString(item.value, 308, y + 12);
            tft.setTextDatum(top_left);
        }
    }

    // ---------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ---------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
        int16_t y = SET_START_Y;
        uint8_t end = min((uint8_t)ITEM_COUNT, (uint8_t)(_scrollOffset + SET_VISIBLE));
        for (uint8_t i = _scrollOffset; i < end; i++) {
            _drawItemAt(t, i, y);
            if (_items[i].section != nullptr) y += SET_SEC_H;
            y += SET_ROW_H;
        }
    }

    // ---------------------------------------------------------
    //  Překresli jednu položku (najde Y na displeji)
    // ---------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t idx) {
        if (idx < _scrollOffset || idx >= _scrollOffset + SET_VISIBLE) return;
        int16_t y = SET_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (_items[i].section != nullptr) y += SET_SEC_H;
            y += SET_ROW_H;
        }
        _drawItemAt(t, idx, y);
    }

    // ---------------------------------------------------------
    //  Scroll
    // ---------------------------------------------------------
    static bool _ensureVisible() {
        if (_cursor < _scrollOffset) {
            _scrollOffset = _cursor;
            return true;
        }
        if (_cursor >= _scrollOffset + SET_VISIBLE) {
            _scrollOffset = _cursor - SET_VISIBLE + 1;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------
    //  Veřejné rozhraní
    // ---------------------------------------------------------
    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {
        _loadFromRTC();
        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN  RIGHT pole  CENTER ok  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
        _drawAllItems(t);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
        // Aktualizuj čas z RTC každou sekundu pokud needitujeme
        if (!_editing) {
            _loadFromRTC();
            _drawItem(t, ITEM_DATE);
            _drawItem(t, ITEM_TIME);
        }
    }

    // ---------------------------------------------------------
    //  Editace hodnot (NTP, Téma, Timeout, Backlight)
    // ---------------------------------------------------------
    static void _editUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_NTP:
                strncpy(_items[idx].value,
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on", 16);
                break;
            case ITEM_THEME:
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

    // Editace polí datumu
    static void _dateUp() {
        switch (_dtField) {
            case 0: _dtDay   = (_dtDay   % 31) + 1; break;
            case 1: _dtMonth = (_dtMonth % 12) + 1; break;
            case 2: if (_dtYear < 2099) _dtYear++;  break;
        }
    }
    static void _dateDown() {
        switch (_dtField) {
            case 0: _dtDay   = (_dtDay   > 1) ? _dtDay   - 1 : 31; break;
            case 1: _dtMonth = (_dtMonth > 1) ? _dtMonth - 1 : 12; break;
            case 2: if (_dtYear > 2000) _dtYear--;  break;
        }
    }

    // Editace polí času
    static void _timeUp() {
        switch (_dtField) {
            case 0: _dtHour   = (_dtHour   + 1) % 24; break;
            case 1: _dtMinute = (_dtMinute + 1) % 60; break;
        }
    }
    static void _timeDown() {
        switch (_dtField) {
            case 0: _dtHour   = (_dtHour   > 0) ? _dtHour   - 1 : 23; break;
            case 1: _dtMinute = (_dtMinute > 0) ? _dtMinute - 1 : 59; break;
        }
    }

    // ---------------------------------------------------------
    //  Obsluha vstupu
    // ---------------------------------------------------------
    Screen handleInput(const Theme* t, SwButton btn) {
        bool dtItem   = (_cursor == ITEM_DATE || _cursor == ITEM_TIME);
        uint8_t maxField = (_cursor == ITEM_DATE) ? 2 : 1;

        if (_editing) {
            // --- Editace datumu / času ---
            if (dtItem) {
                switch (btn) {
                    case SW_UP:
                        if (_cursor == ITEM_DATE) _dateUp();
                        else                      _timeUp();
                        _drawItem(t, _cursor);
                        return SCREEN_NONE;
                    case SW_DOWN:
                        if (_cursor == ITEM_DATE) _dateDown();
                        else                      _timeDown();
                        _drawItem(t, _cursor);
                        return SCREEN_NONE;
                    case SW_RIGHT:
                        if (_dtField < maxField) {
                            _dtField++;
                            _drawItem(t, _cursor);
                        }
                        return SCREEN_NONE;
                    case SW_LEFT:
                        if (_dtField > 0) {
                            _dtField--;
                            _drawItem(t, _cursor);
                        } else {
                            // Zruš editaci
                            _editing = false;
                            _loadFromRTC();
                            _drawItem(t, _cursor);
                        }
                        return SCREEN_NONE;
                    case SW_CENTER:
                        if (_dtField < maxField) {
                            // Přejdi na další pole
                            _dtField++;
                            _drawItem(t, _cursor);
                        } else {
                            // Poslední pole – zapiš do RTC
                            _editing = false;
                            _saveToRTC((ItemId)_cursor);
                            _drawItem(t, _cursor);
                        }
                        return SCREEN_NONE;
                    default:
                        return SCREEN_NONE;
                }
            }

            // --- Editace ostatních položek ---
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
                    _editing = false;
                    // Pokud se zapnulo NTP, spusť okamžitý sync
                    if (_cursor == ITEM_NTP && strcmp(_items[ITEM_NTP].value, "on") == 0) {
                        gNtpResync = true;
                    }
                    ConfigManager::saveBlockSystem();
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_LEFT:
                    _editing = false;
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                default:
                    return SCREEN_NONE;
            }
        }

        // --- Normální navigace ---
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    uint8_t prev = _cursor--;
                    if (_ensureVisible()) {
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, prev);
                        _drawItem(t, _cursor);
                    }
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    uint8_t prev = _cursor++;
                    if (_ensureVisible()) {
                        _drawAllItems(t);
                    } else {
                        _drawItem(t, prev);
                        _drawItem(t, _cursor);
                    }
                }
                return SCREEN_NONE;

            case SW_CENTER:
                if (dtItem && _dtReadonly()) return SCREEN_NONE;
                if (dtItem) {
                    _loadFromRTC();
                    _dtField = 0;
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
        _cursor       = 0;
        _scrollOffset = 0;
        _editing      = false;
        _dtField      = 0;
    }

} // namespace SettingScreen