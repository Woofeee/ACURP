#pragma once
// =============================================================================
// InverterTypes.h – datove typy pro Modbus komunikaci s menicem
// Profily meniců, mapa registru, sdilena data mezi jadry
// =============================================================================

#include <Arduino.h>
#include "HW_Config.h"

// ---------------------------------------------------------------------------
// Vysledna data z menice – sdilena struktura mezi Core 0 a Core 1
// Core 1 zapisuje (Modbus task), Core 0 cte (displej, MQTT, ridici logika)
// Pristup VZDY pres mutex!
// ---------------------------------------------------------------------------
struct InverterData {
    // Vykony [W]
    int32_t  powerGrid;       // sit: + odber, - dodavka do site
    int32_t  powerPV;         // vykon solaru [W]
    int32_t  powerBattery;    // vykon baterie: + vybijeni, - nabijeni [W]
    int32_t  powerLoad;       // celkova spotřeba domu [W]

    // Baterie
    uint16_t soc;             // stav nabiti [%], 0–100
    uint16_t soh;             // zdravi baterie [%], 0–100

    // Stav menice
    uint16_t status;          // registr 10105: 0=wait,2=on-grid,3=fault,5=off-grid
    uint32_t operationFlag;   // registr 10110: bitove priznaky

    // Energie dnes [Wh]
    uint32_t energyPvToday;   // PV vyroba dnes
    uint32_t energyGridToday; // koupeno ze site dnes
    uint32_t energySoldToday; // prodano do site dnes

    // Metainformace
    bool     valid;           // data jsou platna (aspon jednou prectena)
    uint32_t lastUpdateMs;    // millis() posledniho uspesneho cteni
    uint8_t  errorCount;      // pocet chyb komunikace za sebou
};

// ---------------------------------------------------------------------------
// Mapovani registru na pole v InverterData
// ---------------------------------------------------------------------------
enum RegisterMap : uint8_t {
    MAP_GRID      = 0,
    MAP_PV        = 1,
    MAP_BATTERY   = 2,
    MAP_LOAD      = 3,
    MAP_SOC       = 4,
    MAP_SOH       = 5,
    MAP_STATUS    = 6,
    MAP_OP_FLAG   = 7,
    MAP_PV_TODAY  = 8,
    MAP_GRID_BUY  = 9,
    MAP_GRID_SELL = 10,
    MAP_IGNORE    = 0xFF,
};

// ---------------------------------------------------------------------------
// Definice jednoho registru v profilu
// ---------------------------------------------------------------------------
struct RegisterDef {
    uint16_t address;   // Modbus adresa registru
    uint8_t  count;     // pocet registru (1 = U16/I16, 2 = U32/I32)
    bool     isSigned;  // true = I16/I32, false = U16/U32
    int32_t  gain;      // delitel (napr. 1000 → raw/1000)
    int32_t  multiply;  // nasobitel (napr. 1000 pro kW→W)
    uint8_t  mapTo;     // cil v InverterData (enum RegisterMap)
};

// Makra pro prehlednost definice registru
#define REG_U16(addr, gain, mul, map) { addr, 1, false, gain, mul, map }
#define REG_I16(addr, gain, mul, map) { addr, 1, true,  gain, mul, map }
#define REG_U32(addr, gain, mul, map) { addr, 2, false, gain, mul, map }
#define REG_I32(addr, gain, mul, map) { addr, 2, true,  gain, mul, map }

// ---------------------------------------------------------------------------
// Profil menice – sada registru + metadata
// ---------------------------------------------------------------------------
struct InverterProfile {
    const char*        name;
    const RegisterDef* regs;
    uint8_t            regCount;
};

// ==========================================================================
// PROFIL: Solinteg Hybrid Inverter
// Registry dle dokumentace Modbus Register Table V00.17
// ==========================================================================
static const RegisterDef SOLINTEG_REGS[] = {
    // Vykon site (smartmetr): + odber, - dodavka
    REG_I32(11000, 1000, 1,   MAP_GRID),
    // Vykon PV
    REG_U32(11028, 1000, 1,   MAP_PV),
    // Vykon baterie: + vybijeni, - nabijeni
    REG_I32(30258, 1000, 1,   MAP_BATTERY),
    // Celkova spotreba
    REG_I32(31306, 1000, 1,   MAP_LOAD),
    // SOC [%], gain=100
    REG_U16(33000, 100,  1,   MAP_SOC),
    // SOH [%]
    REG_U16(33001, 100,  1,   MAP_SOH),
    // Stav menice
    REG_U16(10105, 1,    1,   MAP_STATUS),
    // PV vyroba dnes [kWh*10 → Wh]
    REG_U16(31005, 10,   100, MAP_PV_TODAY),
    // Koupeno ze site dnes
    REG_U16(31001, 10,   100, MAP_GRID_BUY),
    // Prodano do site dnes
    REG_U16(31000, 10,   100, MAP_GRID_SELL),
};

// ==========================================================================
// PROFIL: Sermatec 10kW
// TODO: doplnit az bude k dispozici dokumentace registru
// ==========================================================================
static const RegisterDef SERMATEC_REGS[] = {};

// ---------------------------------------------------------------------------
// Seznam profilu – index odpovida gConfig.invProfileIndex
// ---------------------------------------------------------------------------
static const InverterProfile INVERTER_PROFILES[] = {
    { "Solinteg", SOLINTEG_REGS, sizeof(SOLINTEG_REGS) / sizeof(SOLINTEG_REGS[0]) },
    { "Sermatec", SERMATEC_REGS, 0 },
};

static const uint8_t INVERTER_PROFILE_COUNT =
    sizeof(INVERTER_PROFILES) / sizeof(INVERTER_PROFILES[0]);
