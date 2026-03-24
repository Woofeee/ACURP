// =============================================================
//  BoilerDetailScreen.h – konfigurace jednotlivého zásobníku
//
//  Zobrazuje a edituje konfiguraci jednoho zásobníku (bytu).
//  Přístup: ControlScreen → Zásobníky → výběr bytu
//
//  Editovatelné položky:
//    Label           – textový název (A–Z, 0–9, mezera)
//    Enable          – zásobník aktivní v systému
//    Povolený odběr  – allowedGridW [W]
//    Čas od          – timeStart [h] (0+0 = bez omezení)
//    Čas do          – timeEnd [h]
//
//  Readonly (výsledek Discovery):
//    Fáze            – L1/L2/L3 (změřeno)
//    Příkon          – [W] (změřeno)
//    Discovery stav  – hotovo / chybí
//
//  Navigace:
//    UP/DOWN   – pohyb kurzoru / změna hodnoty při editaci
//    LEFT      – předchozí zásobník (nebo zpět do ControlScreen)
//    RIGHT     – další zásobník
//    CENTER    – vstup do editace / potvrzení
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
#include "BoilerConfig.h"
#include "Config.h"

extern Config       gConfig;
extern BoilerConfig gBoilerCfg[BOILER_MAX_COUNT];

namespace BoilerDetailScreen {

    // ----------------------------------------------------------
    //  Položky editace
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        ITEM_LABEL = 0,
        ITEM_ENABLE,
        ITEM_ALLOWED_GRID,
        ITEM_TIME_START,
        ITEM_TIME_END,
        ITEM_COUNT,

        // Readonly sekce – nejsou v iteraci editace
        ITEM_DISC_PHASE,
        ITEM_DISC_POWER,
        ITEM_DISC_STATUS,
    };

    static uint8_t _boilerIdx   = 0;   // aktuálně zobrazený zásobník (0–9)
    static uint8_t _cursor      = 0;   // aktivní položka
    static bool    _editing     = false;

    // Pro editaci labelu – pozice znaku
    static uint8_t _labelCharPos = 0;

    #define DETAIL_ROW_H    24
    #define DETAIL_START_Y  (CONTENT_Y + 30)  // pod nadpisem zásobníku

    // ----------------------------------------------------------
    //  Inicializace – volej při přepnutí na tento screen
    // ----------------------------------------------------------
    void begin(uint8_t boilerIdx) {
        _boilerIdx   = constrain(boilerIdx, 0,
                                 (uint8_t)(gConfig.numBoilers - 1));
        _cursor      = 0;
        _editing     = false;
        _labelCharPos = 0;
    }

    // ----------------------------------------------------------
    //  Pomocné: sada povolených znaků pro label
    //  Navigace UP/DOWN prochází: A–Z, 0–9, mezera
    // ----------------------------------------------------------
    static char _nextChar(char c, int8_t dir) {
        // Sada: ' ', 'A'–'Z', '0'–'9'
        const char* charset = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const uint8_t len   = 37;

        // Najdi aktuální pozici
        uint8_t pos = 0;
        for (uint8_t i = 0; i < len; i++) {
            if (charset[i] == c) { pos = i; break; }
        }
        pos = (pos + len + dir) % len;
        return charset[pos];
    }

    // ----------------------------------------------------------
    //  Ulož změnu položky do gBoilerCfg[_boilerIdx]
    // ----------------------------------------------------------
    static void _saveItem(uint8_t idx) {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];

        switch ((ItemId)idx) {
            case ITEM_ENABLE:
                // toggle – provádí se přímo při stisku CENTER
                break;
            case ITEM_ALLOWED_GRID:
                // hodnota se mění průběžně – uložena v cfg přímo
                break;
            case ITEM_TIME_START:
            case ITEM_TIME_END:
                // hodnota se mění průběžně – uložena v cfg přímo
                break;
            case ITEM_LABEL:
                // label se mění průběžně – uložen v cfg přímo
                break;
            default:
                break;
        }

        // TODO: ConfigManager::saveBoilerCfg(_boilerIdx) až bude FRAM mapa
        Serial.printf("[BD] Byt %u uloženo\n", _boilerIdx + 1);
    }

    // ----------------------------------------------------------
    //  Krok UP při editaci
    // ----------------------------------------------------------
    static void _stepUp() {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        switch ((ItemId)_cursor) {
            case ITEM_LABEL:
                cfg.label[_labelCharPos] =
                    _nextChar(cfg.label[_labelCharPos], +1);
                break;
            case ITEM_ENABLE:
                cfg.enabled = !cfg.enabled;
                break;
            case ITEM_ALLOWED_GRID:
                cfg.allowedGridW = (uint16_t)constrain(
                    (int)cfg.allowedGridW + 100, 0, 3000);
                break;
            case ITEM_TIME_START:
                cfg.timeStart = (cfg.timeStart + 1) % 24;
                break;
            case ITEM_TIME_END:
                cfg.timeEnd = (cfg.timeEnd + 1) % 24;
                break;
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Krok DOWN při editaci
    // ----------------------------------------------------------
    static void _stepDown() {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        switch ((ItemId)_cursor) {
            case ITEM_LABEL:
                cfg.label[_labelCharPos] =
                    _nextChar(cfg.label[_labelCharPos], -1);
                break;
            case ITEM_ENABLE:
                cfg.enabled = !cfg.enabled;
                break;
            case ITEM_ALLOWED_GRID:
                cfg.allowedGridW = (uint16_t)constrain(
                    (int)cfg.allowedGridW - 100, 0, 3000);
                break;
            case ITEM_TIME_START:
                cfg.timeStart = (cfg.timeStart + 23) % 24;
                break;
            case ITEM_TIME_END:
                cfg.timeEnd = (cfg.timeEnd + 23) % 24;
                break;
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Nakresli nadpis zásobníku + navigační šipky
    // ----------------------------------------------------------
    static void _drawTitle(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, 28, t->bg);

        // Šipka vlevo (předchozí zásobník)
        tft.setFont(&fonts::Font2);
        tft.setTextColor(_boilerIdx > 0 ? t->accent : t->dim);
        tft.setCursor(8, CONTENT_Y + 8);
        tft.print("<");

        // Název zásobníku uprostřed
        char title[24];
        snprintf(title, sizeof(title), "Byt %u – %s",
                 _boilerIdx + 1,
                 gBoilerCfg[_boilerIdx].label[0] != '\0'
                     ? gBoilerCfg[_boilerIdx].label
                     : "---");
        tft.setTextColor(t->accent);
        tft.setTextDatum(top_center);
        tft.setFont(&fonts::Font4);
        tft.drawString(title, 160, CONTENT_Y + 4);
        tft.setTextDatum(top_left);

        // Šipka vpravo (další zásobník)
        tft.setFont(&fonts::Font2);
        tft.setTextColor(_boilerIdx < gConfig.numBoilers - 1 ? t->accent : t->dim);
        tft.setCursor(308, CONTENT_Y + 8);
        tft.print(">");

        // Indikátor pozice: "3 / 7"
        char pos[8];
        snprintf(pos, sizeof(pos), "%u/%u",
                 _boilerIdx + 1, gConfig.numBoilers);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(top_right);
        tft.drawString(pos, 308, CONTENT_Y + 18);
        tft.setTextDatum(top_left);

        // Oddělovací čára
        tft.drawFastHLine(8, CONTENT_Y + 28, 304, t->dim);
    }

    // ----------------------------------------------------------
    //  Nakresli jednu editovatelnou položku
    // ----------------------------------------------------------
    static void _drawEditRow(const Theme* t, uint8_t idx, int16_t y) {
        BoilerConfig& cfg  = gBoilerCfg[_boilerIdx];
        bool active        = (idx == _cursor);
        bool editing       = (active && _editing);

        // Pozadí
        if (active) {
            tft.fillRect(8, y, 304, DETAIL_ROW_H, t->header);
            tft.fillRect(8, y, 3,   DETAIL_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, DETAIL_ROW_H, t->bg);
        }

        tft.setFont(&fonts::Font2);

        // Label položky
        const char* labels[] = {
            "Label", "Aktivni", "Povol. odber", "Cas od", "Cas do"
        };
        tft.setTextColor(active ? t->accent : t->text);
        tft.setCursor(20, y + 5);
        tft.print(labels[idx]);

        // Hodnota
        char val[20];
        switch ((ItemId)idx) {
            case ITEM_LABEL:
                // Zobraz label se zvýrazněním aktivního znaku při editaci
                if (editing) {
                    // Levá část
                    char left[16] = {};
                    strncpy(left, cfg.label, _labelCharPos);
                    tft.setTextColor(t->text);
                    tft.setTextDatum(middle_right);
                    tft.drawString(left, 220, y + 12);

                    // Aktivní znak – cyan podtržení
                    char cur[2] = { cfg.label[_labelCharPos], '\0' };
                    tft.setTextColor(t->accent);
                    int16_t cx = 220 + (_labelCharPos * 6);  // přibližná X pozice
                    tft.drawString(cur, cx, y + 12);
                    tft.fillRect(cx - 3, y + DETAIL_ROW_H - 4, 8, 2, t->accent);

                    // Pravá část
                    tft.setTextColor(t->text);
                    tft.drawString(cfg.label + _labelCharPos + 1, cx + 8, y + 12);
                    tft.setTextDatum(top_left);
                } else {
                    tft.setTextColor(editing ? t->accent : t->text);
                    tft.setTextDatum(middle_right);
                    tft.drawString(cfg.label[0] ? cfg.label : "---", 308, y + 12);
                    tft.setTextDatum(top_left);
                }
                return;  // label má vlastní kreslení

            case ITEM_ENABLE:
                snprintf(val, sizeof(val), "%s", cfg.enabled ? "on" : "off");
                break;
            case ITEM_ALLOWED_GRID:
                if (cfg.allowedGridW == 0)
                    snprintf(val, sizeof(val), "0 W (jen solar)");
                else
                    snprintf(val, sizeof(val), "%u W", cfg.allowedGridW);
                break;
            case ITEM_TIME_START:
                if (cfg.timeStart == 0 && cfg.timeEnd == 0)
                    snprintf(val, sizeof(val), "0 h (vzdy)");
                else
                    snprintf(val, sizeof(val), "%u h", cfg.timeStart);
                break;
            case ITEM_TIME_END:
                if (cfg.timeStart == 0 && cfg.timeEnd == 0)
                    snprintf(val, sizeof(val), "0 h (vzdy)");
                else
                    snprintf(val, sizeof(val), "%u h", cfg.timeEnd);
                break;
            default:
                val[0] = '\0';
                break;
        }

        tft.setTextColor(editing ? t->accent : t->text);
        tft.setTextDatum(middle_right);
        tft.drawString(val, 308, y + 12);
        tft.setTextDatum(top_left);
    }

    // ----------------------------------------------------------
    //  Nakresli readonly sekci (výsledek Discovery)
    // ----------------------------------------------------------
    static void _drawDiscoveryInfo(const Theme* t) {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        int16_t y = DETAIL_START_Y + ITEM_COUNT * DETAIL_ROW_H + 8;

        tft.drawFastHLine(8, y, 304, t->dim);
        y += 4;

        tft.setFont(&fonts::Font2);

        if (cfg.discoveryDone) {
            // Fáze a příkon
            char buf[32];
            snprintf(buf, sizeof(buf), "Discovery: L%u  %u W",
                     cfg.phase, cfg.powerW);
            tft.setTextColor(t->ok);
            tft.setCursor(16, y);
            tft.print(buf);
        } else {
            tft.setTextColor(t->warn);
            tft.setCursor(16, y);
            tft.print("Discovery: nespusteno");
            tft.setTextColor(t->dim);
            tft.setCursor(16, y + 14);
            tft.print("UdP > Rizeni > Spustit");
        }
    }

    // ----------------------------------------------------------
    //  Překresli celý obsah (bez záhlaví)
    // ----------------------------------------------------------
    static void _drawContent(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);
        _drawTitle(t);

        int16_t y = DETAIL_START_Y;
        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            _drawEditRow(t, i, y);
            y += DETAIL_ROW_H;
        }

        _drawDiscoveryInfo(t);
    }

    // ----------------------------------------------------------
    //  Překresli jen jeden řádek
    // ----------------------------------------------------------
    static void _drawRow(const Theme* t, uint8_t idx) {
        int16_t y = DETAIL_START_Y + idx * DETAIL_ROW_H;
        _drawEditRow(t, idx, y);
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);

        // Spodní lišta – nápověda
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("CENTER edit  LEFT/RIGHT byt  UP/DN hodnota", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);

        _drawContent(t);
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        if (_editing) {
            switch (btn) {
                case SW_UP:
                    _stepUp();
                    _drawRow(t, _cursor);
                    return SCREEN_NONE;

                case SW_DOWN:
                    _stepDown();
                    _drawRow(t, _cursor);
                    return SCREEN_NONE;

                case SW_RIGHT:
                    // Při editaci labelu – přejdi na další znak
                    if (_cursor == ITEM_LABEL) {
                        uint8_t maxLen = (uint8_t)(sizeof(gBoilerCfg[0].label) - 1);
                        if (_labelCharPos < maxLen - 1) {
                            _labelCharPos++;
                            _drawRow(t, _cursor);
                        }
                    }
                    return SCREEN_NONE;

                case SW_LEFT:
                    // Při editaci labelu – předchozí znak nebo zrušení
                    if (_cursor == ITEM_LABEL && _labelCharPos > 0) {
                        _labelCharPos--;
                        _drawRow(t, _cursor);
                        return SCREEN_NONE;
                    }
                    // Jinak zruš editaci
                    _editing = false;
                    _drawRow(t, _cursor);
                    return SCREEN_NONE;

                case SW_CENTER:
                    // Potvrzení – ukonči editaci a ulož
                    _editing = false;
                    _saveItem(_cursor);
                    _drawRow(t, _cursor);
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
                    _drawRow(t, prev);
                    _drawRow(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    uint8_t prev = _cursor++;
                    _drawRow(t, prev);
                    _drawRow(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_LEFT:
                if (_boilerIdx > 0) {
                    // Přepni na předchozí zásobník
                    _boilerIdx--;
                    _cursor  = 0;
                    _editing = false;
                    _drawContent(t);
                } else {
                    // Jsme na prvním zásobníku → zpět do ControlScreen
                    return SCREEN_CONTROL;
                }
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_boilerIdx < gConfig.numBoilers - 1) {
                    // Přepni na další zásobník
                    _boilerIdx++;
                    _cursor  = 0;
                    _editing = false;
                    _drawContent(t);
                }
                return SCREEN_NONE;

            case SW_CENTER:
                // Vstup do editace
                _editing      = true;
                _labelCharPos = 0;
                _drawRow(t, _cursor);
                return SCREEN_NONE;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _editing      = false;
        _labelCharPos = 0;
        // _boilerIdx zachováme – nastaven přes begin()
    }

} // namespace BoilerDetailScreen
