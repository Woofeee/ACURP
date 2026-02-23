// =============================================================
//  BootScreen.h - linux-style boot obrazovka se scrollovanim
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

// Font2 = 8x16 px bitmap
// BOOT_LINE_H    = krok mezi radky
// BOOT_TEXT_Y    = Y offset textu od zacatku radku (top-left pro Font2)
#define BOOT_FONT      &fonts::Font2
#define BOOT_LINE_H    20
#define BOOT_TEXT_Y     2              // 2px mezera nad textem
#define BOOT_ICON_X     4
#define BOOT_TEXT_X    60
#define BOOT_HEADER_H  28
#define BOOT_START_Y   (BOOT_HEADER_H + 2)
#define BOOT_BOTTOM    240

namespace BootScreen {

    static int          _y = BOOT_START_Y;
    static const Theme* _t = nullptr;

    static void _scroll() {
        tft.setScrollRect(0, BOOT_HEADER_H, 320,
                          BOOT_BOTTOM - BOOT_HEADER_H, _t->bg);
        tft.scroll(0, -BOOT_LINE_H);
        tft.fillRect(0, BOOT_BOTTOM - BOOT_LINE_H,
                     320, BOOT_LINE_H, _t->bg);
    }

    static int _nextLine() {
        if (_y + BOOT_LINE_H > BOOT_BOTTOM) {
            _scroll();
            _y = BOOT_BOTTOM - BOOT_LINE_H;
        }
        int y = _y;
        _y += BOOT_LINE_H;
        Serial.printf("[Boot] lineY=%d nextY=%d\n", y, _y);
        return y;
    }

    // Kresli radek – zadny setClipRect, jen fillRect + print
    static void _drawLine(const Theme* t, int lineY,
                          uint16_t iconColor, const char* icon,
                          const char* msg) {
        // Smaz presne tento radek
        tft.fillRect(0, lineY, 320, BOOT_LINE_H, t->bg);
        // Kresli text – Font2 pouziva setCursor jako top-left
        tft.setFont(BOOT_FONT);
        tft.setTextColor(iconColor);
        tft.setCursor(BOOT_ICON_X, lineY + BOOT_TEXT_Y);
        tft.print(icon);
        tft.setTextColor(t->text);
        tft.setCursor(BOOT_TEXT_X, lineY + BOOT_TEXT_Y);
        tft.print(msg);
    }

    void begin(const Theme* t, const char* version) {
        _t = t;
        _y = BOOT_START_Y;
        tft.fillScreen(t->bg);
        tft.fillRect(0, 0, 320, BOOT_HEADER_H, t->header);
        tft.setFont(&fonts::FreeSansBold9pt7b);
        tft.setTextColor(t->text);
        tft.setCursor(8, 6);
        char buf[32];
        snprintf(buf, sizeof(buf), "Solar HMI  %s", version);
        tft.print(buf);
        tft.drawFastHLine(0, BOOT_HEADER_H, 320, t->dim);
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
        int lineY = _nextLine();
        _drawLine(t, lineY, color, icon, msg);
        Serial.printf("%s %s\n", icon, msg);
    }

    bool wifiSta(const Theme* t, const char* ssid, uint32_t timeoutMs) {
        int lineY = _nextLine();
        uint32_t start = millis();
        char buf[52];

        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > timeoutMs) break;
            uint32_t remaining = (timeoutMs - (millis() - start)) / 1000;
            snprintf(buf, sizeof(buf), "WiFi STA %s... %2lus", ssid, remaining);
            _drawLine(t, lineY, t->dim, "[ .. ]", buf);
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            snprintf(buf, sizeof(buf), "WiFi STA %s",
                     WiFi.localIP().toString().c_str());
            _drawLine(t, lineY, t->ok, "[ OK ]", buf);
            Serial.printf("[ OK ] WiFi STA %s\n",
                          WiFi.localIP().toString().c_str());
            return true;
        } else {
            _drawLine(t, lineY, t->warn, "[WARN]", "WiFi STA nedostupne");
            Serial.println("[WARN] WiFi STA nedostupne");
            return false;
        }
    }

} // namespace BootScreen