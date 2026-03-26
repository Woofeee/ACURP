// =============================================================
//  DiagnosticScreen.h – diagnostické obrazovky
//
//  3 záložky (LEFT/RIGHT):
//    [I/O]  [HW Status]  [Alarmy]
//
//  I/O tab:
//    Vlevo: IO1–IO10 pulzní vstupy (puntíky)
//    Vpravo: R1–R10 výstupy relé (indikátory stavu)
//
//  HW Status: uptime, FRAM, RTC, Modbus stav – doladit
//  Alarmy:    seznam aktivních alarmů – doladit
//
//  LEFT → zpět do MENU (nebo předchozí záložka)
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

namespace DiagnosticScreen {

    static uint8_t _tab = 0;   // 0=I/O, 1=HW, 2=Alarmy

    // ---------------------------------------------------------
    //  Záložky
    // ---------------------------------------------------------
    static void _drawTabs(const Theme* t) {
        const char* names[] = { "I/O", "HW Status", "Alarmy" };
        const uint16_t xs[] = { 16, 70, 168 };
        const uint16_t ws[] = { 44, 88,  60 };

        for (uint8_t i = 0; i < 3; i++) {
            bool active = (i == _tab);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(active ? t->accent : t->dim);
            tft.setCursor(xs[i] + 4, CONTENT_Y + 6);
            tft.print(names[i]);
            if (active) {
                tft.drawFastHLine(xs[i], CONTENT_Y + 20, ws[i], t->accent);
            }
        }
    }

    // ---------------------------------------------------------
    //  I/O záložka
    // ---------------------------------------------------------
    static void _drawIO(const Theme* t, const SolarData& d) {
        // Dělicí čára uprostřed
        tft.drawFastVLine(160, CONTENT_Y + 24, CONTENT_H - 24, t->dim);

        // Hlavičky
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setCursor(16, CONTENT_Y + 26);
        tft.print("VSTUPY (pulz)");
        tft.setCursor(168, CONTENT_Y + 26);
        tft.print("VYSTUPY (rele)");

        // I/O řádky
        for (uint8_t i = 0; i < 10; i++) {
            int16_t y = CONTENT_Y + 42 + i * 14;

            // --- Vstupy IO1–IO10 ---
            // TODO: napojit na PulseCounter state
            bool inputActive = false; // placeholder

            tft.setTextColor(t->dim);
            tft.setCursor(16, y);
            char label[8];
            snprintf(label, sizeof(label), "IO%u", i + 1);
            tft.print(label);

            uint16_t dotCol = inputActive ? t->ok : t->dim;
            tft.fillCircle(52, y + 4, 3, dotCol);

            // --- Výstupy R1–R10 ---
            bool relayOn = d.relayOn[i];
            bool heating = d.relayHeating[i];

            snprintf(label, sizeof(label), "R%u", i + 1);
            tft.setTextColor(t->dim);
            tft.setCursor(168, y);
            tft.print(label);

            uint16_t rColor;
            if (!relayOn)    rColor = t->dim;
            else if (heating) rColor = t->err;
            else              rColor = t->ok;

            tft.fillRect(200, y, 12, 10, rColor);

            // Stav text
            tft.setTextColor(rColor);
            tft.setFont(&fonts::Font2);
            tft.setCursor(216, y);
            if (!relayOn)    tft.print("off");
            else if (heating) tft.print("ohrev");
            else              tft.print("ok");
        }
    }

    // ---------------------------------------------------------
    //  HW Status záložka (TODO: napojit na real data)
    // ---------------------------------------------------------
    static void _drawHW(const Theme* t, const SolarData& d) {
        tft.setFont(&fonts::Font2);

        auto row = [&](const char* label, const char* val,
                        uint16_t col, int16_t y) {
            tft.setTextColor(t->dim);
            tft.setCursor(16, y);
            tft.print(label);
            tft.setTextColor(col);
            tft.setCursor(180, y);
            tft.print(val);
        };

        uint32_t uptimeSec = millis() / 1000;
        char upBuf[20];
        snprintf(upBuf, sizeof(upBuf), "%lud %luh %02lum",
                 uptimeSec / 86400,
                 (uptimeSec % 86400) / 3600,
                 (uptimeSec % 3600) / 60);

        char mbuf[16];
        snprintf(mbuf, sizeof(mbuf), d.invOnline ? "OK (%ums)" : "OFFLINE",
                 d.lastUpdateMs ? (uint32_t)(millis() - d.lastUpdateMs) : 0u);

        row("Uptime",   upBuf,                       t->ok,  CONTENT_Y + 32);
        row("Modbus",   mbuf,  d.invOnline ? t->ok : t->err, CONTENT_Y + 50);
        row("Invertor", d.invOnline ? "OK" : "OFFLINE",
            d.invOnline ? t->ok : t->err,                    CONTENT_Y + 68);
        row("FRAM",     "OK",                        t->ok,  CONTENT_Y + 86);
        row("RTC",      "OK",                        t->ok,  CONTENT_Y + 104);
        row("Err.count", String(d.errorCount).c_str(),
            d.errorCount > 5 ? t->err : t->ok,              CONTENT_Y + 122);
    }

    // ---------------------------------------------------------
    //  Alarmy záložka (TODO: AlarmManager)
    // ---------------------------------------------------------
    static void _drawAlarms(const Theme* t) {
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setCursor(16, CONTENT_Y + 50);
        tft.print("Zadne aktivni alarmy");

        // TODO: AlarmManager::getList() → zobrazit seznam
    }

    // ---------------------------------------------------------
    //  Překresli obsah aktivní záložky
    // ---------------------------------------------------------
    static void _drawContent(const Theme* t, const SolarData& d) {
        tft.fillRect(0, CONTENT_Y + 24, 320, CONTENT_H - 24, t->bg);
        switch (_tab) {
            case 0: _drawIO(t, d);     break;
            case 1: _drawHW(t, d);     break;
            case 2: _drawAlarms(t);    break;
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

        _drawTabs(t);
        _drawContent(t, d);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
        // I/O tab – živá data
        if (_tab == 0 || _tab == 1) {
            _drawContent(t, d);
        }
    }

    Screen handleInput(const Theme* t, SwButton btn, const SolarData& d) {
        switch (btn) {
            case SW_LEFT:
                if (_tab == 0) return SCREEN_MENU;
                _tab--;
                _drawTabs(t);
                _drawContent(t, d);
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_tab < 2) {
                    _tab++;
                    _drawTabs(t);
                    _drawContent(t, d);
                }
                return SCREEN_NONE;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() { _tab = 0; }

} // namespace DiagnosticScreen