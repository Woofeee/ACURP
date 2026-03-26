// =============================================================
//  BoilerDetailScreen.h – konfigurace jednotlivého zásobníku
//
//  Editovatelné položky:
//    Label           – textový název (A–Z, 0–9, mezera)
//    Enable          – zásobník aktivní v systému
//    Povolený odběr  – allowedGridW [W]
//    Čas od          – timeStart [h] (0+0 = bez omezení)
//    Čas do          – timeEnd [h]
//
//  Readonly (výsledek Discovery):
//    Discovery: fáze L1/L2/L3, příkon [W]
//
//  Navigace – normální:
//    UP/DOWN        – pohyb kurzoru
//    LEFT           – předchozí zásobník / zpět do ControlScreen
//    RIGHT          – další zásobník
//    CENTER         – vstup do editace
//
//  Navigace – editace (ostatní položky):
//    UP/DOWN        – změna hodnoty
//    CENTER / LEFT  – potvrdit a zavřít
//
//  Navigace – editace labelu (fullscreen):
//    UP/DOWN        – změna znaku na aktivní pozici
//    LEFT / RIGHT   – pohyb po znacích
//    CENTER         – potvrdit celý label a zavřít
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
    };

    static uint8_t _boilerIdx    = 0;
    static uint8_t _cursor       = 0;
    static bool    _editing      = false;
    static bool    _editingLabel = false;  // fullscreen label editor

    // Pracovní kopie labelu při editaci (aby šlo zrušit)
    static char    _labelBuf[16] = {};
    static uint8_t _labelPos     = 0;     // aktivní pozice 0–15

    #define DETAIL_ROW_H    24
    #define DETAIL_START_Y  (CONTENT_Y + 30)

    // Délka labelu (bez null terminátor)
    #define LABEL_MAX       15

    // ----------------------------------------------------------
    //  Inicializace
    // ----------------------------------------------------------
    void begin(uint8_t boilerIdx) {
        _boilerIdx    = constrain(boilerIdx, 0, (uint8_t)(gConfig.numBoilers - 1));
        _cursor       = 0;
        _editing      = false;
        _editingLabel = false;
        _labelPos     = 0;
    }

    // ----------------------------------------------------------
    //  Charset pro label: mezera, A–Z, 0–9
    // ----------------------------------------------------------
    static char _nextChar(char c, int8_t dir) {
        const char* charset = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const uint8_t len   = 37;
        uint8_t pos = 0;
        for (uint8_t i = 0; i < len; i++) {
            if (charset[i] == c) { pos = i; break; }
        }
        pos = (pos + len + dir) % len;
        return charset[pos];
    }

    // ----------------------------------------------------------
    //  Efektivní délka labelu (bez trailing mezer)
    // ----------------------------------------------------------
    static uint8_t _labelLen(const char* s) {
        int8_t last = -1;
        for (uint8_t i = 0; i < LABEL_MAX; i++) {
            if (s[i] == '\0') break;
            if (s[i] != ' ')  last = i;
        }
        return (uint8_t)(last + 1);
    }

    // ==========================================================
    //  FULLSCREEN LABEL EDITOR
    // ==========================================================

    // Nakresli fullscreen editor labelu
    static void _drawLabelEditor(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);

        // Nadpis
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setCursor(16, CONTENT_Y + 6);
        tft.print("Nazev zasobniku");
        tft.drawFastHLine(16, CONTENT_Y + 20, 288, t->dim);

        // --- Zobrazení labelu se zvýrazněným znakem ---
        // Každý znak v boxu 18×32px, od x=8, y=CONTENT_Y+28
        // Vejde se 16 znaků × 18px = 288px → ok na 304px šířky obsahu
        const int16_t BOX_W  = 18;
        const int16_t BOX_H  = 32;
        const int16_t ROW_Y  = CONTENT_Y + 28;
        const int16_t START_X = 16;

        for (uint8_t i = 0; i < LABEL_MAX; i++) {
            int16_t bx = START_X + i * BOX_W;
            bool    active = (i == _labelPos);
            char    c = _labelBuf[i];
            if (c == '\0') c = ' ';

            // Pozadí boxu
            if (active) {
                tft.fillRect(bx, ROW_Y, BOX_W - 1, BOX_H, t->header);
                tft.drawRect(bx, ROW_Y, BOX_W - 1, BOX_H, t->accent);
            } else {
                tft.fillRect(bx, ROW_Y, BOX_W - 1, BOX_H, t->bg);
                // Jemný rámeček pro neprázdné znaky
                if (c != ' ') {
                    tft.drawRect(bx, ROW_Y, BOX_W - 1, BOX_H, t->dim);
                }
            }

            // Znak
            tft.setFont(&fonts::Font4);
            tft.setTextColor(active ? t->accent : (c == ' ' ? t->dim : t->text));
            tft.setTextDatum(middle_center);
            char cs[2] = { c, '\0' };
            tft.drawString(cs, bx + BOX_W / 2, ROW_Y + BOX_H / 2);
        }
        tft.setTextDatum(top_left);

        // --- Šipky nahoru/dolů vedle aktivního boxu ---
        int16_t ax = START_X + _labelPos * BOX_W + BOX_W / 2;
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setTextDatum(bottom_center);
        tft.drawString("^", ax, ROW_Y - 2);
        tft.setTextDatum(top_center);
        tft.drawString("v", ax, ROW_Y + BOX_H + 2);
        tft.setTextDatum(top_left);

        // --- Náhled výsledného textu ---
        int16_t previewY = ROW_Y + BOX_H + 20;
        tft.drawFastHLine(16, previewY, 288, t->dim);
        previewY += 6;

        char preview[LABEL_MAX + 1] = {};
        strncpy(preview, _labelBuf, LABEL_MAX);
        // Ořízni trailing mezery pro náhled
        uint8_t plen = _labelLen(_labelBuf);
        preview[plen] = '\0';

        tft.setFont(&fonts::Font4);
        tft.setTextColor(plen > 0 ? t->text : t->dim);
        tft.setTextDatum(top_center);
        tft.drawString(plen > 0 ? preview : "---", 160, previewY);
        tft.setTextDatum(top_left);

        // --- Spodní lišta ---
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN znak  L/R pozice  CENTER ok", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    // ----------------------------------------------------------
    //  Obsluha vstupu v label editoru
    // ----------------------------------------------------------
    static Screen _handleLabelInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                _labelBuf[_labelPos] = _nextChar(_labelBuf[_labelPos], +1);
                _drawLabelEditor(t);
                return SCREEN_NONE;

            case SW_DOWN:
                _labelBuf[_labelPos] = _nextChar(_labelBuf[_labelPos], -1);
                _drawLabelEditor(t);
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_labelPos < LABEL_MAX - 1) {
                    _labelPos++;
                    _drawLabelEditor(t);
                }
                return SCREEN_NONE;

            case SW_LEFT:
                if (_labelPos > 0) {
                    _labelPos--;
                    _drawLabelEditor(t);
                } else {
                    // Na první pozici – zruš editaci bez uložení
                    _editingLabel = false;
                    _editing      = false;
                }
                return SCREEN_NONE;

            case SW_CENTER:
                // Uložit label
                memset(gBoilerCfg[_boilerIdx].label, 0,
                       sizeof(gBoilerCfg[0].label));
                strncpy(gBoilerCfg[_boilerIdx].label,
                        _labelBuf, LABEL_MAX);
                // Ořízni trailing mezery
                for (int8_t i = LABEL_MAX - 1; i >= 0; i--) {
                    if (gBoilerCfg[_boilerIdx].label[i] == ' ')
                        gBoilerCfg[_boilerIdx].label[i] = '\0';
                    else
                        break;
                }
                Serial.printf("[BD] Label Byt %u: '%s'\n",
                    _boilerIdx + 1, gBoilerCfg[_boilerIdx].label);
                _editingLabel = false;
                _editing      = false;
                return SCREEN_NONE;  // signál pro draw() aby překreslil seznam

            default:
                return SCREEN_NONE;
        }
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP/DOWN pro ostatní položky
    // ----------------------------------------------------------
    static void _stepUp() {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        switch ((ItemId)_cursor) {
            case ITEM_ENABLE:
                cfg.enabled = !cfg.enabled; break;
            case ITEM_ALLOWED_GRID:
                cfg.allowedGridW = (uint16_t)constrain((int)cfg.allowedGridW + 100, 0, 3000); break;
            case ITEM_TIME_START:
                cfg.timeStart = (cfg.timeStart + 1) % 24; break;
            case ITEM_TIME_END:
                cfg.timeEnd = (cfg.timeEnd + 1) % 24; break;
            default: break;
        }
    }

    static void _stepDown() {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        switch ((ItemId)_cursor) {
            case ITEM_ENABLE:
                cfg.enabled = !cfg.enabled; break;
            case ITEM_ALLOWED_GRID:
                cfg.allowedGridW = (uint16_t)constrain((int)cfg.allowedGridW - 100, 0, 3000); break;
            case ITEM_TIME_START:
                cfg.timeStart = (cfg.timeStart + 23) % 24; break;
            case ITEM_TIME_END:
                cfg.timeEnd = (cfg.timeEnd + 23) % 24; break;
            default: break;
        }
    }

    // ==========================================================
    //  SEZNAM (normální view)
    // ==========================================================

    static void _drawTitle(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, 28, t->bg);

        tft.setFont(&fonts::Font2);
        tft.setTextColor(_boilerIdx > 0 ? t->accent : t->dim);
        tft.setCursor(8, CONTENT_Y + 8);
        tft.print("<");

        char title[24];
        snprintf(title, sizeof(title), "Byt %u – %s",
                 _boilerIdx + 1,
                 gBoilerCfg[_boilerIdx].label[0] != '\0'
                     ? gBoilerCfg[_boilerIdx].label : "---");
        tft.setTextColor(t->accent);
        tft.setTextDatum(top_center);
        tft.setFont(&fonts::Font4);
        tft.drawString(title, 160, CONTENT_Y + 4);
        tft.setTextDatum(top_left);

        tft.setFont(&fonts::Font2);
        tft.setTextColor(_boilerIdx < gConfig.numBoilers - 1 ? t->accent : t->dim);
        tft.setCursor(308, CONTENT_Y + 8);
        tft.print(">");

        char pos[8];
        snprintf(pos, sizeof(pos), "%u/%u", _boilerIdx + 1, gConfig.numBoilers);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(top_right);
        tft.drawString(pos, 308, CONTENT_Y + 18);
        tft.setTextDatum(top_left);

        tft.drawFastHLine(8, CONTENT_Y + 28, 304, t->dim);
    }

    static void _drawEditRow(const Theme* t, uint8_t idx, int16_t y) {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        bool active   = (idx == _cursor);
        bool editing  = (active && _editing);

        if (active) {
            tft.fillRect(8, y, 304, DETAIL_ROW_H, t->header);
            tft.fillRect(8, y, 3,   DETAIL_ROW_H, t->accent);
        } else {
            tft.fillRect(8, y, 304, DETAIL_ROW_H, t->bg);
        }

        tft.setFont(&fonts::Font2);

        const char* labels[] = { "Label", "Aktivni", "Povol. odber", "Cas od", "Cas do" };
        tft.setTextColor(active ? t->accent : t->text);
        tft.setCursor(20, y + 5);
        tft.print(labels[idx]);

        // Hodnota vpravo
        char val[20] = {};
        switch ((ItemId)idx) {
            case ITEM_LABEL:
                // Zobraz label nebo "---", šipka >>> naznačuje vstup do editoru
                tft.setTextColor(editing ? t->accent : t->text);
                tft.setTextDatum(middle_right);
                tft.drawString(
                    cfg.label[0] ? cfg.label : "---",
                    active ? 295 : 308, y + 12);
                if (active) {
                    tft.setTextColor(t->dim);
                    tft.drawString(">>>", 308, y + 12);
                }
                tft.setTextDatum(top_left);
                return;

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
            default: break;
        }

        tft.setTextColor(editing ? t->accent : t->text);
        tft.setTextDatum(middle_right);
        tft.drawString(val, 308, y + 12);
        tft.setTextDatum(top_left);
    }

    static void _drawDiscoveryInfo(const Theme* t) {
        BoilerConfig& cfg = gBoilerCfg[_boilerIdx];
        int16_t y = DETAIL_START_Y + ITEM_COUNT * DETAIL_ROW_H + 8;

        tft.drawFastHLine(8, y, 304, t->dim);
        y += 4;

        tft.setFont(&fonts::Font2);
        if (cfg.discoveryDone) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Discovery: L%u  %u W", cfg.phase, cfg.powerW);
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

    static void _drawRow(const Theme* t, uint8_t idx) {
        _drawEditRow(t, idx, DETAIL_START_Y + idx * DETAIL_ROW_H);
    }

    // Nakresli spodní lištu pro seznam
    static void _drawFooterList(const Theme* t) {
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("CENTER edit  LEFT/RIGHT byt  UP/DN hodnota", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    // ==========================================================
    //  Veřejné rozhraní
    // ==========================================================

    void draw(const Theme* t, const DateTime& dt,
              uint8_t apState, uint8_t staState, uint8_t invState,
              bool alarm, const SolarData& d) {

        tft.fillScreen(t->bg);
        Header::draw(t, dt, apState, staState, invState, alarm);

        if (_editingLabel) {
            _drawLabelEditor(t);
        } else {
            _drawFooterList(t);
            _drawContent(t);
        }
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        // --- Label editor (fullscreen) ---
        if (_editingLabel) {
            Screen ret = _handleLabelInput(t, btn);
            // Po potvrzení (CENTER) nebo zrušení (LEFT na pos 0) překresli seznam
            if (!_editingLabel) {
                _drawFooterList(t);
                _drawContent(t);
            }
            return ret;
        }

        // --- Editace ostatních položek ---
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
                case SW_CENTER:
                case SW_LEFT:
                    _editing = false;
                    Serial.printf("[BD] Byt %u uloženo\n", _boilerIdx + 1);
                    _drawRow(t, _cursor);
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

            case SW_CENTER:
                if (_cursor == ITEM_LABEL) {
                    // Otevři fullscreen label editor
                    memset(_labelBuf, ' ', LABEL_MAX);
                    _labelBuf[LABEL_MAX] = '\0';
                    // Načti stávající label do pracovního bufferu
                    uint8_t srcLen = (uint8_t)strlen(gBoilerCfg[_boilerIdx].label);
                    if (srcLen > LABEL_MAX) srcLen = LABEL_MAX;
                    memcpy(_labelBuf, gBoilerCfg[_boilerIdx].label, srcLen);
                    // Zbytek nech jako mezery (už nastaveno výše)
                    _labelPos     = 0;
                    _editingLabel = true;
                    _drawLabelEditor(t);
                } else {
                    _editing = true;
                    _drawRow(t, _cursor);
                }
                return SCREEN_NONE;

            case SW_LEFT:
                if (_boilerIdx > 0) {
                    _boilerIdx--;
                    _cursor  = 0;
                    _editing = false;
                    _drawContent(t);
                } else {
                    return SCREEN_CONTROL;
                }
                return SCREEN_NONE;

            case SW_RIGHT:
                if (_boilerIdx < gConfig.numBoilers - 1) {
                    _boilerIdx++;
                    _cursor  = 0;
                    _editing = false;
                    _drawContent(t);
                }
                return SCREEN_NONE;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _editing      = false;
        _editingLabel = false;
        _labelPos     = 0;
    }

} // namespace BoilerDetailScreen