// =============================================================
//  BootScreen.h – linux-style boot obrazovka
//
//  Max BOOT_MAX_LINES řádků. Při přidání dalšího řádku se
//  obsah posune nahoru (soft scroll – překreslení z bufferu).
//
//  Použití:
//    BootScreen::begin(theme, version);
//    BootScreen::print(theme, BOOT_OK, "I2C OK");
//    bool ok = BootScreen::wifiSta(theme, ssid, timeout_ms);
// =============================================================
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ST7789V_Pico2W.h"
#include "Theme.h"

#define BOOT_OK       0
#define BOOT_ERR      1
#define BOOT_WARN     2
#define BOOT_DISABLED 3

#define BOOT_FONT       &fonts::Font2
#define BOOT_LINE_H     20             // výška řádku [px]
#define BOOT_TEXT_Y      2             // Y offset textu v řádku
#define BOOT_ICON_X      4             // X ikony
#define BOOT_TEXT_X     60             // X textu
#define BOOT_HEADER_H   28             // výška záhlaví [px]
#define BOOT_MAX_LINES   9             // max viditelných řádků
                                       // ← změň toto číslo dle potřeby

namespace BootScreen {

    struct BootLine {
        char     icon[8];
        char     msg[52];
        uint16_t color;
        bool     used;
    };

    static BootLine     _lines[BOOT_MAX_LINES] = {};
    static uint8_t      _count  = 0;
    static const Theme* _t      = nullptr;
    static char         _version[16] = {};

    static void _drawHeader(const Theme* t) {
        tft.fillRect(0, 0, 320, BOOT_HEADER_H, t->header);
        tft.setFont(&fonts::FreeSansBold9pt7b);
        tft.setTextColor(t->text);
        tft.setCursor(8, 6);
        char buf[32];
        snprintf(buf, sizeof(buf), "ACU RP  %s", _version);
        tft.print(buf);
        tft.drawFastHLine(0, BOOT_HEADER_H, 320, t->dim);
    }

    static void _drawLine(const Theme* t, int y,
                          uint16_t iconColor, const char* icon,
                          const char* msg) {
        tft.fillRect(0, y, 320, BOOT_LINE_H, t->bg);
        tft.setFont(BOOT_FONT);
        tft.setTextColor(iconColor);
        tft.setCursor(BOOT_ICON_X, y + BOOT_TEXT_Y);
        tft.print(icon);
        tft.setTextColor(t->text);
        tft.setCursor(BOOT_TEXT_X, y + BOOT_TEXT_Y);
        tft.print(msg);
    }

    static void _redraw(const Theme* t) {
        int startY = BOOT_HEADER_H + 2;
        tft.fillRect(0, startY, 320, BOOT_MAX_LINES * BOOT_LINE_H, t->bg);
        for (uint8_t i = 0; i < BOOT_MAX_LINES; i++) {
            if (_lines[i].used) {
                _drawLine(t, startY + i * BOOT_LINE_H,
                          _lines[i].color,
                          _lines[i].icon,
                          _lines[i].msg);
            }
        }
    }

    static void _addLine(const Theme* t,
                         uint16_t color, const char* icon,
                         const char* msg) {
        if (_count < BOOT_MAX_LINES) {
            uint8_t idx = _count;
            _lines[idx].color = color;
            _lines[idx].used  = true;
            strncpy(_lines[idx].icon, icon, sizeof(_lines[idx].icon) - 1);
            strncpy(_lines[idx].msg,  msg,  sizeof(_lines[idx].msg)  - 1);
            int y = (BOOT_HEADER_H + 2) + idx * BOOT_LINE_H;
            _drawLine(t, y, color, icon, msg);
            _count++;
        } else {
            // Posuň buffer o jeden nahoru
            for (uint8_t i = 0; i < BOOT_MAX_LINES - 1; i++) {
                _lines[i] = _lines[i + 1];
            }
            uint8_t last = BOOT_MAX_LINES - 1;
            _lines[last].color = color;
            _lines[last].used  = true;
            strncpy(_lines[last].icon, icon, sizeof(_lines[last].icon) - 1);
            strncpy(_lines[last].msg,  msg,  sizeof(_lines[last].msg)  - 1);
            _redraw(t);
        }
    }

    static void _updateLastLine(const Theme* t,
                                uint16_t color, const char* icon,
                                const char* msg) {
        uint8_t idx = (_count > 0 && _count <= BOOT_MAX_LINES) ?
                      _count - 1 : BOOT_MAX_LINES - 1;
        _lines[idx].color = color;
        strncpy(_lines[idx].icon, icon, sizeof(_lines[idx].icon) - 1);
        strncpy(_lines[idx].msg,  msg,  sizeof(_lines[idx].msg)  - 1);
        int y = (BOOT_HEADER_H + 2) + idx * BOOT_LINE_H;
        _drawLine(t, y, color, icon, msg);
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void begin(const Theme* t, const char* version) {
        _t     = t;
        _count = 0;
        memset(_lines, 0, sizeof(_lines));
        strncpy(_version, version, sizeof(_version) - 1);
        tft.fillScreen(t->bg);
        _drawHeader(t);
    }

    void print(const Theme* t, uint8_t status, const char* msg) {
        uint16_t color;
        const char* icon;
        switch (status) {
            case BOOT_OK:   color = t->ok;   icon = "[ OK ]"; break;
            case BOOT_ERR:  color = t->err;  icon = "[ERR] "; break;
            case BOOT_WARN: color = t->warn; icon = "[WARN]"; break;
            default:        color = t->dim;  icon = "[----]"; break;
        }
        _addLine(t, color, icon, msg);
        Serial.printf("%s %s\n", icon, msg);
    }

    bool wifiSta(const Theme* t, const char* ssid, uint32_t timeoutMs) {
        char buf[52];
        snprintf(buf, sizeof(buf), "WiFi STA %s...", ssid);
        _addLine(t, t->dim, "[ .. ]", buf);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > timeoutMs) break;
            uint32_t remaining = (timeoutMs - (millis() - start)) / 1000;
            snprintf(buf, sizeof(buf),
                     "WiFi STA %s... %2lus", ssid, remaining);
            _updateLastLine(t, t->dim, "[ .. ]", buf);
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            snprintf(buf, sizeof(buf), "WiFi STA %s",
                     WiFi.localIP().toString().c_str());
            _updateLastLine(t, t->ok, "[ OK ]", buf);
            Serial.printf("[ OK ] WiFi STA %s\n",
                          WiFi.localIP().toString().c_str());
            return true;
        } else {
            _updateLastLine(t, t->warn, "[WARN]", "WiFi STA nedostupne");
            Serial.println("[WARN] WiFi STA nedostupne");
            return false;
        }
    }

} // namespace BootScreen
