// =============================================================
//  main_ui_loop.h – UI smyčka ACU RP (Core 0)
//
//  Vlož do main.cpp a zavolej:
//    uiSetup() v setup()
//    uiLoop()  v loop()
//
//  Předpoklady v main.cpp:
//    extern LGFX tft;
//    FiveWaySwitch gSwitch;   – definováno v main.cpp
//    extern PCF85063A gRTC;
//    SolarModel::begin() voláno před startem tasků
// =============================================================
#pragma once
#include <Arduino.h>
#include "ScreenManager.h"
#include "SolarData.h"
#include "Theme.h"
#include "PCF85063A.h"
#include "FiveWaySwitch.h"

// Všechny screeny
#include "MainScreen.h"
#include "MenuScreen.h"
#include "HistoryScreen.h"
#include "DiagnosticScreen.h"
#include "SettingScreen.h"
#include "PasswordScreen.h"
#include "UdPScreen.h"

// =============================================================
//  Glue proměnné
//  Definovány v main.cpp – jen extern reference
// =============================================================
extern const Theme*       gTheme;
extern PCF85063A          gRTC;
extern volatile bool      gWifiSta;
extern volatile bool      gWifiAp;
extern FiveWaySwitch      gSwitch;

static SolarData    gUI_data  = {};
static DateTime     gUI_dt    = {};
static bool         gAlarm    = false;

// Stav indikátorů záhlaví
static uint8_t gDotAP  = DOT_OFF;
static uint8_t gDotSTA = DOT_OFF;
static uint8_t gDotINV = DOT_OFF;

// =============================================================
//  Pomocná funkce: přepni screen a volej reset() pokud existuje
// =============================================================
static void _doSwitch(Screen next) {
    switch (next) {
        case SCREEN_MENU:       MenuScreen::reset();       break;
        case SCREEN_HISTORY:    HistoryScreen::reset();    break;
        case SCREEN_DIAGNOSTIC: DiagnosticScreen::reset(); break;
        case SCREEN_SETTING:    SettingScreen::reset();    break;
        case SCREEN_PASSWORD:   PasswordScreen::reset();   break;
        case SCREEN_UDP:        UdPScreen::reset();        break;
        default: break;
    }
    ScreenManager::switchTo(next);
}

// =============================================================
//  draw() – vykresli aktuální screen
// =============================================================
static void _drawCurrent() {
    Screen cur = ScreenManager::current();

    switch (cur) {
        case SCREEN_MAIN:
            MainScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_MENU:
            MenuScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_HISTORY:
            HistoryScreen::loadFromFRAM();
            HistoryScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_DIAGNOSTIC:
            DiagnosticScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_SETTING:
            SettingScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_PASSWORD:
            PasswordScreen::loadFromFRAM();
            PasswordScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_UDP:
            UdPScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        default:
            break;
    }
    ScreenManager::clearDraw();
}

// =============================================================
//  update() – periodická aktualizace (každou sekundu)
// =============================================================
static void _updateCurrent() {
    Screen cur = ScreenManager::current();

    switch (cur) {
        case SCREEN_MAIN:
            MainScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_MENU:
            MenuScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_HISTORY:
            HistoryScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_DIAGNOSTIC:
            DiagnosticScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_SETTING:
            SettingScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_PASSWORD:
            PasswordScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_UDP:
            UdPScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        default:
            break;
    }
}

// =============================================================
//  handleInput() – obsluha 5-way switche pro aktuální screen
// =============================================================
static void _handleInput(SwButton btn) {
    if (btn == SW_NONE) return;

    Screen next = SCREEN_NONE;
    Screen cur  = ScreenManager::current();

    switch (cur) {
        case SCREEN_MAIN:
            next = MainScreen::handleInput(btn);
            break;
        case SCREEN_MENU:
            next = MenuScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_HISTORY:
            next = HistoryScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_DIAGNOSTIC:
            next = DiagnosticScreen::handleInput(gTheme, btn, gUI_data);
            break;
        case SCREEN_SETTING:
            next = SettingScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_PASSWORD:
            next = PasswordScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_UDP:
            next = UdPScreen::handleInput(gTheme, btn);
            break;
        default:
            break;
    }

    if (next != SCREEN_NONE && next != cur) {
        _doSwitch(next);
    }
}

// =============================================================
//  Aktualizuj stav záhlaví z externích zdrojů
// =============================================================
static void _refreshState() {
    SolarModel::get(gUI_data);
    gUI_dt  = gRTC.getTime();

    gDotSTA = gWifiSta ? DOT_OK : DOT_OFF;
    gDotAP  = gWifiAp  ? DOT_OK : DOT_OFF;
    gDotINV = gUI_data.invOnline ? DOT_OK : DOT_ERROR;

    // Alarm – fault stav měniče
    gAlarm  = (gUI_data.invStatus == 3);
}

// =============================================================
//  uiSetup() – inicializace UI, volej v setup()
// =============================================================
void uiSetup() {
    PasswordScreen::loadFromFRAM();
    HistoryScreen::loadFromFRAM();

    ScreenManager::replaceTo(SCREEN_MAIN);
    _refreshState();
    _drawCurrent();

    Serial.println("[UI] Setup OK");
}

// =============================================================
//  uiLoop() – hlavní smyčka UI, volej v loop()
// =============================================================
void uiLoop() {
    // 1. Přečti vstup ze 5-way switche
    SwButton btn = gSwitch.read();
    _handleInput(btn);

    // 2. Pokud se změnil screen → překresli
    if (ScreenManager::needDraw()) {
        _refreshState();
        _drawCurrent();
        return;
    }

    // 3. Periodický update (1× za sekundu)
    if (ScreenManager::shouldUpdate()) {
        _refreshState();
        _updateCurrent();
    }
}