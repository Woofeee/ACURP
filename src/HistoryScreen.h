// =============================================================
//  HistoryScreen.h – týdenní historie výroby / spotřeby
//
//  Layout:
//    Záložky: [VYROBA] [SPOTREBA]  (LEFT/RIGHT přepíná)
//    Sloupcový graf 7 dní – nejstarší vlevo, dnes vpravo (cyan)
//    Hodnota kWh nad sloupcem, zkratka dne pod sloupcem
//    Dole: Celkem / Průměr za 7 dní
//
//  Data: denní součty z FRAM (14 × float32 = 56 B)
//    Indexy 0–6  = výroba [kWh] (pondělí = 0, index 6 = dnes)
//    Indexy 7–13 = spotřeba [kWh]
//
//  LEFT  → přepni záložku (nebo zpět pokud na VYROBA)
//  RIGHT → přepni záložku
//  LEFT na VYROBA → SCREEN_MENU
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

// FRAM layout – denní data
#define FRAM_ADDR_HISTORY   0x0010  // začátek historie
// float32 výroba[7] + float32 spotreba[7] = 56 B

namespace HistoryScreen {

    // 0 = Výroba, 1 = Spotřeba
    static uint8_t _tab = 0;

    // Data pro zobrazení – načtená z FRAM
    static float _production[7]  = {};
    static float _consumption[7] = {};

    // Zkratky dnů (dnes = index 6, šest dní zpět)
    // Generováno dynamicky z RTC
    static char _dayLabels[7][4] = {};

    // ---------------------------------------------------------
    //  Generuj zkratky dnů (Po, Út, St... nebo Mo, Tu, We...)
    // ---------------------------------------------------------
    static const char* _dayAbbr(uint8_t wday) {
        // wday: 0=Ne, 1=Po, 2=Út, 3=St, 4=Čt, 5=Pá, 6=So
        static const char* names[] = {
            "Ne","Po","Ut","St","Ct","Pa","So"
        };
        return names[wday % 7];
    }

    // ---------------------------------------------------------
    //  Načti historii z FRAM
    // ---------------------------------------------------------
    void loadFromFRAM() {
        // TODO: napojit na FM24CL64 driver
        // Prozatím testovací data
        float testP[] = { 8.2f, 12.4f, 6.1f, 15.8f, 11.3f, 9.7f, 3.45f };
        float testC[] = { 5.1f, 7.8f,  4.3f,  9.2f,  6.5f, 5.8f, 2.10f };
        memcpy(_production,  testP, sizeof(testP));
        memcpy(_consumption, testC, sizeof(testC));
    }

    // ---------------------------------------------------------
    //  Vypočti den v týdnu z data (Zeller's congruence)
    //  Vrací: 0=Ne, 1=Po, 2=Út, 3=St, 4=Čt, 5=Pá, 6=So
    // ---------------------------------------------------------
    static uint8_t _calcWeekday(const DateTime& dt) {
        // Tomohiko Sakamoto algorithm
        static const int8_t t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
        uint16_t y = dt.year;
        uint8_t  m = dt.month;
        uint8_t  d = dt.day;
        if (m < 3) y--;
        return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    }

    // ---------------------------------------------------------
    //  Generuj popisky dnů relativně od dnes
    // ---------------------------------------------------------
    static void _buildDayLabels(const DateTime& dt) {
        uint8_t today = _calcWeekday(dt);
        for (int8_t i = 0; i < 7; i++) {
            int8_t day = (today - (6 - i) + 7) % 7;
            strncpy(_dayLabels[i], _dayAbbr(day), 3);
            _dayLabels[i][3] = '\0';
        }
        strncpy(_dayLabels[6], "dnes", 4);
        _dayLabels[6][3] = '\0'; // zkrat na "dne"
    }

    // ---------------------------------------------------------
    //  Nakresli záložky
    // ---------------------------------------------------------
    static void _drawTabs(const Theme* t) {
        const char* tabNames[] = { "VYROBA", "SPOTREBA" };
        uint16_t tabX[] = { 20, 100 };
        uint16_t tabW[] = { 70,  90 };

        for (uint8_t i = 0; i < 2; i++) {
            bool active = (i == _tab);
            tft.fillRect(tabX[i], CONTENT_Y + 4, tabW[i], 16,
                         active ? t->header : t->bg);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(tabX[i] + 4, CONTENT_Y + 6);
            tft.print(tabNames[i]);
            // Spodní linka aktivní záložky
            if (active) {
                tft.drawFastHLine(tabX[i], CONTENT_Y + 20, tabW[i], t->accent);
            }
        }
    }

    // ---------------------------------------------------------
    //  Nakresli sloupcový graf
    // ---------------------------------------------------------
    static void _drawChart(const Theme* t) {
        float* data = (_tab == 0) ? _production : _consumption;

        // Najdi maximum pro škálování
        float maxVal = 0.1f;
        for (uint8_t i = 0; i < 7; i++) {
            if (data[i] > maxVal) maxVal = data[i];
        }

        // Prostor grafu
        const int16_t chartX  = 12;
        const int16_t chartY  = CONTENT_Y + 28;
        const int16_t chartW  = 296;
        const int16_t chartH  = 110;
        const int16_t barW    = 30;
        const int16_t barGap  = 12;
        const int16_t totalW  = 7 * barW + 6 * barGap;
        const int16_t startX  = chartX + (chartW - totalW) / 2;

        // Čára základny
        tft.drawFastHLine(chartX, chartY + chartH, chartW, t->dim);

        for (uint8_t i = 0; i < 7; i++) {
            int16_t bx = startX + i * (barW + barGap);
            int16_t barH = (int16_t)(data[i] / maxVal * chartH);
            if (barH < 2) barH = 2;
            int16_t by = chartY + chartH - barH;

            // Barva: dnes = cyan, ostatní = solar žlutá @ 70% opacity
            // Na TFT simulujeme opacity tmavší barvou
            uint16_t col = (i == 6) ? t->accent : 0xD6A0; // žlutá tmavší

            tft.fillRect(bx, by, barW, barH, col);

            // Hodnota nad sloupcem
            char buf[8];
            snprintf(buf, sizeof(buf), "%.1f", data[i]);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(i == 6 ? t->accent : t->text);
            tft.setTextDatum(bottom_center);
            tft.drawString(buf, bx + barW / 2, by - 2);

            // Zkratka dne pod sloupcem
            tft.setTextColor(t->dim);
            tft.setTextDatum(top_center);
            tft.drawString(_dayLabels[i], bx + barW / 2, chartY + chartH + 3);
        }
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Nakresli souhrn (Celkem + Průměr)
    // ---------------------------------------------------------
    static void _drawSummary(const Theme* t) {
        float* data = (_tab == 0) ? _production : _consumption;

        float total = 0;
        for (uint8_t i = 0; i < 7; i++) total += data[i];
        float avg = total / 7.0f;

        char buf[32];
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);

        snprintf(buf, sizeof(buf), "Celkem: %.1f kWh", total);
        tft.setCursor(16, FTR_Y - 28);
        tft.print(buf);

        snprintf(buf, sizeof(buf), "Prumer: %.1f kWh/den", avg);
        tft.setCursor(16, FTR_Y - 14);
        tft.print(buf);
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);
        Header::drawFooter(t, d);

        _buildDayLabels(dt);
        _drawTabs(t);
        _drawChart(t);
        _drawSummary(t);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_LEFT:
                if (_tab == 0) return SCREEN_MENU;
                _tab = 0;
                // Překresli obsah bez záhlaví
                tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
                _drawTabs(t);
                _drawChart(t);
                _drawSummary(t);
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_tab < 1) {
                    _tab = 1;
                    tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
                    _drawTabs(t);
                    _drawChart(t);
                    _drawSummary(t);
                }
                return SCREEN_NONE;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() { _tab = 0; }

} // namespace HistoryScreen
