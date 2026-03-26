// =============================================================
//  NetworkScreen.h – konfigurace sítě
//
//  Dostupné z: UdP → Network
//  Edituje: gConfig (WiFi STA, WiFi AP, NTP, Hostname)
//
//  Sekce:
//    WiFi STA  – Enable, SSID, Heslo, DHCP, IP, Maska, GW, DNS
//    WiFi AP   – Enable, SSID, Heslo, Kanal, Skryty, IP, DHCP od, DHCP do
//    NTP       – Enable, Server, Timezone, Resync
//    Ostatni   – Hostname
//
//  Navigace:
//    UP/DOWN  – pohyb kurzoru / změna hodnoty při editaci
//    CENTER   – vstup do editace / potvrzení
//    LEFT     – zpět do UDP / zrušení editace
//
//  Textové položky (SSID, Heslo, Server, TZ, Hostname):
//    CENTER otevře fullscreen textový editor
//    Charset: A–Z, a–z, 0–9, mezera, . - _ @ / : ! # $
//
//  Geometrie:
//    Stejná jako ControlScreen – pixelový scroll, FTR_Y ochrana
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

extern Config        gConfig;
extern volatile bool gWifiSta;
extern volatile bool gWifiAp;
extern volatile bool gNtpResync;

namespace NetworkScreen {

    // ----------------------------------------------------------
    //  Definice položek
    // ----------------------------------------------------------
    enum ItemId : uint8_t {
        // Sekce: WiFi STA
        ITEM_STA_EN = 0,
        ITEM_STA_SSID,
        ITEM_STA_PASS,
        ITEM_STA_DHCP,
        ITEM_STA_IP,
        ITEM_STA_MASK,
        ITEM_STA_GW,
        ITEM_STA_DNS,

        // Sekce: WiFi AP
        ITEM_AP_EN,
        ITEM_AP_SSID,
        ITEM_AP_PASS,
        ITEM_AP_CHANNEL,
        ITEM_AP_HIDDEN,
        ITEM_AP_IP,
        ITEM_AP_DHCP_START,
        ITEM_AP_DHCP_END,

        // Sekce: NTP
        ITEM_NTP_EN,
        ITEM_NTP_SERVER,
        ITEM_NTP_TZ,
        ITEM_NTP_RESYNC,

        // Sekce: Ostatni
        ITEM_HOSTNAME,

        ITEM_COUNT
    };

    struct NetItem {
        const char* section;   // nullptr = pokračování předchozí sekce
        const char* label;
        char        value[20];
        bool        isText;    // true = otevře fullscreen textový editor
    };

    static NetItem _items[ITEM_COUNT] = {
        { "WiFi STA",  "Enable",    "off",            false },
        { nullptr,     "SSID",      "",               true  },
        { nullptr,     "Heslo",     "",               true  },
        { nullptr,     "DHCP",      "on",             false },
        { nullptr,     "IP",        "0.0.0.0",        false },
        { nullptr,     "Maska",     "255.255.255.0",  false },
        { nullptr,     "Gateway",   "0.0.0.0",        false },
        { nullptr,     "DNS",       "8.8.8.8",        false },
        { "WiFi AP",   "Enable",    "off",            false },
        { nullptr,     "SSID",      "ACU-RP",         true  },
        { nullptr,     "Heslo",     "",               true  },
        { nullptr,     "Kanal",     "6",              false },
        { nullptr,     "Skryty",    "ne",             false },
        { nullptr,     "IP",        "192.168.4.1",    false },
        { nullptr,     "DHCP od",   "192.168.4.2",    false },
        { nullptr,     "DHCP do",   "192.168.4.10",   false },
        { "NTP",       "Enable",    "on",             false },
        { nullptr,     "Server",    "pool.ntp.org",   true  },
        { nullptr,     "Timezone",  "CET-1CEST,...",  true  },
        { nullptr,     "Resync",    "6 h",            false },
        { "Ostatni",   "Hostname",  "ACU-RP",         true  },
    };

    #define NET_ROW_H    26
    #define NET_SEC_H    16
    #define NET_START_Y  (CONTENT_Y + 4)

    static uint8_t _cursor       = 0;
    static uint8_t _scrollOffset = 0;
    static bool    _editing      = false;

    // Sdílený sprite z main_ui_loop – nastav přes setSprite()
    static LGFX_Sprite* _spr = nullptr;
    static void setSprite(LGFX_Sprite* s) { _spr = s; }
    static bool    _editingText  = false;  // fullscreen textový editor

    // ----------------------------------------------------------
    //  TEXTOVÝ EDITOR – fullscreen, znak po znaku
    //  Charset: A–Z, a–z, 0–9, mezera, speciální znaky
    // ----------------------------------------------------------

    // Charset – skupiny pro přehlednost v editoru
    // Řádek 0: A–Z (26)
    // Řádek 1: a–z (26)
    // Řádek 2: 0–9 + speciální (10 + 12 = 22)
    static const char _charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789 .-_@/:!#$%&+=?";
    static const uint8_t _charsetLen = sizeof(_charset) - 1;  // bez '\0'

    // Pracovní buffer pro editaci textu
    #define TEXT_BUF_MAX  48   // max délka editovaného pole
    static char    _textBuf[TEXT_BUF_MAX + 1] = {};
    static uint8_t _textMaxLen  = 0;   // max délka aktuálního pole
    static uint8_t _textPos     = 0;   // aktivní pozice
    static uint8_t _textItem    = 0;   // idx položky která se edituje

    // Najdi pozici znaku v charsetu
    static uint8_t _charsetFind(char c) {
        for (uint8_t i = 0; i < _charsetLen; i++) {
            if (_charset[i] == c) return i;
        }
        return 0;  // nenalezeno → první znak
    }

    static char _charNext(char c, int8_t dir) {
        uint8_t pos = _charsetFind(c);
        pos = (uint8_t)((pos + _charsetLen + dir) % _charsetLen);
        return _charset[pos];
    }

    // Efektivní délka bufferu (bez trailing mezer/nul)
    static uint8_t _textLen() {
        int8_t last = -1;
        for (uint8_t i = 0; i < _textMaxLen; i++) {
            if (_textBuf[i] == '\0') break;
            if (_textBuf[i] != ' ')  last = i;
        }
        return (uint8_t)(last + 1);
    }

    // Nakresli fullscreen textový editor
    static void _drawTextEditor(const Theme* t) {
        tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg);

        // Nadpis – název položky
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setCursor(16, CONTENT_Y + 6);
        tft.print(_items[_textItem].label);
        tft.drawFastHLine(16, CONTENT_Y + 20, 288, t->dim);

        // --- Charset řádky jako nápověda ---
        // Zobrazíme aktuální znak + kontext ±3 v pásu
        const int16_t STRIP_Y = CONTENT_Y + 26;
        const int16_t STRIP_H = 18;

        uint8_t curCharIdx = _charsetFind(_textBuf[_textPos]);

        // Pás 9 znaků: curCharIdx-4 .. curCharIdx+4
        tft.fillRect(0, STRIP_Y, 320, STRIP_H, t->bg);
        const uint8_t STRIP_N = 9;
        const int16_t CHAR_W  = 28;
        const int16_t STRIP_X = (320 - STRIP_N * CHAR_W) / 2;

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

        // Šipky nad/pod aktivním znakem
        int16_t ax = STRIP_X + (STRIP_N / 2) * CHAR_W + CHAR_W / 2;
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->accent);
        tft.setTextDatum(bottom_center);
        tft.drawString("^", ax, STRIP_Y - 1);
        tft.setTextDatum(top_center);
        tft.drawString("v", ax, STRIP_Y + STRIP_H + 1);
        tft.setTextDatum(top_left);

        // --- Boxy editovaného textu ---
        // Šířka boxu závisí na délce pole:
        //   maxLen ≤ 16  → BOX_W = 16px (vejde se 16 × 16 = 256px)
        //   maxLen ≤ 24  → BOX_W = 12px (24 × 12 = 288px)
        //   maxLen ≤ 32  → BOX_W = 9px  (32 × 9 = 288px)
        //   maxLen ≤ 48  → BOX_W = 6px  (48 × 6 = 288px)

        uint8_t boxLen;   // kolik boxů na jeden řádek
        int16_t BOX_W, BOX_H;

        if (_textMaxLen <= 16) {
            BOX_W = 16; BOX_H = 28; boxLen = _textMaxLen;
        } else if (_textMaxLen <= 24) {
            BOX_W = 12; BOX_H = 24; boxLen = _textMaxLen;
        } else if (_textMaxLen <= 32) {
            BOX_W = 9;  BOX_H = 22; boxLen = _textMaxLen;
        } else {
            BOX_W = 6;  BOX_H = 20; boxLen = _textMaxLen;
        }

        // Dva řádky boxů pokud se nevejdou na jeden
        uint8_t row1Len = (boxLen > 24) ? 24 : boxLen;
        uint8_t row2Len = (boxLen > 24) ? (boxLen - 24) : 0;

        const int16_t BOX_Y1 = STRIP_Y + STRIP_H + 14;
        const int16_t BOX_Y2 = BOX_Y1 + BOX_H + 4;
        const int16_t BOX_START_X = (320 - row1Len * BOX_W) / 2;

        // Kreslení boxů – řádek 1
        for (uint8_t i = 0; i < row1Len; i++) {
            int16_t bx   = BOX_START_X + i * BOX_W;
            int16_t by   = BOX_Y1;
            bool    active = (i == _textPos);
            char    c = (_textBuf[i] != '\0') ? _textBuf[i] : ' ';

            if (active) {
                tft.fillRect(bx, by, BOX_W - 1, BOX_H, t->header);
                tft.drawRect(bx, by, BOX_W - 1, BOX_H, t->accent);
            } else {
                tft.fillRect(bx, by, BOX_W - 1, BOX_H, t->bg);
                if (c != ' ') tft.drawRect(bx, by, BOX_W - 1, BOX_H, t->dim);
            }

            char cs[2] = { c, '\0' };
            if (BOX_W >= 12) tft.setFont(&fonts::Font2);
            else             tft.setFont(&fonts::Font0);
            tft.setTextColor(active ? t->accent : (c == ' ' ? t->dim : t->text));
            tft.setTextDatum(middle_center);
            tft.drawString(cs, bx + BOX_W / 2, by + BOX_H / 2);
        }

        // Kreslení boxů – řádek 2 (pro dlouhá pole ≥ 25 znaků)
        if (row2Len > 0) {
            const int16_t BOX_START_X2 = (320 - row2Len * BOX_W) / 2;
            for (uint8_t i = 0; i < row2Len; i++) {
                uint8_t idx  = 24 + i;
                int16_t bx   = BOX_START_X2 + i * BOX_W;
                int16_t by   = BOX_Y2;
                bool    active = (idx == _textPos);
                char    c = (_textBuf[idx] != '\0') ? _textBuf[idx] : ' ';

                if (active) {
                    tft.fillRect(bx, by, BOX_W - 1, BOX_H, t->header);
                    tft.drawRect(bx, by, BOX_W - 1, BOX_H, t->accent);
                } else {
                    tft.fillRect(bx, by, BOX_W - 1, BOX_H, t->bg);
                    if (c != ' ') tft.drawRect(bx, by, BOX_W - 1, BOX_H, t->dim);
                }

                char cs[2] = { c, '\0' };
                if (BOX_W >= 12) tft.setFont(&fonts::Font2);
                else             tft.setFont(&fonts::Font0);
                tft.setTextColor(active ? t->accent : (c == ' ' ? t->dim : t->text));
                tft.setTextDatum(middle_center);
                tft.drawString(cs, bx + BOX_W / 2, by + BOX_H / 2);
            }
        }

        tft.setTextDatum(top_left);

        // --- Náhled výsledného textu ---
        int16_t previewY = (row2Len > 0) ? BOX_Y2 + BOX_H + 6 : BOX_Y1 + BOX_H + 6;
        if (previewY + 20 < FTR_Y) {
            tft.drawFastHLine(16, previewY, 288, t->dim);
            previewY += 4;
            uint8_t plen = _textLen();
            char preview[TEXT_BUF_MAX + 1] = {};
            strncpy(preview, _textBuf, plen);
            tft.setFont(&fonts::Font2);
            tft.setTextColor(plen > 0 ? t->text : t->dim);
            tft.setTextDatum(top_center);
            tft.drawString(plen > 0 ? preview : "(prazdne)", 160, previewY);
            tft.setTextDatum(top_left);
        }

        // --- Spodní lišta ---
        tft.fillRect(0, FTR_Y, 320, FTR_H, t->header);
        tft.setFont(&fonts::Font2);
        tft.setTextColor(t->dim);
        tft.setTextDatum(middle_center);
        tft.drawString("UP/DN znak  L/R pozice  CENTER ok", 160, FTR_Y + 13);
        tft.setTextDatum(top_left);
    }

    // Obsluha vstupu v textovém editoru
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
                    // Na první pozici – zruš editaci bez uložení
                    _editingText = false;
                }
                return SCREEN_NONE;

            case SW_CENTER: {
                // Ulož do _items a do gConfig
                // Ořízni trailing mezery
                uint8_t plen = _textLen();
                memset(_items[_textItem].value, 0, sizeof(_items[0].value));
                strncpy(_items[_textItem].value,
                        _textBuf,
                        min((uint8_t)(sizeof(_items[0].value) - 1), plen));

                // Propaguj do gConfig
                switch ((ItemId)_textItem) {
                    case ITEM_STA_SSID:
                        strncpy(gConfig.wifiStaSsid, _textBuf,
                                sizeof(gConfig.wifiStaSsid) - 1);
                        gConfig.wifiStaSsid[plen < sizeof(gConfig.wifiStaSsid) - 1
                                            ? plen : sizeof(gConfig.wifiStaSsid) - 1] = '\0';
                        break;
                    case ITEM_STA_PASS:
                        strncpy(gConfig.wifiStaPass, _textBuf,
                                sizeof(gConfig.wifiStaPass) - 1);
                        gConfig.wifiStaPass[min((uint8_t)(sizeof(gConfig.wifiStaPass)-1), plen)] = '\0';
                        break;
                    case ITEM_AP_SSID:
                        strncpy(gConfig.wifiApSsid, _textBuf,
                                sizeof(gConfig.wifiApSsid) - 1);
                        gConfig.wifiApSsid[min((uint8_t)(sizeof(gConfig.wifiApSsid)-1), plen)] = '\0';
                        break;
                    case ITEM_AP_PASS:
                        strncpy(gConfig.wifiApPass, _textBuf,
                                sizeof(gConfig.wifiApPass) - 1);
                        gConfig.wifiApPass[min((uint8_t)(sizeof(gConfig.wifiApPass)-1), plen)] = '\0';
                        break;
                    case ITEM_NTP_SERVER:
                        strncpy(gConfig.ntpServer, _textBuf,
                                sizeof(gConfig.ntpServer) - 1);
                        gConfig.ntpServer[min((uint8_t)(sizeof(gConfig.ntpServer)-1), plen)] = '\0';
                        break;
                    case ITEM_NTP_TZ:
                        strncpy(gConfig.ntpTz, _textBuf,
                                sizeof(gConfig.ntpTz) - 1);
                        gConfig.ntpTz[min((uint8_t)(sizeof(gConfig.ntpTz)-1), plen)] = '\0';
                        break;
                    case ITEM_HOSTNAME:
                        strncpy(gConfig.hostname, _textBuf,
                                sizeof(gConfig.hostname) - 1);
                        gConfig.hostname[min((uint8_t)(sizeof(gConfig.hostname)-1), plen)] = '\0';
                        break;
                    default: break;
                }
                Serial.printf("[NET] %s = '%s'\n",
                    _items[_textItem].label, _items[_textItem].value);
                _editingText = false;
                return SCREEN_NONE;
            }

            default:
                return SCREEN_NONE;
        }
    }

    // ----------------------------------------------------------
    //  Načti hodnoty z gConfig do _items[]
    // ----------------------------------------------------------
    static void _loadFromConfig() {
        // STA
        snprintf(_items[ITEM_STA_EN].value,   20, "%s",
                 gConfig.wifiStaEn ? "on" : "off");
        strncpy(_items[ITEM_STA_SSID].value,  gConfig.wifiStaSsid,
                min((int)sizeof(_items[0].value)-1, 31));
        // Heslo – zobrazíme hvězdičky
        uint8_t passLen = (uint8_t)strlen(gConfig.wifiStaPass);
        passLen = min(passLen, (uint8_t)(sizeof(_items[0].value) - 1));
        memset(_items[ITEM_STA_PASS].value, '*', passLen);
        _items[ITEM_STA_PASS].value[passLen] = '\0';

        snprintf(_items[ITEM_STA_DHCP].value, 20, "%s",
                 gConfig.wifiStaDhcp ? "on" : "off");
        snprintf(_items[ITEM_STA_IP].value,   20, "%u.%u.%u.%u",
                 gConfig.wifiStaIp[0], gConfig.wifiStaIp[1],
                 gConfig.wifiStaIp[2], gConfig.wifiStaIp[3]);
        snprintf(_items[ITEM_STA_MASK].value, 20, "%u.%u.%u.%u",
                 gConfig.wifiStaMask[0], gConfig.wifiStaMask[1],
                 gConfig.wifiStaMask[2], gConfig.wifiStaMask[3]);
        snprintf(_items[ITEM_STA_GW].value,   20, "%u.%u.%u.%u",
                 gConfig.wifiStaGw[0], gConfig.wifiStaGw[1],
                 gConfig.wifiStaGw[2], gConfig.wifiStaGw[3]);
        snprintf(_items[ITEM_STA_DNS].value,  20, "%u.%u.%u.%u",
                 gConfig.wifiStaDns[0], gConfig.wifiStaDns[1],
                 gConfig.wifiStaDns[2], gConfig.wifiStaDns[3]);

        // AP
        snprintf(_items[ITEM_AP_EN].value,    20, "%s",
                 gConfig.wifiApEn ? "on" : "off");
        strncpy(_items[ITEM_AP_SSID].value,   gConfig.wifiApSsid,
                min((int)sizeof(_items[0].value)-1, 19));
        uint8_t apPassLen = (uint8_t)strlen(gConfig.wifiApPass);
        apPassLen = min(apPassLen, (uint8_t)(sizeof(_items[0].value) - 1));
        memset(_items[ITEM_AP_PASS].value, '*', apPassLen);
        _items[ITEM_AP_PASS].value[apPassLen] = '\0';

        snprintf(_items[ITEM_AP_CHANNEL].value, 20, "%u",   gConfig.wifiApChannel);
        snprintf(_items[ITEM_AP_HIDDEN].value,  20, "%s",
                 gConfig.wifiApHidden ? "ano" : "ne");
        snprintf(_items[ITEM_AP_IP].value,      20, "%u.%u.%u.%u",
                 gConfig.wifiApIp[0], gConfig.wifiApIp[1],
                 gConfig.wifiApIp[2], gConfig.wifiApIp[3]);
        snprintf(_items[ITEM_AP_DHCP_START].value, 20, "%u.%u.%u.%u",
                 gConfig.wifiApDhcpStart[0], gConfig.wifiApDhcpStart[1],
                 gConfig.wifiApDhcpStart[2], gConfig.wifiApDhcpStart[3]);
        snprintf(_items[ITEM_AP_DHCP_END].value,   20, "%u.%u.%u.%u",
                 gConfig.wifiApDhcpEnd[0], gConfig.wifiApDhcpEnd[1],
                 gConfig.wifiApDhcpEnd[2], gConfig.wifiApDhcpEnd[3]);

        // NTP
        snprintf(_items[ITEM_NTP_EN].value,     20, "%s",
                 gConfig.ntpEn ? "on" : "off");
        strncpy(_items[ITEM_NTP_SERVER].value,  gConfig.ntpServer,
                min((int)sizeof(_items[0].value)-1, 31));
        // Timezone – zkrácený náhled
        strncpy(_items[ITEM_NTP_TZ].value, gConfig.ntpTz,
                min((int)sizeof(_items[0].value)-1, 19));
        snprintf(_items[ITEM_NTP_RESYNC].value, 20, "%lu h",
                 (unsigned long)(gConfig.ntpResyncSec / 3600));

        // Ostatni
        strncpy(_items[ITEM_HOSTNAME].value, gConfig.hostname,
                min((int)sizeof(_items[0].value)-1, 23));
    }

    // ----------------------------------------------------------
    //  Ulož editovanou hodnotu zpět do gConfig (non-text položky)
    // ----------------------------------------------------------
    static void _saveItem(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_STA_EN:
                gConfig.wifiStaEn = (strcmp(_items[idx].value, "on") == 0);
                break;
            case ITEM_STA_DHCP:
                gConfig.wifiStaDhcp = (strcmp(_items[idx].value, "on") == 0);
                break;
            case ITEM_STA_IP:
            case ITEM_STA_MASK:
            case ITEM_STA_GW:
            case ITEM_STA_DNS: {
                uint8_t* arr = nullptr;
                if      (idx == ITEM_STA_IP)   arr = gConfig.wifiStaIp;
                else if (idx == ITEM_STA_MASK)  arr = gConfig.wifiStaMask;
                else if (idx == ITEM_STA_GW)    arr = gConfig.wifiStaGw;
                else                            arr = gConfig.wifiStaDns;
                sscanf(_items[idx].value, "%hhu.%hhu.%hhu.%hhu",
                       &arr[0], &arr[1], &arr[2], &arr[3]);
                break;
            }
            case ITEM_AP_EN:
                gConfig.wifiApEn = (strcmp(_items[idx].value, "on") == 0);
                break;
            case ITEM_AP_CHANNEL:
                gConfig.wifiApChannel = (uint8_t)constrain(
                    atoi(_items[idx].value), 1, 13);
                break;
            case ITEM_AP_HIDDEN:
                gConfig.wifiApHidden = (strcmp(_items[idx].value, "ano") == 0);
                break;
            case ITEM_AP_IP:
            case ITEM_AP_DHCP_START:
            case ITEM_AP_DHCP_END: {
                uint8_t* arr = nullptr;
                if      (idx == ITEM_AP_IP)         arr = gConfig.wifiApIp;
                else if (idx == ITEM_AP_DHCP_START)  arr = gConfig.wifiApDhcpStart;
                else                                 arr = gConfig.wifiApDhcpEnd;
                sscanf(_items[idx].value, "%hhu.%hhu.%hhu.%hhu",
                       &arr[0], &arr[1], &arr[2], &arr[3]);
                break;
            }
            case ITEM_NTP_EN:
                gConfig.ntpEn = (strcmp(_items[idx].value, "on") == 0);
                if (gConfig.ntpEn) gNtpResync = true;
                break;
            case ITEM_NTP_RESYNC:
                gConfig.ntpResyncSec = (uint32_t)constrain(
                    atoi(_items[idx].value), 1, 24) * 3600;
                break;
            default: break;
        }
        ConfigManager::saveBlockWifi();
        Serial.printf("[NET] Ulozeno: %s = %s\n",
            _items[idx].label, _items[idx].value);
    }

    // ----------------------------------------------------------
    //  Krok hodnoty UP/DOWN pro non-text položky
    // ----------------------------------------------------------
    static void _stepUp(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_STA_EN:
            case ITEM_AP_EN:
            case ITEM_NTP_EN:
            case ITEM_STA_DHCP:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            case ITEM_AP_HIDDEN:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "ano") == 0 ? "ne" : "ano");
                break;
            case ITEM_AP_CHANNEL: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 13);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_NTP_RESYNC: {
                int v = constrain(atoi(_items[idx].value) + 1, 1, 24);
                snprintf(_items[idx].value, 20, "%d h", v);
                break;
            }
            // IP adresy – inkrementuj poslední oktet
            case ITEM_STA_IP:
            case ITEM_STA_MASK:
            case ITEM_STA_GW:
            case ITEM_STA_DNS:
            case ITEM_AP_IP:
            case ITEM_AP_DHCP_START:
            case ITEM_AP_DHCP_END: {
                uint8_t a, b, c, d;
                sscanf(_items[idx].value, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d);
                d = (d + 1) % 256;
                snprintf(_items[idx].value, 20, "%u.%u.%u.%u", a, b, c, d);
                break;
            }
            default: break;
        }
    }

    static void _stepDown(uint8_t idx) {
        switch ((ItemId)idx) {
            case ITEM_STA_EN:
            case ITEM_AP_EN:
            case ITEM_NTP_EN:
            case ITEM_STA_DHCP:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "on") == 0 ? "off" : "on");
                break;
            case ITEM_AP_HIDDEN:
                snprintf(_items[idx].value, 20, "%s",
                    strcmp(_items[idx].value, "ano") == 0 ? "ne" : "ano");
                break;
            case ITEM_AP_CHANNEL: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 13);
                snprintf(_items[idx].value, 20, "%d", v);
                break;
            }
            case ITEM_NTP_RESYNC: {
                int v = constrain(atoi(_items[idx].value) - 1, 1, 24);
                snprintf(_items[idx].value, 20, "%d h", v);
                break;
            }
            case ITEM_STA_IP:
            case ITEM_STA_MASK:
            case ITEM_STA_GW:
            case ITEM_STA_DNS:
            case ITEM_AP_IP:
            case ITEM_AP_DHCP_START:
            case ITEM_AP_DHCP_END: {
                uint8_t a, b, c, d;
                sscanf(_items[idx].value, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d);
                d = (d + 255) % 256;
                snprintf(_items[idx].value, 20, "%u.%u.%u.%u", a, b, c, d);
                break;
            }
            default: break;
        }
    }

    // ----------------------------------------------------------
    //  Kreslení položky
    //  Vrací výšku nakreslené položky (0 = FTR_Y dosaženo).
    // ----------------------------------------------------------


    // ----------------------------------------------------------
    //  Pomocná: Y pozice položky idx (simuluje _drawAllItems)
    // ----------------------------------------------------------
    static int16_t _itemY(uint8_t idx) {
        int16_t y = NET_START_Y;
        for (uint8_t i = _scrollOffset; i < idx; i++) {
            if (_items[i].section != nullptr) y += NET_SEC_H;
            y += NET_ROW_H;
        }
        return y;
    }

    static int16_t _drawItemAt(const Theme* t, uint8_t idx, int16_t y) {
        NetItem& item    = _items[idx];
        bool       active   = (idx == _cursor);
        bool       editing  = (active && _editing);
        int16_t    drawn    = 0;
        LovyanGFX* dc       = _spr ? (LovyanGFX*)_spr : (LovyanGFX*)&tft;
        int16_t    sy       = _spr ? y - CONTENT_Y : y;

        // Sekce header
        if (item.section != nullptr) {
            if (y + NET_SEC_H > FTR_Y) return 0;
            dc->fillRect(0, sy, 320, NET_SEC_H, t->bg);
            dc->setFont(&fonts::Font2);
            dc->setTextColor(t->dim);
            dc->setCursor(16, sy);
            dc->print(item.section);
            sy += 14;
            dc->drawFastHLine(16, sy, 288, t->dim);
            sy += 2;
            drawn += NET_SEC_H;
        }

        if (y + drawn + NET_ROW_H > FTR_Y) return drawn;

        // Pozadí řádku
        if (active) {
            dc->fillRect(8, sy, 304, NET_ROW_H, t->header);
            dc->fillRect(8, sy, 3,   NET_ROW_H, t->accent);
        } else {
            dc->fillRect(8, sy, 304, NET_ROW_H, t->bg);
        }

        // Label
        dc->setFont(&fonts::Font2);
        dc->setTextColor(active ? t->accent : t->text);
        dc->setCursor(20, sy + 7);
        dc->print(item.label);

        // Hodnota – textové položky mají >>> hint
        if (item.isText) {
            dc->setTextColor(editing ? t->accent : t->text);
            dc->setTextDatum(middle_right);
            dc->drawString(item.value[0] ? item.value : "---",
                           active ? 290 : 303, sy + 12);
            if (active) {
                dc->setTextColor(t->dim);
                dc->drawString(">>>", 308, sy + 12);
            }
            dc->setTextDatum(top_left);
        } else {
            dc->setTextColor(editing ? t->accent : t->text);
            dc->setTextDatum(middle_right);
            dc->drawString(item.value, 308, sy + 12);
            dc->setTextDatum(top_left);
        }

        drawn += NET_ROW_H;
        return drawn;
    }

    // ----------------------------------------------------------
    //  Překresli všechny viditelné položky
    // ----------------------------------------------------------
    static void _drawAllItems(const Theme* t) {
        if (_spr) { _spr->fillScreen(t->bg); }
        else { tft.fillRect(0, CONTENT_Y, 320, CONTENT_H, t->bg); }
        int16_t y = NET_START_Y;
        for (uint8_t i = _scrollOffset; i < ITEM_COUNT; i++) {
            int16_t h = _drawItemAt(t, i, y);
            if (h == 0) break;
            y += h;
        }
        if (_spr) { _spr->pushSprite(0, CONTENT_Y); }
    }

    // ----------------------------------------------------------
    //  Překresli jednu položku
    // ----------------------------------------------------------
    static void _drawItem(const Theme* t, uint8_t idx) {
        _drawAllItems(t);
    }

    // ----------------------------------------------------------
    //  Scroll – pixelový přístup (stejný jako ControlScreen)
    // ----------------------------------------------------------
    static bool _ensureVisible() {
        if (_cursor < _scrollOffset) {
            _scrollOffset = _cursor;
            return true;
        }
        bool changed = false;
        while (_scrollOffset < _cursor) {
            int16_t y      = _itemY(_cursor);
            int16_t secH   = (_items[_cursor].section != nullptr) ? NET_SEC_H : 0;
            int16_t bottom = y + secH + NET_ROW_H;
            if (bottom <= FTR_Y) break;
            _scrollOffset++;
            changed = true;
        }
        return changed;
    }

    // ----------------------------------------------------------
    //  Nakresli spodní lištu pro seznam
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
            _drawAllItems(t);
        }
    }

    void update(const Theme* t, const DateTime& dt,
                uint8_t apState, uint8_t staState, uint8_t invState,
                bool alarm, const SolarData& d) {
        Header::update(t, dt, apState, staState, invState, alarm);
    }

    Screen handleInput(const Theme* t, SwButton btn) {

        // --- Textový editor ---
        if (_editingText) {
            Screen ret = _handleTextInput(t, btn);
            if (!_editingText) {
                // Editor zavřen – překresli seznam
                _loadFromConfig();
                _drawFooter(t);
                _drawAllItems(t);
            }
            return ret;
        }

        // --- Editace non-text položky ---
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
                case SW_LEFT:
                    _editing = false;
                    _saveItem(_cursor);
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

            case SW_CENTER: {
                NetItem& item = _items[_cursor];
                if (item.isText) {
                    // Otevři textový editor
                    _textItem   = _cursor;
                    _textPos    = 0;
                    _editingText = true;

                    // Zjisti max délku pole a naplň buffer
                    switch ((ItemId)_cursor) {
                        case ITEM_STA_SSID:
                            _textMaxLen = sizeof(gConfig.wifiStaSsid) - 1;
                            strncpy(_textBuf, gConfig.wifiStaSsid, _textMaxLen);
                            break;
                        case ITEM_STA_PASS:
                            _textMaxLen = sizeof(gConfig.wifiStaPass) - 1;
                            // Heslo editujeme v plaintextu (ne hvězdičky)
                            strncpy(_textBuf, gConfig.wifiStaPass, _textMaxLen);
                            break;
                        case ITEM_AP_SSID:
                            _textMaxLen = sizeof(gConfig.wifiApSsid) - 1;
                            strncpy(_textBuf, gConfig.wifiApSsid, _textMaxLen);
                            break;
                        case ITEM_AP_PASS:
                            _textMaxLen = sizeof(gConfig.wifiApPass) - 1;
                            strncpy(_textBuf, gConfig.wifiApPass, _textMaxLen);
                            break;
                        case ITEM_NTP_SERVER:
                            _textMaxLen = sizeof(gConfig.ntpServer) - 1;
                            strncpy(_textBuf, gConfig.ntpServer, _textMaxLen);
                            break;
                        case ITEM_NTP_TZ:
                            _textMaxLen = sizeof(gConfig.ntpTz) - 1;
                            strncpy(_textBuf, gConfig.ntpTz, _textMaxLen);
                            break;
                        case ITEM_HOSTNAME:
                            _textMaxLen = sizeof(gConfig.hostname) - 1;
                            strncpy(_textBuf, gConfig.hostname, _textMaxLen);
                            break;
                        default:
                            _textMaxLen = sizeof(_items[0].value) - 1;
                            strncpy(_textBuf, item.value, _textMaxLen);
                            break;
                    }
                    // Doplň mezerami na plnou délku
                    uint8_t srcLen = (uint8_t)strlen(_textBuf);
                    if (srcLen < _textMaxLen)
                        memset(_textBuf + srcLen, ' ', _textMaxLen - srcLen);
                    _textBuf[_textMaxLen] = '\0';

                    _drawTextEditor(t);
                } else {
                    _editing = true;
                    _drawItem(t, _cursor);
                }
                return SCREEN_NONE;
            }

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
        _editingText  = false;
        _textPos      = 0;
        _loadFromConfig();
    }

} // namespace NetworkScreen