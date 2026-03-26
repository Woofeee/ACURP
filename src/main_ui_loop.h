// =============================================================
//  main_ui_loop.h – UI smyčka ACU RP (Core 0)
//
//  Vlož do main.cpp a zavolej:
//    uiSetup() v setup()
//    uiLoop()  v loop()
//
//  Předpoklady v main.cpp:
//    extern LGFX tft;
//    FiveWaySwitch    gSwitch;
//    extern PCF85063A gRTC;
//    MCP23017         gMCP;
//    BoilerConfig     gBoilerCfg[BOILER_MAX_COUNT];
//    BoilerSystem     gBoilerSys;
//    SolarModel::begin() voláno před startem tasků
// =============================================================
#pragma once
#include <Arduino.h>
#include "ScreenManager.h"
#include "SolarData.h"
#include "Theme.h"
#include "PCF85063A.h"
#include "FiveWaySwitch.h"
#include "MCP23017.h"
#include "BoilerConfig.h"

// Všechny screeny
#include "MainScreen.h"
#include "MenuScreen.h"
#include "HistoryScreen.h"
#include "DiagnosticScreen.h"
#include "SettingScreen.h"
#include "PasswordScreen.h"
#include "UdPScreen.h"
#include "DiscoveryScreen.h"
#include "ControlScreen.h"
#include "BoilerDetailScreen.h"
#include "NetworkScreen.h"

// =============================================================
//  Extern reference na globální proměnné z main.cpp
// =============================================================
extern const Theme*       gTheme;
extern PCF85063A          gRTC;
extern volatile bool      gWifiSta;
extern volatile bool      gWifiAp;
extern FiveWaySwitch      gSwitch;
extern MCP23017           gMCP;
extern BoilerConfig       gBoilerCfg[BOILER_MAX_COUNT];
extern BoilerSystem       gBoilerSys;

static SolarData    gUI_data = {};
static DateTime     gUI_dt   = {};
static bool         gAlarm   = false;

static uint8_t gDotAP  = DOT_OFF;
static uint8_t gDotSTA = DOT_OFF;
static uint8_t gDotINV = DOT_OFF;

// =============================================================
//  Sdílený sprite pro obsah screenů (320 × CONTENT_H px = 117 KB)
//  Alokuje se jednou v uiSetup(). Screeny si ho půjčují přes
//  gContentSprite – nikdy není v paměti víc než jeden.
// =============================================================
static LGFX_Sprite gContentSprite(&tft);

// =============================================================
//  Přepni screen + volej reset() pokud existuje
// =============================================================
static void _doSwitch(Screen next) {
    switch (next) {
        case SCREEN_MENU:       MenuScreen::reset();       break;
        case SCREEN_HISTORY:    HistoryScreen::reset();    break;
        case SCREEN_DIAGNOSTIC: DiagnosticScreen::reset(); break;
        case SCREEN_SETTING:    SettingScreen::reset();    break;
        case SCREEN_PASSWORD:   PasswordScreen::reset();   break;
        case SCREEN_UDP:        UdPScreen::reset();        break;
        case SCREEN_DISCOVERY:
            DiscoveryScreen::reset();
            DiscoveryScreen::begin(gMCP, gBoilerCfg, gBoilerSys.numBoilers);
            break;
        case SCREEN_CONTROL: ControlScreen::reset(); break;
        case SCREEN_BOILER_DETAIL:
            BoilerDetailScreen::reset();
            BoilerDetailScreen::begin(0);  // začni od bytu 1
            break;
        case SCREEN_NETWORK:
            NetworkScreen::reset();
            break;
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
        case SCREEN_DISCOVERY:
            DiscoveryScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_CONTROL:
            ControlScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_BOILER_DETAIL:
            BoilerDetailScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_NETWORK:
            NetworkScreen::draw(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        default:
            break;
    }
    ScreenManager::clearDraw();
}

// =============================================================
//  update() – periodická aktualizace (každou sekundu)
//  Discovery screen se aktualizuje každé 2s (Modbus poll interval)
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
        case SCREEN_DISCOVERY:
            // Discovery update voláme vždy (má vlastní časování 2s)
            DiscoveryScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_CONTROL:
            ControlScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_BOILER_DETAIL:
            BoilerDetailScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        case SCREEN_NETWORK:
            NetworkScreen::update(gTheme, gUI_dt,
                gDotAP, gDotSTA, gDotINV, gAlarm, gUI_data);
            break;
        default:
            break;
    }
}

// =============================================================
//  handleInput() – obsluha 5-way switche
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
        case SCREEN_DISCOVERY:
            next = DiscoveryScreen::handleInput(gTheme, btn, gUI_data);
            break;
        case SCREEN_CONTROL:
            next = ControlScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_BOILER_DETAIL:
            next = BoilerDetailScreen::handleInput(gTheme, btn);
            break;
        case SCREEN_NETWORK:
            next = NetworkScreen::handleInput(gTheme, btn);
            break;
        default:
            break;
    }

    if (next != SCREEN_NONE && next != cur) {
        _doSwitch(next);
    }
}

// =============================================================
//  Aktualizuj stav záhlaví
// =============================================================
static void _refreshState() {
    SolarModel::get(gUI_data);
    gUI_dt  = gRTC.getTime();

    gDotSTA = gWifiSta ? DOT_OK : DOT_OFF;
    gDotAP  = gWifiAp  ? DOT_OK : DOT_OFF;
    gDotINV = gUI_data.invOnline ? DOT_OK : DOT_ERROR;
    gAlarm  = (gUI_data.invStatus == 3);
}

// =============================================================
//  uiSetup() – inicializace UI, volej v setup()
// =============================================================
void uiSetup() {
    PasswordScreen::loadFromFRAM();
    HistoryScreen::loadFromFRAM();

    // Alokuj sdílený sprite pro obsah screenů
    void* ok = gContentSprite.createSprite(320, CONTENT_H);
    Serial.printf("[UI] content sprite alloc %s (%u KB)\n",
        ok ? "OK" : "FAIL",
        (unsigned)(320 * CONTENT_H * 2 / 1024));

    // Předej sprite screenům
    ControlScreen::setSprite(&gContentSprite);
    NetworkScreen::setSprite(&gContentSprite);

    ScreenManager::replaceTo(SCREEN_MAIN);
    _refreshState();
    _drawCurrent();

    Serial.println("[UI] Setup OK");
}

// =============================================================
//  uiLoop() – hlavní smyčka UI, volej v loop()
// =============================================================
void uiLoop() {
    // 1. Vstup ze switche
    SwButton btn = gSwitch.read();
    _handleInput(btn);

    // 2. Překresli při změně screenu
    if (ScreenManager::needDraw()) {
        _refreshState();
        _drawCurrent();
        return;
    }

    // 3. Periodický update 1× za sekundu
    // Discovery screen má vlastní časování – update voláme vždy
    if (ScreenManager::current() == SCREEN_DISCOVERY) {
        _refreshState();
        _updateCurrent();
    } else if (ScreenManager::shouldUpdate()) {
        _refreshState();
        _updateCurrent();
    }
}
