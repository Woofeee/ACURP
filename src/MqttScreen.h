// =============================================================
//  MqttScreen.h – konfigurace MQTT odesílání dat
//
//  Dostupné z: UdP → MQTT
//  Edituje: gConfig (MQTT broker, autentizace, topic, interval)
//
//  Sekce:
//    Pripojeni – Enable, Broker IP, Port
//    Overeni   – Username, Heslo
//    Data      – Topic prefix, Interval
//    Stav      – Připojeno, Posl. odeslání (readonly, live)
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru / změna hodnoty při editaci
//    CENTER   – vstup do editace / potvrzení
//    LEFT     – zpět do UDP / zrušení editace
//
//  Textové položky (Username, Heslo, Topic):
//    CENTER otevře fullscreen textový editor
//    Charset: A–Z, a–z, 0–9, mezera, . - _ @ / : ! # $
//
//  IP adresa – editace po oktetech (jako SerialScreen):
//    CENTER → vstup, L/R oktet, UP/DN hodnota, CENTER potvrdit
//
//  Geometrie:
//    Sdílený gContentSprite, pixelový scroll, FTR_Y ochrana.
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
#include "Config.h"

extern Config gConfig;

namespace MqttScreen {

    // ----------------------------------------------------------
    //  Definice položek
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        // Sekce: Připojení
        ITEM_ENABLE = 0,
        ITEM_BROKER_IP,
        ITEM_PORT,

        // Sekce: Ověření
        ITEM_USER,
        ITEM_PASS,

        // Sekce: Data
        ITEM_TOPIC,
        ITEM_INTERVAL,

        // Sekce: Stav (readonly)
        ITEM_STAT_CONN,
        ITEM_STAT_LAST,

        ITEM_COUNT
    };

    struct MqItem {
        const char* section;
        const char* label;
        char        value[20];
        bool        readonly;
        bool        isText;     // true = fullscreen textový editor
    };

    static MqItem _items[ITEM_COUNT] = {
        { "Pripojeni", "Enable",       "off",           false, false },
        { nullptr,     "Broker IP",    "0.0.0.0",       false, false },
        { nullptr,     "Port",         "1883",          false, false },
        { "Overeni",   "Username",     "",              false, true  },
        { nullptr,     "Heslo",        "",              false, true  },
        { "Data",      "Topic",        "solar/acu-rp",  false, true  },
        { nullptr,     "Interval",     "60 s",          false, false },
        { "Stav",      "Pripojeno",    "---",           true,  false },
        { nullptr,     "Posl. odesl",  "---",           true,  false },
    };

    #define MQ_ROW_H    26
    #define MQ_SEC_H    16
    #define MQ_START_Y  (CONTENT_Y + 4)

    static uint8_t _cursor       = 0;
    static uint8_t _scrollOffset = 0;
    static bool    _editing      = false;

    // Sdílený sprite z main_ui_loop
    static LGFX_Sprite* _spr = nullptr;
    void setSprite(LGFX_Sprite* s) { _spr = s; }

    // IP editace po oktetech
    static bool    _editingIp    = false;
    static uint8_t _ipField      = 0;
    static uint8_t _ipOctets[4]  = {};

    // Fullscreen textový editor
    static bool    _editingText  = false;

    // Charset pro textový editor
    static const char _charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .-_@/:!#$%&+=?";
    static const uint8_t _charsetLen = sizeof(_charset) - 1;

    #define MQ_TEXT_BUF_MAX  32
    static char    _textBuf[MQ_TEXT_BUF_MAX + 1] = {};
    static uint8_t _textMaxLen  = 0;
    static uint8_t _textPos     = 0;
    static uint8_t _textItem    = 0;

    // ----------------------------------------------------------
    //  Charset pomocné funkce
    // ----------------------------------------------------------
    static uint8_t _charsetFind(char c) {
        for (uint8_t i = 0; i < _charsetLen; i++) {
            if (_charset[i] == c) return i;
        }
        return 0;
    }

    static char _charNext(char c, int8_t dir) {
        uint8_t pos = _charsetFind(c);
        pos = (uint8_t)((pos + _charsetLen + dir) % _charsetLen);
        return _charset[pos];
    }

    static uint8_t _textLen() {
        int8_t last = -1;
        for (uint8_t i = 0; i < _textMaxLen; i++) {
            if (_textBuf[i] == '\0') break;
            if (_textBuf[i] != ' ')  last = i;
        }
        return (uint8_t)(last + 1);
    }

    // ----------------------------------------------------------
    //  Načti hodnoty z gConfig
    // ----------------------------------------------------------
    static void _loadFromConfig() {
        snprintf(_items[ITEM_ENABLE].value, 20, "%s",
            gConfig.mqttEn ? "on" : "off");
        snprintf(_items[ITEM_BROKER_IP].value, 20, "%u.%u.%u.%u",
            gConfig.mqttBrokerIp[0], gConfig.mqttBrokerIp[1],
            gConfig.mqttBrokerIp[2], gConfig.mqttBrokerIp[3]);
        snprintf(_items[ITEM_PORT].value, 20, "%u", gConfig.mqttPort);

        // Username – zobrazit nebo "---"
        if (gConfig.mqttUser[0] != '\0')
            strncpy(_items[ITEM_USER].value, gConfig.mqttUser,
                    sizeof(_items[0].value) - 1);
        else
            snprintf(_items[ITEM_USER].value, 20, "---");

        // Heslo – hvězdičky
        uint8_t passLen = (uint8_t)strlen(gConfig.mqttPass);
        if (passLen > 0) {
            passLen = min(passLen, (uint8_t)(sizeof(_items[0].value) - 1));
            memset(_items[ITEM_PASS].value, '*', passLen);
            _items[ITEM_PASS].value[passLen] = '\0';
        } else {
            snprintf(_items[ITEM_PASS].value, 20, "---");
        }

        strncpy(_items[ITEM_TOPIC].value, gConfig.mqttTopic,
                sizeof(_items[0].value) - 1);
        snprintf(_items[ITEM_INTERVAL].value, 20, "%u s",
            gConfig.mqttIntervalSec);
    }

    // ----------------------------------------------------------
    //  Aktualizuj živá data v sekci Stav
    // ----------------------------------------------------------
    static void _updateStatus() {
        // TODO: napojit na skutečný MQTT klient stav
        snprintf(_items[ITEM_STAT_CONN].value, 20, "---");
        snprintf(_items[ITEM_STAT_LAST].value, 20, "---");
    }

    // ----------------------------------------------------------
    //  Ulož editovanou hodnotu zpět do gConfig
    // ----------------------------------------------------------
    static void _saveItem(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_ENABLE:
                gConfig.mqttEn = (strcmp(_items[idx].value, "on") == 0);
                break;
            case ITEM_BROKER_IP:
                // Uloženo přímo při potvrzení IP editoru
                break;
            case ITEM_PORT:
                gConfig.mqttPort = (uint16_t)constrain(
                    atoi(_items[idx].value), 1, 65535);
                break;
            case ITEM_INTERVAL:
                gConfig.mqttIntervalSec = (uint16_t)constrain(
                    atoi(_items[idx].value), 10, 300);
                break;
            default: break;
        }
        // TODO: ConfigManager::saveToFram()
        Serial.printf("[MQTT_S] Uloženo: %s = %s\n",
            _items[idx].label, _items[idx].value);
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP/DOWN
    // ----------------------------------------------------------
    static void _stepUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_ENABLE:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            case ITEM_PORT: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 65535);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_INTERVAL: {
                int v = constrain(atoi(_items[idx].value) + 10, 10, 300);
                snprintf(_items[idx].value, 20, "%d s", v);
                break;
            }
            default: break;
        }
    }

    static void _stepDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_ENABLE:
                _stepUp(idx);  // jen 2 hodnoty
                break;
            case ITEM_PORT: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 65535);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_INTERVAL: {
                int v = constrain(atoi(_items[idx].value) - 10, 10, 300);
                snprintf(_items[idx].value, 20, "%d s", v);
                break;
            }
            default: break;
        }
    }

    // ==========================================================
    //  FULLSCREEN TEXTOVÝ EDITOR
    // ==========================================================
    static void _drawTextEditor(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);

        // Nadpis
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setCursor(16, CONTENT_Y + 6);
        tft.print(_items[_textItem].label);
        tft.drawFastHLine(16, CONTENT_Y + 20, 288, t->dim);

        // Pás znaků – ±4 kolem aktivního
        const int16_t STRIP_Y = CONTENT_Y + 26;
        const int16_t STRIP_H = 18;
        uint8_t curCharIdx = _charsetFind(_textBuf[_textPos]);

        const uint8_t STRIP_N = 9;
        const int16_t CHAR_W  = 28;
        const int16_t STRIP_X = (320 - STRIP_N * CHAR_W) / 2;

        tft.fillRect(0, STRIP_Y, 320, STRIP_H, t->bg);
        for (uint8_t k = 0; k < STRIP_N; k++) {
            int16_t cidx = (int16_t)curCharIdx - (STRIP_N / 2) + k;
            cidx = ((cidx % (int16_t)_charsetLen) + _charsetLen) % _charsetLen;
            char c = _charset[cidx];
            bool isActive = (k == STRIP_N / 2);
            int16_t cx = STRIP_X + k * CHAR_W;

            if (isActive) {
                tft.fillRect(cx, STRIP_Y, CHAR_W - 1, STRIP_H, t->header);
                tft.drawRect(cx, STRIP_Y, CHAR_W - 1, STRIP_H, t->accent);
            }

            char cs[2] = { c == ' ' ? '_' : c, '\0' };
            tft.setFont(&fonts::Font2);
            tft.setTextColor(isActive ? t->accent : t->dim);
            tft.setTextDatum(middle_center);
            tft.drawString(cs, cx + CHAR_W / 2, STRIP_Y + STRIP_H / 2);
        }
        tft.setTextDatum(top_left);

        // Šipky
        int16_t ax = STRIP_X + (STRIP_N / 2) * CHAR_W + CHAR_W / 2;
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setTextDatum(bottom_center);
        tft.drawString("^", ax, STRIP_Y - 1);
        tft.setTextDatum(top_center);
        tft.drawString("v", ax, STRIP_Y + STRIP_H + 1);
        tft.setTextDatum(top_left);

        // Boxy editovaného textu
        int16_t BOX_W, BOX_H;
        if (_textMaxLen <= 16) { BOX_W = 16; BOX_H = 28; }
        else if (_textMaxLen <= 24) { BOX_W = 12; BOX_H = 24; }
        else { BOX_W = 9; BOX_H = 22; }

        uint8_t row1Len = (_textMaxLen > 24) ? 24 : _textMaxLen;
        uint8_t row2Len = (_textMaxLen > 24) ? (_textMaxLen - 24) : 0;

        const int16_t BOX_Y1 = STRIP_Y + STRIP_H + 14;
        const int16_t BOX_Y2 = BOX_Y1 + BOX_H + 4;
        const int16_t BOX_START_X = (320 - row1Len * BOX_W) / 2;

        for (uint8_t i = 0; i < row1Len; i++) {
            int16_t bx = BOX_START_X + i * BOX_W;
            bool active = (i == _textPos);
            char c = (_textBuf[i] != '\0') ? _textBuf[i] : ' ';

            if (active) {
                tft.fillRect(bx, BOX_Y1, BOX_W - 1, BOX_H, t->header);
                tft.drawRect(bx, BOX_Y1, BOX_W - 1, BOX_H, t->accent);
            } else {
                tft.fillRect(bx, BOX_Y1, BOX_W - 1, BOX_H, t->bg);
                if (c != ' ') tft.drawRect(bx, BOX_Y1, BOX_W - 1, BOX_H, t->dim);
            }

            char cs[2] = { c, '\0' };
            if (BOX_W >= 12) tft.setFont(&fonts::Font2);
            else             tft.setFont(&fonts::Font0);
            tft.setTextColor(active ? t->accent : (c == ' ' ? t->dim : t->text));
            tft.setTextDatum(middle_center);
            tft.drawString(cs, bx + BOX_W / 2, BOX_Y1 + BOX_H / 2);
        }

        // Řádek 2
        if (row2Len > 0) {
            const int16_t BOX_START_X2 = (320 - row2Len * BOX_W) / 2;
            for (uint8_t i = 0; i < row2Len; i++) {
                uint8_t idx = 24 + i;
                int16_t bx = BOX_START_X2 + i * BOX_W;
                bool active = (idx == _textPos);
                char c = (_textBuf[idx] != '\0') ? _textBuf[idx] : ' ';

                if (active) {
                    tft.fillRect(bx, BOX_Y2, BOX_W - 1, BOX_H, t->header);
                    tft.drawRect(bx, BOX_Y2, BOX_W - 1, BOX_H, t->accent);
                } else {
                    tft.fillRect(bx, BOX_Y2, BOX_W - 1, BOX_H, t->bg);
                    if (c != ' ') tft.drawRect(bx, BOX_Y2, BOX_W - 1, BOX_H, t->dim);
                }

                char cs[2] = { c, '\0' };
                if (BOX_W >= 12) tft.setFont(&fonts::Font2);
                else             tft.setFont(&fonts::Font0);
                tft.setTextColor(active ? t->accent : (c == ' ' ? t->dim : t->text));
                tft.setTextDatum(middle_center);
                tft.drawString(cs, bx + BOX_W / 2, BOX_Y2 + BOX_H / 2);
            }
        }
        tft.setTextDatum(top_left);

        // Náhled
        int16_t previewY = (row2Len > 0) ? BOX_Y2 + BOX_H + 6 : BOX_Y1 + BOX_H + 6;
        if (previewY + 20 < FTR_Y) {
            tft.drawFastHLine(16, previewY, 288, t->dim);
            previewY += 4;
            uint8_t plen = _textLen();
            char preview[MQ_TEXT_BUF_MAX + 1] = {};
            strncpy(preview, _textBuf, plen);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(plen > 0 ? t->text : t->dim);
            tft.setTextDatum(top_center);
            tft.drawString(plen > 0 ? preview : "(prazdne)", 160, previewY);
            tft.setTextDatum(top_left);
        }

        // Footer
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN znak  L/R pozice  CENTER ok", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    static Screen _handleTextInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                _textBuf[_textPos] = _charNext(_textBuf[_textPos], +1);
                _drawTextEditor(t);
                return SCREEN_NONE;
            case SW_DOWN:
                _textBuf[_textPos] = _charNext(_textBuf[_textPos], -1);
                _drawTextEditor(t);
                return SCREEN_NONE;
            case SW_RIGHT:
                if (_textPos < _textMaxLen - 1) {
                    _textPos++;
                    _drawTextEditor(t);
                }
                return SCREEN_NONE;
            case SW_LEFT:
                if (_textPos > 0) {
                    _textPos--;
                    _drawTextEditor(t);
                } else {
                    _editingText = false;
                    _editing     = false;
                }
                return SCREEN_NONE;
            case SW_CENTER: {
                uint8_t plen = _textLen();

                // Propaguj do gConfig
                switch ((ItemId)_textItem) {
                    case ITEM_USER:
                        memset(gConfig.mqttUser, 0, sizeof(gConfig.mqttUser));
                        strncpy(gConfig.mqttUser, _textBuf,
                                min((uint8_t)(sizeof(gConfig.mqttUser) - 1), plen));
                        break;
                    case ITEM_PASS:
                        memset(gConfig.mqttPass, 0, sizeof(gConfig.mqttPass));
                        strncpy(gConfig.mqttPass, _textBuf,
                                min((uint8_t)(sizeof(gConfig.mqttPass) - 1), plen));
                        break;
                    case ITEM_TOPIC:
                        memset(gConfig.mqttTopic, 0, sizeof(gConfig.mqttTopic));
                        strncpy(gConfig.mqttTopic, _textBuf,
                                min((uint8_t)(sizeof(gConfig.mqttTopic) - 1), plen));
                        break;
                    default: break;
                }

                Serial.printf("[MQTT_S] %s = '%.*s'\n",
                    _items[_textItem].label, plen, _textBuf);
                _editingText = false;
                _editing     = false;
                return SCREEN_NONE;
            }
            default:
                return SCREEN_NONE;
        }
    }

    // ==========================================================
    //  IP EDITOR po oktetech
    // ==========================================================

    // Forward deklarace – definice níže
    static int16_t _drawItemAt(const Theme* t, uint8_t idx, int16_t y);
    static void    _drawAllItems(const Theme* t);
    static void    _drawItem(const Theme* t, uint8_t idx);

    static void _drawIpEditor(const Theme* t) {
        _drawAllItems(t);

        int16_t y = MQ_START_Y;
        for (uint8_t i = _scrollOffset; i < ITEM_BROKER_IP; i++) {
            if (_items[i].section != nullptr) y += MQ_SEC_H;
            y += MQ_ROW_H;
        }
        int16_t secH = (_items[ITEM_BROKER_IP].section != nullptr) ? MQ_SEC_H : 0;
        int16_t rowY = y + secH;

        LovyanGFX* dc = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t sy = _spr ? rowY - CONTENT_Y : rowY;

        dc->fillRect(8, sy, 304, MQ_ROW_H, t->header);
        dc->fillRect(8, sy, 3,   MQ_ROW_H, t->accent);

        dc->setFont(&fonts::Font2);
        dc->setTextColor(t->accent);
        dc->setCursor(20, sy + 7);
        dc->print("Broker IP");

        char parts[4][4];
        for (uint8_t i = 0; i < 4; i++)
            snprintf(parts[i], 4, "%u", _ipOctets[i]);

        const int16_t baseX = 180;
        int16_t cx = baseX;
        for (uint8_t i = 0; i < 4; i++) {
            bool act = (_ipField == i);
            uint8_t w = (uint8_t)strlen(parts[i]) * 8;

            dc->setTextColor(act ? t->accent : t->text);
            dc->setCursor(cx, sy + 7);
            dc->print(parts[i]);
            if (act)
                dc->fillRect(cx, sy + MQ_ROW_H - 4, w, 2, t->accent);
            cx += w;
            if (i < 3) {
                dc->setTextColor(t->dim);
                dc->setCursor(cx, sy + 7);
                dc->print(".");
                cx += 6;
            }
        }

        if (_spr) { _spr->pushSprite(0, CONTENT_Y); }
    }

    static Screen _handleIpInput(const Theme* t, SwButton btn) {
        switch (btn) {
            case SW_UP:
                _ipOctets[_ipField] = (_ipOctets[_ipField] + 1) % 256;
                _drawIpEditor(t);
                return SCREEN_NONE;
            case SW_DOWN:
                _ipOctets[_ipField] = (_ipOctets[_ipField] + 255) % 256;
                _drawIpEditor(t);
                return SCREEN_NONE;
            case SW_RIGHT:
                if (_ipField < 3) { _ipField++; _drawIpEditor(t); }
                return SCREEN_NONE;
            case SW_LEFT:
                if (_ipField > 0) {
                    _ipField--;
                    _drawIpEditor(t);
                } else {
                    _editingIp = false;
                    _editing   = false;
                    _loadFromConfig();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;
            case SW_CENTER:
                memcpy(gConfig.mqttBrokerIp, _ipOctets, 4);
                snprintf(_items[ITEM_BROKER_IP].value, 20, "%u.%u.%u.%u",
                    _ipOctets[0], _ipOctets[1], _ipOctets[2], _ipOctets[3]);
                _editingIp = false;
                _editing   = false;
                Serial.printf("[MQTT_S] Broker IP = %u.%u.%u.%u\n",
                    _ipOctets[0], _ipOctets[1], _ipOctets[2], _ipOctets[3]);
                _drawAllItems(t);
                return SCREEN_NONE;
            default:
                return SCREEN_NONE;
        }
    }

    // ----------------------------------------------------------
    //  Kreslení položky
    // ----------------------------------------------------------
    static int16_t _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        MqItem& item   = _items[idx];
        bool     active  = (idx == _cursor);
        bool     editing = (active && _editing && !_editingIp && !_editingText);
        int16_t  drawn   = 0;
        LovyanGFX* dc    = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t    sy    = _spr ? y - CONTENT_Y : y;

        // Sekce header
        if (item.section != nullptr) {
            if (y + MQ_SEC_H > FTR_Y) return 0;
            dc->fillRect(0, sy, 320, MQ_SEC_H, t->bg);
            dc->setFont(&fonts::Font2);
            dc->setTextColor(t->dim);
            dc->setCursor(16, sy);
            dc->print(item.section);
            sy += 14;
            dc->drawFastHLine(16, sy, 288, t->dim);
            sy += 2;
            drawn += MQ_SEC_H;
        }

        if (y + drawn + MQ_ROW_H > FTR_Y) return drawn;

        // Pozadí řádku
        if (active) {
            dc->fillRect(8, sy, 304, MQ_ROW_H, t->header);
            dc->fillRect(8, sy, 3,   MQ_ROW_H, t->accent);
        } else {
            dc->fillRect(8, sy, 304, MQ_ROW_H, t->bg);
        }

        // Label
        dc->setFont(&fonts::Font2);
        dc->setTextColor(item.readonly
            ? t->dim
            : (active ? t->accent : t->text));
        dc->setCursor(20, sy + 7);
        dc->print(item.label);

        // Hodnota
        if (item.isText) {
            // Textové položky – >>> hint
            dc->setTextColor(editing ? t->accent : t->text);
            dc->setTextDatum(middle_right);
            dc->drawString(item.value[0] && strcmp(item.value, "---") != 0
                           ? item.value : "---",
                           active ? 290 : 303, sy + 12);
            if (active) {
                dc->setTextColor(t->dim);
                dc->drawString(">>>", 308, sy + 12);
            }
            dc->setTextDatum(top_left);
        } else {
            uint16_t valCol = item.readonly ? t->dim
                            : (editing ? t->accent : t->text);
            dc->setTextColor(valCol);
            dc->setTextDatum(middle_right);
            dc->drawString(item.value, 308, sy + 12);
            dc->setTextDatum(top_left);
        }

        drawn += MQ_ROW_H;
        return drawn;
    }

    // ----------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ----------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        if (_spr) { _spr->fillScreen(t->bg); }
        else { tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg); }
        int16_t y = MQ_START_Y;
        for (uint8_t i = _scrollOffset; i < ITEM_COUNT; i++) {
            int16_t h = _drawItemAt(t, i, y);
            if (h == 0) break;
            y += h;
        }
        if (_spr) { _spr->pushSprite(0, CONTENT_Y); }
    }

    static void _drawItem(const Theme* t, uint8_t idx) {
        _drawAllItems(t);
    }

    // ----------------------------------------------------------
    //  Y pozice položky
    // ----------------------------------------------------------
    static int16_t _itemY(uint8_t idx) {
        int16_t y = MQ_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (_items[i].section != nullptr) y += MQ_SEC_H;
            y += MQ_ROW_H;
        }
        return y;
    }

    // ----------------------------------------------------------
    //  Scroll
    // ----------------------------------------------------------
    static bool _ensureVisible() {
        if (_cursor < _scrollOffset) {
            _scrollOffset = _cursor;
            return true;
        }
        bool changed = false;
        while (_scrollOffset < _cursor) {
            int16_t y      = _itemY(_cursor);
            int16_t secH   = (_items[_cursor].section != nullptr) ? MQ_SEC_H : 0;
            int16_t bottom = y + secH + MQ_ROW_H;
            if (bottom <= FTR_Y) break;
            _scrollOffset++;
            changed = true;
        }
        return changed;
    }

    // ----------------------------------------------------------
    //  Spodní lišta pro seznam
    // ----------------------------------------------------------
    static void _drawFooter(const Theme* t) {
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN  CENTER edit  LEFT zpet", 160, FTR_Y + 13);
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

        if (_editingText) {
            _drawTextEditor(t);
        } else {
            _drawFooter(t);
            _loadFromConfig();
            _updateStatus();
            _drawAllItems(t);
        }
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);

        if (!_editing && !_editingText) {
            _updateStatus();
            _drawAllItems(t);
        }
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        // --- Textový editor ---
        if (_editingText) {
            Screen ret = _handleTextInput(t, btn);
            if (!_editingText) {
                _loadFromConfig();
                _drawFooter(t);
                _drawAllItems(t);
            }
            return ret;
        }

        // --- IP editor ---
        if (_editingIp) {
            return _handleIpInput(t, btn);
        }

        // --- Editace běžné položky ---
        if (_editing) {
            switch (btn) {
                case SW_UP:
                    _stepUp(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_DOWN:
                    _stepDown(_cursor);
                    _drawItem(t, _cursor);
                    return SCREEN_NONE;
                case SW_CENTER:
                    _editing = false;
                    _saveItem(_cursor);
                    _loadFromConfig();
                    _drawAllItems(t);
                    return SCREEN_NONE;
                case SW_LEFT:
                    _editing = false;
                    _loadFromConfig();
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
                    _cursor--;
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_DOWN:
                if (_cursor < ITEM_COUNT - 1) {
                    _cursor++;
                    _ensureVisible();
                    _drawAllItems(t);
                }
                return SCREEN_NONE;

            case SW_CENTER:
                if (_items[_cursor].readonly) return SCREEN_NONE;

                // IP adresa – speciální editor
                if (_cursor == ITEM_BROKER_IP) {
                    _editingIp = true;
                    _editing   = true;
                    _ipField   = 0;
                    memcpy(_ipOctets, gConfig.mqttBrokerIp, 4);
                    _drawIpEditor(t);
                    return SCREEN_NONE;
                }

                // Textové položky – fullscreen editor
                if (_items[_cursor].isText) {
                    _textItem    = _cursor;
                    _textPos     = 0;
                    _editingText = true;
                    _editing     = true;

                    switch ((ItemId)_cursor) {
                        case ITEM_USER:
                            _textMaxLen = sizeof(gConfig.mqttUser) - 1;
                            strncpy(_textBuf, gConfig.mqttUser, _textMaxLen);
                            break;
                        case ITEM_PASS:
                            _textMaxLen = sizeof(gConfig.mqttPass) - 1;
                            strncpy(_textBuf, gConfig.mqttPass, _textMaxLen);
                            break;
                        case ITEM_TOPIC:
                            _textMaxLen = sizeof(gConfig.mqttTopic) - 1;
                            strncpy(_textBuf, gConfig.mqttTopic, _textMaxLen);
                            break;
                        default:
                            _textMaxLen = sizeof(_items[0].value) - 1;
                            strncpy(_textBuf, _items[_cursor].value, _textMaxLen);
                            break;
                    }
                    uint8_t srcLen = (uint8_t)strlen(_textBuf);
                    if (srcLen < _textMaxLen)
                        memset(_textBuf + srcLen, ' ', _textMaxLen - srcLen);
                    _textBuf[_textMaxLen] = '\0';

                    _drawTextEditor(t);
                    return SCREEN_NONE;
                }

                // Běžná editace
                _editing = true;
                _drawItem(t, _cursor);
                return SCREEN_NONE;

            case SW_LEFT:
                return SCREEN_UDP;

            default:
                return SCREEN_NONE;
        }
    }

    void reset() {
        _cursor       = 0;
        _scrollOffset = 0;
        _editing      = false;
        _editingIp    = false;
        _editingText  = false;
        _ipField      = 0;
        _textPos      = 0;
        _loadFromConfig();
    }

} // namespace MqttScreen