# CLAUDE_ROADMAP.md – Solar HMI – Plánované funkce
# Přilož spolu s CLAUDE.md když plánuješ novou funkcionalitu nebo odhaduješ náročnost.
# Není potřeba přikládat při běžné práci na existujícím kódu.

---

## Přehled priorit

| Priorita | Oblast                        | Odhad náročnosti | Stav      |
|----------|-------------------------------|------------------|-----------|
| 🔴 HIGH  | FRAM mapa – přepracování      | střední          |           |
| 🔴 HIGH  | FRAM persistence – dokončení  | střední          |           |
| 🔴 HIGH  | Modbus TCP test na HW         | nízká            |           |
| 🟡 MED   | BoilerController – dokončení  | střední          |           |
| 🟡 MED   | Web server + rozhraní         | vysoká           |           |
| 🟡 MED   | MQTT klient (runtime)         | střední          |           |
| 🟡 MED   | SSR řízení výkonu (burst)     | střední          |           |
| 🟡 MED   | RTC kalibrace z NTP           | nízká            |           |
| 🟡 MED   | WiFi AP dual mode             | střední          |           |
| 🟢 LOW   | Více témat displeje           | nízká            |           |
| 🟢 LOW   | AlarmManager                  | střední          |           |
| 🟢 LOW   | LogoScreen PNG                | nízká            |           |
| 🟢 LOW   | Sermatec Modbus profil        | střední          |           |

### Hotovo ✅ (tato session)

- SerialScreen – Modbus konfigurace (TCP/RTU, profil, IP editor, restart dialog)
- InverterScreen – parametry elektrárny (FVE, baterie, provozní limity)
- MqttScreen – MQTT konfigurace (broker, auth, topic, interval)
- HistoryScreen – přepracován: 3 záložky (Výroba, Spotřeba, Zásobníky), DaySummary struct
- Header – footer respektuje numBoilers, kostičky vycentrované
- DiagnosticScreen – navigační lišta místo kostiček
- UdP menu – všech 5 položek aktivních
- Config.h – RTU parametry (dataBits, parity, stopBits), elektrárna, MQTT
- ModbusClient.h – RTU přijímá parametry sériové linky

---

## 🔴 FRAM mapa – přepracování

Stávající mapa byla navržena před implementací řídicí logiky zásobníků.
Přibyla nová data která nemají v mapě místo. Přepracování je nutné
před implementací FRAM persistence pro zásobníky.

### Co přibude
- BoilerConfig × 10 (48B každá = 480B)
- BoilerSystem (~60B)
- BoilerRuntime × 10 (~15B každý = 150B)
- BoilerDayStats × 10 × 14 dní (8B × 140 = 1120B, cyklický buffer)
- DaySummary × 7 dní (16B × 7 = 112B)
- HDO konfigurace a history spínání distributora
- Elektrárna konfigurace (pvPowerKwp10, batteryKwh10, ...)
- MQTT konfigurace (broker, user, pass, topic, interval)
- RTU parametry (dataBits, parity, stopBits)

### Principy návrhu
- Logické bloky pohromadě, zarovnané adresy
- Rezerva za každým blokem
- Magic byte + verze schématu (detekce nekompatibility při upgrade FW)

### Plánované bloky

```
0x0000–0x00FF  Systém (256B)
               magic byte, verze schématu, téma, PIN, flagy

0x0100–0x02FF  Config WiFi + NTP + RTC offset (512B)

0x0300–0x04FF  Config Modbus/Inverter + RTU parametry (512B)

0x0500–0x06FF  BoilerSystem konfigurace (512B)

0x0700–0x08FF  BoilerConfig × 10 × 48B = 480B (512B blok)

0x0900–0x09FF  BoilerRuntime × 10 (256B)

0x0A00–0x0EFF  BoilerDayStats cyklický buffer
               14 dní × 10 zásobníků × 8B = 1120B

0x0F00–0x0FFF  DaySummary × 7 dní × 16B = 112B (256B blok)

0x1000–0x10FF  Elektrárna konfigurace (256B)

0x1100–0x11FF  MQTT konfigurace (256B)

0x1200–0x13FF  SSR konfigurace (budoucnost, 512B)

0x1400–0x17FF  HDO monitor / history spínání (1024B)

0x1800–0x1FFF  Rezerva (~2 KB)
```

---

## 🔴 FRAM persistence – dokončení

Po přepracování mapy implementovat:

- `ConfigManager::loadFromFram()` – WiFi, Modbus, téma, RTC offset,
  BoilerSystem, BoilerConfig[], BoilerRuntime[], elektrárna, MQTT, RTU
- `ConfigManager::saveToFram()` – kompletní uložení
- Magic byte detekce prvního spuštění
- SettingScreen – uložení Téma/Timeout/Backlight/NTP
- BoilerDetailScreen – uložení per zásobník konfigurace
- DiscoveryScreen – uložení výsledků Discovery
- HistoryScreen – DaySummary[] do FRAM
- PasswordScreen – load/save PIN
- SerialScreen – uložení Modbus konfigurace
- InverterScreen – uložení parametrů elektrárny
- MqttScreen – uložení MQTT konfigurace

---

## 🔴 Modbus TCP test na HW

Kód je hotový, TCP komunikace se simulátorem zatím neověřena fyzicky.

```bash
# Na PC spustit simulátor:
python solinteg_simulator.py --port 502

# V Config.h nastavit:
invTransport = TRANSPORT_TCP;
invIp[]      = {IP_ADRESA_PC};
invTcpPort   = 502;
invSlaveId   = 255;
```

Pico a PC musí být ve stejné WiFi síti.

---

## 🟡 BoilerController – dokončení zbývajících funkcí

### _shouldBlockHDO()
Implementovat průměr FVE ze statistik v FRAM:
```cpp
bool _shouldBlockHDO() const {
    if (_sys.hdoMode == HDO_ALWAYS) return false;
    if (_sys.hdoMode == HDO_NEVER)  return true;
    // HDO_ADAPTIVE: průměr posledních hdoAdaptiveDays dní
    // float avg = loadAvgFveFromFram();
    // return avg >= _sys.hdoThresholdKwh;
    return false;  // TODO
}
```

### _updateStats()
Akumulace denních statistik v RAM, flush do FRAM každý nový den:
```cpp
// Pro každý HEATING zásobník každý tick:
//   phaseL > 0 → energySolarWh += powerW * BOILER_TICK_MS / 3600000
//   phaseL < 0 → energyGridWh  += powerW * BOILER_TICK_MS / 3600000
// Detekce nového dne z RTC → flush + reset čítačů
```

### Detekce funkčnosti HDO pinu
Pokud pin nebyl aktivní v posledních 24h → přepnout na časovou zálohu.
Uložit timestamp posledního aktivního stavu do FRAM.

### DiagnosticScreen I/O záložka
Napojit IO1–IO10 na `gPulse.getEnergyWh(i)` nebo živý stav IRQ pinu.

---

## 🟡 Web server + rozhraní

HTTP server přístupný z prohlížeče – live data + konfigurace
bez nutnosti ovládání přes displej.

**Architektura:**
- Synchronní `WebServer` z earlephilhower core
- LittleFS pro statické soubory (HTML, CSS, JS)
- `server.handleClient()` v loop() na Core 0
- JS polluje REST API každé 2–3s

**REST API:**
- `GET /` → index.html z LittleFS
- `GET /api/data` → JSON SolarData + stav zásobníků
- `GET /api/config` → JSON konfigurace
- `POST /api/config` → uložit do FRAM (budoucnost)
- `GET /api/boilers` → stav + statistiky zásobníků
- `GET /api/history` → DaySummary[7]

**Soubory:**
- `WebServerManager.h` – setup, routes, JSON builder
- `data/index.html` – SPA dashboard
- `data/style.css` – tmavý motiv
- `data/app.js` – fetch, polling, DOM update

**Fáze 1:** Readonly dashboard (monitoring)
**Fáze 2:** POST endpointy pro konfiguraci

---

## 🟡 MQTT klient (runtime)

Konfigurace je hotová (MqttScreen), zbývá runtime klient.

**Knihovna:** PubSubClient (jednoduchá, ověřená na Pico)

**Data odesílaná každých N sekund (mqttIntervalSec):**
- powerPV, powerGrid, powerBattery, powerLoad, soc
- phaseL1/L2/L3
- energyPvToday, energyGridToday, energySoldToday
- relayOn[10], boilerState[10]
- Denní statistiky zásobníků

**Implementace:**
- `MqttManager.h` – PubSubClient wrapper, reconnect, publish
- Volat z loop() na Core 0
- Podmíněná kompilace pokud mqttEn == false → nic nedělej

---

## 🟡 SSR řízení výkonu (burst-mode)

Plynulá regulace výkonu střídavé zátěže přes SSR moduly
se spínáním v nule (zero-cross). Burst-mode PWM – celé půlvlny.

### Hardware
- 3× SSR modul (typ SSR-40DA nebo podobný)
- Řídicí vstup: 3–32V DC, GPIO z Pica přes optočlen
- GPIO 18, 19, 20 (volné po odstranění PulseCounter)
- PWM perioda 1–10s, duty cycle 0–100%
- Spínání v nule → žádné EMI problémy

### Konfigurace (SsrConfig)
```cpp
struct SsrConfig {
    uint8_t  gpio;           // GPIO pin (18/19/20)
    uint8_t  phase;          // fáze: 1=L1, 2=L2, 3=L3
    uint16_t maxPowerW;      // max výkon zátěže [W]
    bool     enabled;
    uint16_t pwmPeriodMs;    // perioda PWM [ms], výchozí 2000
    char     label[16];      // popis, např. "Bojler chodba"
};
```

### Řídicí logika – priorita
1. SSR plynule reguluje přebytek (0–100% výkonu) – jemnější
2. Když SSR nestíhá (přebytek > kapacita SSR) → zapni on/off zásobník
3. Když přebytek klesne → nejdřív vypni zásobník, pak sniž SSR

### Softwarové požadavky
- `SsrController.h` – výpočet duty cycle z přebytku na fázi, HW PWM
- Rozšíření BoilerController o koordinaci s SSR
- Nový screen v UdP pro konfiguraci SSR
- RP2350 má HW PWM slice → žádné zatížení CPU
- FRAM: 3 × ~48B konfigurace (blok 0x1200)

### Odhad náročnosti
Střední – HW PWM je jednoduchý, složitější je koordinace s on/off logikou.

---

## 🟡 RTC kalibrace z NTP drift měření

- Uložit Unix timestamp posledního NTP syncu do FRAM
- Po dalším syncu: `drift_ppm = (rtc_unix - ntp_unix) / elapsed_s * 1e6`
- `gRTC.setCalibration(PCF85063A::calcOffset(drift_ppm))`
- Uložit offset do FRAM 0x0300 (stávající adresa)
- Zobrazit v DiagnosticScreen HW Status: offset + drift
- Rozsah korekce: ±69.8 ppm (krok ±1.09 ppm)

---

## 🟡 WiFi AP dual mode

- `WiFi.mode(WIFI_AP_STA)`
- `WiFi.softAP(ssid, pass, channel)`
- Boot screen: zobrazit obě IP
- NetworkScreen již má AP konfiguraci hotovou

---

## 🟢 Více témat displeje

- Doplnit `THEME_LIGHT` a `THEME_INDUSTRIAL` v Theme.h
- Aktualizovat `THEMES[]` a `THEME_COUNT = 3`
- SettingScreen → Téma → zápis do FRAM 0x0001

---

## 🟢 AlarmManager

Centrální správa alarmů – ukládá, zobrazuje, signalizuje.

```cpp
struct AlarmEntry {
    uint8_t  code;
    uint32_t timestamp;
    bool     active;
};
```

Zdroje alarmů:
- invStatus==3 (fault měniče) – aktuálně jen bliká ⚠ v záhlaví
- Zombie zásobník (BOILER_ALARM)
- errorCount > threshold
- RTC invalid
- FRAM chyba
- WiFi dlouhodobě nedostupná

Napojit na DiagnosticScreen záložka Alarmy.

---

## 🟢 LogoScreen PNG

- PNG soubor `/logo.png` do LittleFS
- `tft.drawPng("/logo.png", 0, 0, 320, 240)` – LovyanGFX nativně
- `pio run --target uploadfs`

---

## 🟢 Sermatec Modbus profil (index 1)

- Čeká na dokumentaci registrů Sermatec SMT-10K-TL
- Doplnit `SERMATEC_REGS[]` v InverterTypes.h
- SerialScreen → výběr profilu již funkční

---

## 🔵 Podmenu UdP – stav

| Podmenu  | Co řídí                                        | Stav      |
|----------|------------------------------------------------|-----------|
| Řízení   | BoilerSystem, Discovery, detail zásobníků      | hotovo ✅  |
| Serial   | Modbus transport, profil, RTU/TCP, IP          | hotovo ✅  |
| Network  | WiFi STA/AP, NTP, Hostname                     | hotovo ✅  |
| MQTT     | Broker, port, auth, topic, interval            | hotovo ✅  |
| Střídač  | FVE, baterie, provozní limity                  | hotovo ✅  |
