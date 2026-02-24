#pragma once
// =============================================================================
// InverterDriver.h – Modbus driver pro komunikaci s menicem
// Periodicky cte registry dle aktivniho profilu a plni InverterData
// Bezi jako FreeRTOS task na Core 1
// =============================================================================

#include <Arduino.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "Config.h"
#include "InverterTypes.h"
#include "ModbusClient.h"

// Pocet chyb za sebou nez se oznaci data za neplatna
#define INVERTER_MAX_ERRORS  5

// =============================================================================
// InverterDriver
// =============================================================================
class InverterDriver {
public:
    // Konstruktor – vezme konfiguraci primo z gConfig
    // dereCallback: funkce pro rizeni DE/RE pinu RS485 pres MCP23017
    //               pro TCP transport predej nullptr
    InverterDriver(const Config& cfg, ModbusDeReCallback dereCallback = nullptr)
        : _cfg(cfg), _dereCallback(dereCallback)
    {
        memset(&_data, 0, sizeof(_data));
        _mutex = xSemaphoreCreateMutex();
    }

    ~InverterDriver() {
        if (_client) delete _client;
        if (_mutex)  vSemaphoreDelete(_mutex);
    }

    // Inicializace – vytvori transportni vrstvu a pokusi se pripojit
    bool begin() {
        const InverterProfile& profile = INVERTER_PROFILES[_cfg.invProfileIndex];
        Serial.printf("[INV] Profil: %s  transport: %s\n",
            profile.name,
            _cfg.invTransport == TRANSPORT_TCP ? "TCP" : "RTU");

        if (_cfg.invTransport == TRANSPORT_RTU) {
            _client = new ModbusRTUClient(_cfg.invBaudRate, _dereCallback);
        } else {
            _client = new ModbusTCPClient(_cfg.invIp, _cfg.invTcpPort);
        }

        bool ok = _client->begin();
        Serial.printf("[INV] Transport %s\n", ok ? "OK" : "CHYBA");
        return ok;
    }

    // Jednorázové cteni vsech registru profilu
    // Vraci true pokud aspon jeden registr byl precten uspesne
    bool poll() {
        const InverterProfile& profile = INVERTER_PROFILES[_cfg.invProfileIndex];
        if (profile.regCount == 0) return false;

        bool    anyOk      = false;
        uint8_t errorCount = 0;

        for (uint8_t i = 0; i < profile.regCount; i++) {
            const RegisterDef& reg = profile.regs[i];

            uint16_t raw[2] = {0, 0};
            ModbusError err = _client->readHoldingRegisters(
                _cfg.invSlaveId,
                reg.address,
                reg.count,
                raw
            );

            if (err != MODBUS_OK) {
                errorCount++;
                if (errorCount <= 3) {
                    Serial.printf("[INV] Chyba reg %u: %u\n", reg.address, err);
                }
                continue;
            }

            anyOk = true;
            _applyRegister(reg, raw);
        }

        // Aktualizace metadat pod mutex
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (anyOk) {
                _data.valid        = true;
                _data.lastUpdateMs = millis();
                _data.errorCount   = errorCount;
            } else {
                _data.errorCount++;
                if (_data.errorCount >= INVERTER_MAX_ERRORS) {
                    _data.valid = false;
                }
            }
            xSemaphoreGive(_mutex);
        }

        return anyOk;
    }

    // Kopiruje aktualni data do cilove struktury (thread-safe)
    bool getData(InverterData& out) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            out = _data;
            xSemaphoreGive(_mutex);
            return out.valid;
        }
        return false;
    }

    // Vraci true pokud jsou data cerstva (mene nez maxAgeMs ms stara)
    bool isDataFresh(uint32_t maxAgeMs = 10000) {
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            bool fresh = _data.valid &&
                         (millis() - _data.lastUpdateMs) < maxAgeMs;
            xSemaphoreGive(_mutex);
            return fresh;
        }
        return false;
    }

    // Zapis jednoho registru (ridici prikazy)
    // POZOR: Solinteg – pis vzdy jen jeden registr najednou!
    ModbusError writeRegister(uint16_t addr, uint16_t value) {
        return _client->writeSingleRegister(_cfg.invSlaveId, addr, value);
    }

    // Nastavi pracovni mod menice (registr 50000)
    // EMS_ACCtrlMode  = 769  (0x0301)
    // EMS_BattCtrlMode= 771  (0x0303)
    ModbusError setWorkMode(uint16_t modeValue) {
        return writeRegister(50000, modeValue);
    }

    // Vraci nazev aktivniho profilu
    const char* profileName() const {
        return INVERTER_PROFILES[_cfg.invProfileIndex].name;
    }

    // -----------------------------------------------------------------------
    // FreeRTOS task wrapper
    // Pouziti v main.cpp:
    //   xTaskCreate(InverterDriver::task, "Inverter", 4096, &gInverter, 3, nullptr);
    // -----------------------------------------------------------------------
    static void task(void* param) {
        InverterDriver* drv = static_cast<InverterDriver*>(param);
        Serial.println("[INV] Task spusten");

        if (!drv->begin()) {
            Serial.println("[INV] Inicializace selhala, task ukoncen");
            vTaskDelete(nullptr);
            return;
        }

        TickType_t xLastWake = xTaskGetTickCount();
        for (;;) {
            vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(drv->_cfg.invPollMs));

            if (!drv->poll()) {
                Serial.println("[INV] Poll selhal, cekam 5s...");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

private:
    const Config&      _cfg;
    ModbusDeReCallback _dereCallback;
    ModbusClient*      _client = nullptr;
    InverterData       _data;
    SemaphoreHandle_t  _mutex;

    // Prepocita raw Modbus hodnotu a ulozi do _data
    void _applyRegister(const RegisterDef& reg, const uint16_t* raw) {
        int32_t value = 0;

        if (reg.count == 1) {
            value = reg.isSigned ? (int16_t)raw[0] : (int32_t)raw[0];
        } else {
            uint32_t combined = ((uint32_t)raw[0] << 16) | raw[1];
            value = reg.isSigned ? (int32_t)combined : (int32_t)combined;
        }

        int32_t scaled = (reg.gain != 0) ? (value * reg.multiply) / reg.gain : value;

        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (reg.mapTo) {
                case MAP_GRID:      _data.powerGrid        = scaled; break;
                case MAP_PV:        _data.powerPV          = scaled; break;
                case MAP_BATTERY:   _data.powerBattery     = scaled; break;
                case MAP_LOAD:      _data.powerLoad        = scaled; break;
                case MAP_SOC:       _data.soc              = (uint16_t)scaled; break;
                case MAP_SOH:       _data.soh              = (uint16_t)scaled; break;
                case MAP_STATUS:    _data.status           = (uint16_t)scaled; break;
                case MAP_OP_FLAG:   _data.operationFlag    = (uint32_t)scaled; break;
                case MAP_PV_TODAY:  _data.energyPvToday    = (uint32_t)scaled; break;
                case MAP_GRID_BUY:  _data.energyGridToday  = (uint32_t)scaled; break;
                case MAP_GRID_SELL: _data.energySoldToday  = (uint32_t)scaled; break;
                default: break;
            }
            xSemaphoreGive(_mutex);
        }
    }
};
