// =============================================================
//  HistoryScreen.h – týdenní historie
//
//  3 záložky (LEFT/RIGHT přepíná):
//    [VYROBA]  [SPOTREBA]  [ZASOBNIKY]
//
//  VYROBA:
//    Sloupcový graf 7 dní – denní výroba FVE [kWh]
//    Dnes = cyan, ostatní = žlutá
//    Dole: Celkem / Průměr za 7 dní
//
//  SPOTREBA:
//    Sloupcový graf 7 dní – denní spotřeba domu [kWh]
//    Dnes = cyan, ostatní = žlutá
//    Dole: Celkem / Průměr
//
//  ZASOBNIKY:
//    Dvoubarevný sloupcový graf 7 dní:
//      Zelená (spodní) = ohřev ze soláru [kWh]
//      Červená (horní)  = ohřev ze sítě [kWh]
//    Dole: Celkem / Průměr / % solár
//
//  Data: DaySummary[7] z FRAM (zatím testovací)
//
//  LEFT na VYROBA → SCREEN_MENU
//  LEFT na jiné záložce → předchozí záložka
//  RIGHT → další záložka
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

namespace HistoryScreen {

    // ==========================================================
    //  Denní souhrn – 16B, 7 dní v FRAM (zatím RAM)
    // ==========================================================
    struct DaySummary {
        uint16_t pvWh;           // výroba FVE [Wh]
        uint16_t loadWh;         // spotřeba domu [Wh]
        uint16_t gridBuyWh;      // koupeno ze sítě [Wh]
        uint16_t gridSellWh;     // prodáno do sítě [Wh]
        uint16_t boilerSolarWh;  // zásobníky ze soláru [Wh]
        uint16_t boilerGridWh;   // zásobníky ze sítě [Wh]
        uint8_t  reserved[4];    // zarovnání na 16B
    };
    static_assert(sizeof(DaySummary) == 16, "DaySummary must be 16 bytes");

    #define HISTORY_DAYS  7

    // 0 = Výroba, 1 = Spotřeba, 2 = Zásobníky
    static uint8_t _tab = 0;

    // Data pro zobrazení – načtená z FRAM (zatím testovací)
    static DaySummary _days[HISTORY_DAYS] = {};

    // Zkratky dnů
    static char _dayLabels[HISTORY_DAYS][5] = {};

    // ---------------------------------------------------------
    //  Zkratky dnů
    // ---------------------------------------------------------
    static const char* _dayAbbr(uint8_t wday) {
        // wday: 0=Ne, 1=Po, 2=Út, 3=St, 4=Čt, 5=Pá, 6=So
        static const char* names[] = {
            "Ne","Po","Ut","St","Ct","Pa","So"
        };
        return names[wday % 7];
    }

    // ---------------------------------------------------------
    //  Tomohiko Sakamoto – den v týdnu z data
    //  Vrací: 0=Ne, 1=Po, 2=Út, 3=St, 4=Čt, 5=Pá, 6=So
    // ---------------------------------------------------------
    static uint8_t _calcWeekday(const DateTime& dt) {
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
        for (int8_t i = 0; i < HISTORY_DAYS; i++) {
            int8_t day = (today - (HISTORY_DAYS - 1 - i) + 7) % 7;
            strncpy(_dayLabels[i], _dayAbbr(day), 3);
            _dayLabels[i][3] = '\0';
        }
        // Poslední den = "dnes" (zkráceno na 3 znaky)
        strncpy(_dayLabels[HISTORY_DAYS - 1], "dne", 4);
    }

    // ---------------------------------------------------------
    //  Načti historii z FRAM
    // ---------------------------------------------------------
    void loadFromFRAM() {
        // TODO: napojit na FM24CL64 driver – DaySummary × 7 z FRAM
        // Prozatím testovací data
        DaySummary test[HISTORY_DAYS] = {
            {  8200, 5100, 1200, 4300, 3200,  400, {} },  // den -6
            { 12400, 7800, 2100, 6700, 5100,  800, {} },  // den -5
            {  6100, 4300,  900, 2700, 2400,  200, {} },  // den -4
            { 15800, 9200, 1800, 8400, 6800,  600, {} },  // den -3
            { 11300, 6500, 1500, 6300, 4900,  500, {} },  // den -2
            {  9700, 5800, 1100, 5000, 4100,  300, {} },  // den -1
            {  3450, 2100,  500, 1850, 1200,  100, {} },  // dnes
        };
        memcpy(_days, test, sizeof(test));
    }

    // ---------------------------------------------------------
    //  Nakresli záložky
    // ---------------------------------------------------------
    static void _drawTabs(const Theme* t) {
        const char* tabNames[] = { "VYROBA", "SPOTREBA", "ZASOBNIKY" };
        const uint16_t tabX[]  = { 10, 85, 185 };
        const uint16_t tabW[]  = { 65, 90, 100 };

        for (uint8_t i = 0; i < 3; i++) {
            bool active = (i == _tab);
            tft.fillRect(tabX[i], CONTENT_Y + 4, tabW[i], 16,
                         active ? t->header : t->bg);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(tabX[i] + 4, CONTENT_Y + 6);
            tft.print(tabNames[i]);
            if (active) {
                tft.drawFastHLine(tabX[i], CONTENT_Y + 20, tabW[i], t->accent);
            }
        }
    }

    // ---------------------------------------------------------
    //  Geometrie grafu – sdílená pro všechny záložky
    // ---------------------------------------------------------
    #define CHART_X      12
    #define CHART_Y      (CONTENT_Y + 28)
    #define CHART_W      296
    #define CHART_H      110
    #define BAR_W        30
    #define BAR_GAP      12
    #define TOTAL_BAR_W  (HISTORY_DAYS * BAR_W + (HISTORY_DAYS - 1) * BAR_GAP)
    #define BAR_START_X  (CHART_X + (CHART_W - TOTAL_BAR_W) / 2)

    // ---------------------------------------------------------
    //  Nakresli jednoduchý sloupcový graf (Výroba / Spotřeba)
    // ---------------------------------------------------------
    static void _drawSimpleChart(const Theme* t, const float data[HISTORY_DAYS]) {
        // Najdi maximum
        float maxVal = 0.1f;
        for (uint8_t i = 0; i < HISTORY_DAYS; i++) {
            if (data[i] > maxVal) maxVal = data[i];
        }

        // Čára základny
        tft.drawFastHLine(CHART_X, CHART_Y + CHART_H, CHART_W, t->dim);

        for (uint8_t i = 0; i < HISTORY_DAYS; i++) {
            int16_t bx = BAR_START_X + i * (BAR_W + BAR_GAP);
            int16_t barH = (int16_t)(data[i] / maxVal * CHART_H);
            if (barH < 2) barH = 2;
            int16_t by = CHART_Y + CHART_H - barH;

            // Barva: dnes = cyan, ostatní = solar žlutá
            uint16_t col = (i == HISTORY_DAYS - 1) ? t->accent : 0xD6A0;
            tft.fillRect(bx, by, BAR_W, barH, col);

            // Hodnota nad sloupcem
            char buf[8];
            snprintf(buf, sizeof(buf), "%.1f", data[i]);
            tft.setFont(&fonts::Font2);
            tft.setTextColor((i == HISTORY_DAYS - 1) ? t->accent : t->text);
            tft.setTextDatum(bottom_center);
            tft.drawString(buf, bx + BAR_W / 2, by - 2);

            // Zkratka dne pod sloupcem
            tft.setTextColor(t->dim);
            tft.setTextDatum(top_center);
            tft.drawString(_dayLabels[i], bx + BAR_W / 2, CHART_Y + CHART_H + 3);
        }
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Nakresli dvoubarevný sloupcový graf (Zásobníky)
    //  Zelená (spodní) = solár, Červená (horní) = síť
    // ---------------------------------------------------------
    static void _drawBoilerChart(const Theme* t) {
        // Data pro graf
        float solar[HISTORY_DAYS];
        float grid[HISTORY_DAYS];
        float total[HISTORY_DAYS];
        float maxVal = 0.1f;

        for (uint8_t i = 0; i < HISTORY_DAYS; i++) {
            solar[i] = _days[i].boilerSolarWh / 1000.0f;
            grid[i]  = _days[i].boilerGridWh  / 1000.0f;
            total[i] = solar[i] + grid[i];
            if (total[i] > maxVal) maxVal = total[i];
        }

        // Čára základny
        tft.drawFastHLine(CHART_X, CHART_Y + CHART_H, CHART_W, t->dim);

        for (uint8_t i = 0; i < HISTORY_DAYS; i++) {
            int16_t bx = BAR_START_X + i * (BAR_W + BAR_GAP);
            int16_t totalH = (int16_t)(total[i] / maxVal * CHART_H);
            if (totalH < 2) totalH = 2;

            int16_t solarH = (total[i] > 0)
                ? (int16_t)(solar[i] / total[i] * totalH)
                : totalH;
            int16_t gridH = totalH - solarH;

            int16_t byBottom = CHART_Y + CHART_H;

            // Zelená (solár) – spodní část
            if (solarH > 0) {
                tft.fillRect(bx, byBottom - solarH, BAR_W, solarH, t->ok);
            }
            // Červená (síť) – horní část
            if (gridH > 0) {
                tft.fillRect(bx, byBottom - totalH, BAR_W, gridH, t->err);
            }

            // Hodnota nad sloupcem
            char buf[8];
            snprintf(buf, sizeof(buf), "%.1f", total[i]);
            tft.setFont(&fonts::Font2);
            tft.setTextColor((i == HISTORY_DAYS - 1) ? t->accent : t->text);
            tft.setTextDatum(bottom_center);
            tft.drawString(buf, bx + BAR_W / 2, byBottom - totalH - 2);

            // Zkratka dne pod sloupcem
            tft.setTextColor(t->dim);
            tft.setTextDatum(top_center);
            tft.drawString(_dayLabels[i], bx + BAR_W / 2, CHART_Y + CHART_H + 3);
        }
        tft.setTextDatum(top_left);
    }

    // ---------------------------------------------------------
    //  Nakresli souhrn pod grafem
    // ---------------------------------------------------------
    static void _drawSummary(const Theme* t) {
        char buf[40];
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);

        if (_tab == 0) {
            // Výroba
            float total = 0;
            for (uint8_t i = 0; i < HISTORY_DAYS; i++)
                total += _days[i].pvWh / 1000.0f;
            float avg = total / HISTORY_DAYS;

            snprintf(buf, sizeof(buf), "Celkem: %.1f kWh", total);
            tft.setCursor(16, FTR_Y - 28);
            tft.print(buf);

            snprintf(buf, sizeof(buf), "Prumer: %.1f kWh/den", avg);
            tft.setCursor(16, FTR_Y - 14);
            tft.print(buf);

        } else if (_tab == 1) {
            // Spotřeba
            float total = 0;
            for (uint8_t i = 0; i < HISTORY_DAYS; i++)
                total += _days[i].loadWh / 1000.0f;
            float avg = total / HISTORY_DAYS;

            snprintf(buf, sizeof(buf), "Celkem: %.1f kWh", total);
            tft.setCursor(16, FTR_Y - 28);
            tft.print(buf);

            snprintf(buf, sizeof(buf), "Prumer: %.1f kWh/den", avg);
            tft.setCursor(16, FTR_Y - 14);
            tft.print(buf);

        } else {
            // Zásobníky
            float totalSolar = 0, totalGrid = 0;
            for (uint8_t i = 0; i < HISTORY_DAYS; i++) {
                totalSolar += _days[i].boilerSolarWh / 1000.0f;
                totalGrid  += _days[i].boilerGridWh  / 1000.0f;
            }
            float totalAll = totalSolar + totalGrid;
            float avg = totalAll / HISTORY_DAYS;
            uint8_t pctSolar = (totalAll > 0)
                ? (uint8_t)(totalSolar / totalAll * 100.0f + 0.5f)
                : 0;

            snprintf(buf, sizeof(buf), "Celkem: %.1f kWh  solar: %u%%",
                totalAll, pctSolar);
            tft.setCursor(16, FTR_Y - 28);
            tft.print(buf);

            snprintf(buf, sizeof(buf), "Prumer: %.1f kWh/den", avg);
            tft.setCursor(16, FTR_Y - 14);
            tft.print(buf);

            // Legenda
            tft.fillRect(250, FTR_Y - 28, 8, 8, t->ok);
            tft.setTextColor(t->dim);
            tft.setCursor(262, FTR_Y - 28);
            tft.print("sol");

            tft.fillRect(250, FTR_Y - 14, 8, 8, t->err);
            tft.setCursor(262, FTR_Y - 14);
            tft.print("sit");
        }
    }

    // ---------------------------------------------------------
    //  Nakresli graf podle aktivní záložky
    // ---------------------------------------------------------
    static void _drawChart(const Theme* t) {
        if (_tab == 0) {
            // Výroba
            float data[HISTORY_DAYS];
            for (uint8_t i = 0; i < HISTORY_DAYS; i++)
                data[i] = _days[i].pvWh / 1000.0f;
            _drawSimpleChart(t, data);

        } else if (_tab == 1) {
            // Spotřeba
            float data[HISTORY_DAYS];
            for (uint8_t i = 0; i < HISTORY_DAYS; i++)
                data[i] = _days[i].loadWh / 1000.0f;
            _drawSimpleChart(t, data);

        } else {
            // Zásobníky – dvoubarevný graf
            _drawBoilerChart(t);
        }
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);

        // Spodní lišta – navigace
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("LEFT/RIGHT zalozka  LEFT zpet", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);

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
                _tab--;
                tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
                _drawTabs(t);
                _drawChart(t);
                _drawSummary(t);
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_tab < 2) {
                    _tab++;
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