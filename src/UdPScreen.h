// =============================================================
//  UdPScreen.h – Uvedení do Provozu (chráněno PINem)
//
//  Položky:
//    Rizeni    → SCREEN_CONTROL (BoilerController konfigurace + Discovery)
//    Serial    → (future) SerialConfigScreen
//    Network   → (future) NetworkConfigScreen
//    MQTT      → (future) MqttConfigScreen
//    Stridac   → (future) InverterConfigScreen
//
//  LEFT → SCREEN_MENU
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

namespace UdPScreen {

    struct UdPItem {
        const char* label;
        const char* hint;
        Screen      target;
        bool        available;
    };

    static const UdPItem _items[] = {
        { "Rizeni",  "boilery, discovery", SCREEN_CONTROL,  true  },
        { "Serial",  "Modbus, RS485",      SCREEN_NONE,     false },
        { "Network", "WiFi, IP, NTP",      SCREEN_NETWORK,  true  },
        { "MQTT",    "broker, topic",      SCREEN_NONE,     false },
        { "Stridac", "typ, adresa",        SCREEN_NONE,     false },
    };
    static const uint8_t _itemCount = sizeof(_items) / sizeof(_items[0]);

    static uint8_t _cursor = 0;

    #define UDP_ROW_H   28
    #define UDP_START_Y (CONTENT_Y + 28)

    static void _drawItem(const Theme* t, uint8_t idx) {
        const UdPItem& item = _items[idx];
        int16_t y    = UDP_START_Y + idx * UDP_ROW_H;
        bool active  = (idx == _cursor);
        bool avail   = item.available;

        if (active) {
            tft.fillRect(8, y, 304, UDP_ROW_H, t->header);
            tft.fillRect(8, y, 3,   UDP_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, UDP_ROW_H, t->bg);
        }

        uint16_t col;
        if (!avail)  col = active ? t->dim : 0x2104;
        else         col = active ? t->accent : t->text;

        tft.setFont(&fonts::Font2);
        tft.setTextColor(col);
        tft.setCursor(20, y + 8);
        tft.print(item.label);

        if (strlen(item.hint) > 0) {
            tft.setTextColor(t->dim);
            tft.setTextDatum(middle_right);
            tft.drawString(item.hint, 310, y + 14);
            tft.setTextDatum(top_left);
        }

        if (!avail) {
            tft.setFont(&fonts::Font2);
            tft.setTextColor(0x2104);
            tft.setCursor(155, y + 8);
            tft.print("(brzy)");
        }

        if (avail) {
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(300, y + 8);
            tft.print(">");
        }
    }

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);

        // Spodní lišta – navigace (jako SettingScreen)
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN  CENTER vstup  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);

        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setCursor(16, CONTENT_Y + 6);
        tft.print("INSTALACE");
        tft.setTextColor(t->dim);
        tft.drawFastHLine(16, CONTENT_Y + 20, 288, t->dim);

        for (uint8_t i = 0; i < _itemCount; i++) {
            _drawItem(t, i);
        }
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                if (_cursor > 0) {
                    uint8_t prev = _cursor--;
                    _drawItem(t, prev);
                    _drawItem(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < _itemCount - 1) {
                    uint8_t prev = _cursor++;
                    _drawItem(t, prev);
                    _drawItem(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_CENTER: {
                const UdPItem& item = _items[_cursor];
                if (!item.available) return SCREEN_NONE;
                return item.target;
            }

            case SW_LEFT:
                return SCREEN_MENU;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() { _cursor = 0; }

} // namespace UdPScreen