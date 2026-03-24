# CLAUDE_MODBUS.md – Solar HMI – Modbus stack
# Přilož spolu s CLAUDE.md + CLAUDE_STATE.md při práci s Modbus, InverterDriver, registry.

---

## Architektura

```
InverterDriver (FreeRTOS task, prio 2, stack 6144B)
  │
  ├── ModbusRTUClient   – RS485 přes UART0 (GPIO 0/1), DE/RE přes MCP23017 GPB7
  │     └── Serial1.setTX/setRX před begin() – earlephilhower API!
  │
  └── ModbusTCPClient   – WiFi TCP socket
        └── client.setTimeout(3000) před connect() – Pico WiFiClient API!
```

### Vztah k řídicí logice zásobníků
Inverter task (Core 1) čte data z měniče a zapisuje do SolarModel.
BoilerController::tick() čte ze SolarModel a rozhoduje o spínání relé.
Oba tasky jsou synchronizovány přes `invPollMs = BOILER_TICK_MS = 2000ms`.
Viz CLAUDE_BOILER.md pro detaily řídicí logiky.

### Proč dva transporty
- **RTU** = primární provoz (kabel RS485, spolehlivé)
- **TCP** = testování bez fyzického přístupu k měniči (simulátor na PC)

---

## Soubory

| Soubor            | Obsah                                                      |
|-------------------|------------------------------------------------------------|
| `ModbusClient.h`  | abstraktní base + ModbusRTUClient + ModbusTCPClient        |
| `InverterTypes.h` | InverterData, RegisterDef, RegisterMap, profily měničů     |
| `InverterDriver.h`| FreeRTOS task, polling logika, mutex, sdílená data         |

---

## ModbusClient API

```cpp
class ModbusClient {
    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual bool isConnected() = 0;
    virtual ModbusError readHoldingRegisters(
        uint8_t slaveId, uint16_t startAddr,
        uint8_t count, uint16_t* buf) = 0;
    virtual ModbusError writeSingleRegister(
        uint8_t slaveId, uint16_t addr, uint16_t value) = 0;
};

enum ModbusError : uint8_t {
    MODBUS_OK=0, MODBUS_ERR_TIMEOUT=1, MODBUS_ERR_CRC=2,
    MODBUS_ERR_EXCEPTION=3, MODBUS_ERR_NO_CONN=4,
    MODBUS_ERR_WRONG_RESP=5, MODBUS_ERR_FRAME=6
};
```

---

## Konfigurace (Config.h)

```cpp
uint8_t  invProfileIndex = 0;           // 0=Solinteg, 1=Sermatec
uint8_t  invTransport    = TRANSPORT_TCP;
uint8_t  invSlaveId      = 255;         // TCP: 255 (0xFF), RTU: typicky 1
uint32_t invBaudRate     = 9600;
uint8_t  invIp[4]        = {10,0,1,28};
uint16_t invTcpPort      = 502;
uint16_t invPollMs       = 2000;        // MUSÍ být shodné s BOILER_TICK_MS!
```

FRAM blok: 0x0500–0x050F (viz CLAUDE_FRAM.md).

---

## InverterDriver

```cpp
// Vytvoření instance (main.cpp):
InverterDriver gInverter(gConfig, nullptr);
// nullptr = DE/RE callback (TCP nepotřebuje)

// Pro RTU doplnit callback v main.cpp:
auto dereCallback = [](bool tx){ gMCP.setRS485Transmit(tx); };
InverterDriver gInverter(gConfig, dereCallback);

// Spuštění FreeRTOS tasku:
xTaskCreate(InverterDriver::task, "Inverter", 6144, &gInverter, 2, nullptr);

// Čtení dat (thread-safe kopie):
InverterData inv;
gInverter.getData(inv);

// Zápis registru:
gInverter.writeRegister(addr, value);
gInverter.setWorkMode(modeValue);  // registr 50000
```

### TCP retry chování
- `begin()` se opakuje každých `INVERTER_TCP_RETRY_MS=10000ms` dokud server nedostupný
- Task nikdy nekončí – připojí se jakmile simulátor/měnič nastartuje
- `begin()` maže starý `_client` před novým (bez memory leak)
- Po selhání poll(): pauza 5s + reset timing

### RTU chování
- `begin()` vždy uspěje (jen init UART) – selhání = fatální HW → task se ukončí

---

## InverterData – sdílená struktura

```cpp
struct InverterData {
    int32_t  powerGrid;       // síť celkem [W]: + odběr, - dodávka
    int32_t  powerPV;         // výkon PV [W]
    int32_t  powerBattery;    // baterie [W]: + vybíjení, - nabíjení
    int32_t  powerLoad;       // spotřeba domu [W]
    int32_t  phaseL1, phaseL2, phaseL3;  // fáze [W]: + dodávka, - odběr
    uint16_t soc, soh;        // [%]
    uint16_t status;          // 0=wait, 2=on-grid, 3=fault, 5=off-grid
    uint32_t operationFlag;
    uint32_t energyPvToday, energyGridToday, energySoldToday;  // [Wh]
    bool     valid;
    uint32_t lastUpdateMs;
    uint8_t  errorCount;
};
```

> Přístup přes `gInverter.getData(inv)` nebo `SolarModel::updateFromInverter(inv)`.

---

## Solinteg registrový profil (index 0)

Dle dokumentace **Modbus Register Table V00.17**.
Měnič: **Solinteg MHT-20K-40** – třífázový, asymetrický výkon na fázích.

### Znaménková konvence

| Registr              | Kladné (+)      | Záporné (−)    |
|----------------------|-----------------|----------------|
| Grid L1/L2/L3, total | dodávka do sítě | odběr ze sítě  |
| Battery (30258)      | vybíjení        | nabíjení       |
| PV (11028)           | výroba (≥ 0)    | –              |
| Load (31306)         | spotřeba (≥ 0)  | –              |

### Čtené registry (FC 03)

| Adresa   | Popis           | Typ | Gain | Výsledek   |
|----------|-----------------|-----|------|------------|
| 11000/01 | Grid total      | I32 | 1    | W          |
| 11028/29 | PV power        | U32 | 1    | W          |
| 30258/59 | Battery power   | I32 | 1    | W          |
| 31306/07 | Load power      | I32 | 1    | W          |
| 33000    | SOC             | U16 | 100  | %          |
| 33001    | SOH             | U16 | 100  | %          |
| 10105    | Inverter status | U16 | 1    | kód stavu  |
| 31005    | PV energy today | U16 | 10   | Wh (×100)  |
| 31001    | Grid buy today  | U16 | 10   | Wh (×100)  |
| 31000    | Grid sell today | U16 | 10   | Wh (×100)  |
| 10994/95 | Grid fáze L1    | I32 | 1    | W          |
| 10996/97 | Grid fáze L2    | I32 | 1    | W          |
| 10998/99 | Grid fáze L3    | I32 | 1    | W          |

**Slave ID:** 255 (0xFF) pro TCP, typicky 1 pro RTU.

### Energie – škálování
```cpp
REG_U16(31005, 10, 100, MAP_PV_TODAY)  // gain=10, multiply=100 → Wh
```

---

## Sermatec profil (index 1)

Zatím prázdný. Čeká na dokumentaci registrů.

---

## Testovací infrastruktura – Python simulátor

Soubor: `solinteg_simulator.py` (v kořeni projektu).

```bash
python solinteg_simulator.py                          # TCP port 502
python solinteg_simulator.py --port 5020              # bez admin
python solinteg_simulator.py --transport rtu --serial COM3
```

### Připojení Pica k simulátoru (TCP)
```cpp
invTransport = TRANSPORT_TCP;
invIp[]      = {10, 0, 1, 28};  // IP PC (ipconfig / ip addr)
invTcpPort   = 502;
invSlaveId   = 255;
```

### Škálování v simulátoru
```python
SCALE = {
    33000: 100,   # SOC → registr = % × 100
    33001: 100,   # SOH → registr = % × 100
    31005: 0.01,  # PV today → registr = Wh / 100
    31001: 0.01,  # Grid buy
    31000: 0.01,  # Grid sell
}
# Výkony [W] a fáze neškálovat – přímo ve wattech
```

### Presety simulátoru

| Preset     | PV      | Grid        | Baterie  |
|------------|---------|-------------|----------|
| Poledne ☀️ | 15 kW   | dodává      | nabíjí   |
| Ráno 🌅    | 3 kW    | odebírá     | nabíjí   |
| Večer 🌆   | malé    | odebírá     | vybíjí   |
| Noc 🌙     | 0       | odebírá     | vybíjí   |
| Přebytek ⚡ | 20 kW  | max dodávka | –        |
| Fault ❌   | 0       | 0           | status=3 |

---

## Kompatibilita – earlephilhower core

```cpp
// Serial1 – MUSÍ setTX/setRX před begin()
Serial1.setTX(MODBUS_TX_PIN);
Serial1.setRX(MODBUS_RX_PIN);
Serial1.begin(9600);

// WiFiClient timeout – MUSÍ setTimeout před connect()
_client.setTimeout(3000);
bool ok = _client.connect(ip, _port);
```

---

## Napojení na SolarData a BoilerController

```cpp
// taskHeartbeat – po každém poll:
InverterData inv;
gInverter.getData(inv);
SolarModel::updateFromInverter(inv);

// taskBoiler – každé BOILER_TICK_MS:
SolarData d;
SolarModel::get(d);
gBoilerCtrl->tick(d);  // řídicí logika zásobníků
```

---

## TODO

- [ ] Otestovat TCP komunikaci na fyzickém HW (Pico + simulátor na PC)
- [ ] Doplnit DE/RE callback pro RTU v main.cpp
- [ ] Implementovat FRAM persistence Modbus konfigurace (blok 0x0500)
- [ ] Sermatec profil – doplnit registry až bude dokumentace
