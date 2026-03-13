// =============================================================
//  SolarData.h – sdílená datová struktura Solar HMI
//
//  Sdílená struktura mezi Core 0 (displej, UI) a Core 1 (Modbus).
//  Core 1 zapisuje přes SolarModel::update().
//  Core 0 čte přes SolarModel::get().
//  Přístup VŽDY přes mutex – nikdy přímo do _data!
//
//  Rozšíření: přidej pole do SolarData a aktualizuj
//  SolarModel::update() v InverterDriver.h
// =============================================================
#pragma once
#include <Arduino.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "InverterTypes.h"   // InverterData – vyplněna z Modbus tasku

// =============================================================
//  Sdílená data – plní Core 1 (Modbus), čte Core 0
// =============================================================
struct SolarData {

    // --- Výkony [W] ---
    int32_t  powerPV;           // výroba solárních panelů
    int32_t  powerLoad;         // celková spotřeba domu
    int32_t  powerBattery;      // baterie: + vybíjení, - nabíjení
    int32_t  powerGrid;         // síť celkem: + odběr, - dodávka

    // --- Přeток fází [W] ---
    // Kladná = dodávka do sítě (zelená)
    // Záporná = odběr ze sítě (červená)
    int32_t  phaseL1;
    int32_t  phaseL2;
    int32_t  phaseL3;

    // --- Baterie ---
    uint16_t soc;               // stav nabití [%] 0–100
    uint16_t soh;               // zdraví baterie [%] 0–100

    // --- Energie dnes [Wh] ---
    uint32_t energyPvToday;     // výroba dnes
    uint32_t energyGridToday;   // koupeno ze sítě dnes
    uint32_t energySoldToday;   // prodáno do sítě dnes

    // --- Výstupy (relé) ---
    // relayOn:      relé fyzicky sepnuto řídicí logikou
    // relayHeating: teče proud = zásobník se ohřívá
    //               (odvozeno z měření změny toku fází po sepnutí)
    bool     relayOn[10];
    bool     relayHeating[10];

    // --- Elektroměry bytů [Wh] ---
    // Čítáno z pulzních vstupů IRQ (PulseCounter)
    uint32_t apartmentWh[10];

    // --- Stav měniče ---
    uint16_t invStatus;         // 0=čekám, 2=on-grid, 3=fault, 5=off-grid
    bool     invOnline;         // Modbus komunikace OK

    // --- Meta ---
    bool     valid;             // data platná (aspoň jednou přečtena)
    uint32_t lastUpdateMs;      // millis() posledního úspěšného čtení
    uint8_t  errorCount;        // počet Modbus chyb za sebou
};

// =============================================================
//  SolarModel – thread-safe přístup k SolarData
// =============================================================
namespace SolarModel {

    static SolarData      _data   = {};
    static SemaphoreHandle_t _mutex = nullptr;

    // Inicializace – volej jednou v setup() před startem tasků
    void begin() {
        _mutex = xSemaphoreCreateMutex();
        memset(&_data, 0, sizeof(_data));
        Serial.println("[SolarModel] Init OK");
    }

    // Zkopíruje aktuální data do out (thread-safe)
    // Vrací true pokud jsou data platná
    bool get(SolarData& out) {
        if (!_mutex) return false;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            out = _data;
            xSemaphoreGive(_mutex);
            return out.valid;
        }
        return false;
    }

    // Aktualizuje data z InverterDriver (volá Core 1 po každém úspěšném poll)
    // Použití: SolarModel::updateFromInverter(inv_data);
    void updateFromInverter(const InverterData& inv) {
        if (!_mutex) return;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _data.powerPV          = inv.powerPV;
            _data.powerLoad        = inv.powerLoad;
            _data.powerBattery     = inv.powerBattery;
            _data.powerGrid        = inv.powerGrid;
            _data.soc              = inv.soc;
            _data.soh              = inv.soh;
            _data.energyPvToday    = inv.energyPvToday;
            _data.energyGridToday  = inv.energyGridToday;
            _data.energySoldToday  = inv.energySoldToday;
            _data.invStatus        = inv.status;
            _data.invOnline        = inv.valid;
            _data.valid            = inv.valid;
            _data.lastUpdateMs     = inv.lastUpdateMs;
            _data.errorCount       = inv.errorCount;

            // Fáze L1/L2/L3 – reálné hodnoty z Modbus registrů 10994/10996/10998
            _data.phaseL1 = inv.phaseL1;
            _data.phaseL2 = inv.phaseL2;
            _data.phaseL3 = inv.phaseL3;

            xSemaphoreGive(_mutex);
        }
    }

    // Aktualizuje stav relé a odvozený stav ohřevu (volá řídicí logika)
    void updateRelays(const bool on[10], const bool heating[10]) {
        if (!_mutex) return;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            memcpy(_data.relayOn,      on,      10 * sizeof(bool));
            memcpy(_data.relayHeating, heating, 10 * sizeof(bool));
            xSemaphoreGive(_mutex);
        }
    }

    // Aktualizuje odběr bytů z PulseCounter (volá Core 0 v loop)
    void updateApartments(const uint32_t wh[10]) {
        if (!_mutex) return;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            memcpy(_data.apartmentWh, wh, 10 * sizeof(uint32_t));
            xSemaphoreGive(_mutex);
        }
    }

    // Přímý zápis fází (pokud měnič podporuje individuální fáze)
    void updatePhases(int32_t l1, int32_t l2, int32_t l3) {
        if (!_mutex) return;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            _data.phaseL1 = l1;
            _data.phaseL2 = l2;
            _data.phaseL3 = l3;
            xSemaphoreGive(_mutex);
        }
    }

} // namespace SolarModel
