# CLAUDE_STATE.md – Aktuální stav projektu Solar HMI
# Přilož vždy spolu s CLAUDE.md.
# Aktualizuj sekci "Právě řešíme" a "Next" po každé session.

---

## Hotovo ✅

### Hardware drivery
- MCP23017 – relé 0–9, RS485 DE/RE, allRelaysOff(), graceful degradace bez čipu
- FM24CL64 – readByte/writeByte/readBlock/writeBlock/eraseRegion
- PCF85063A – getTime/setTime/setCalibration/calcOffset, CLKOUT vypnut
- PulseCounter – IRQ na GPIO 18–27, debounce, přepočet na Wh/kWh
- FiveWaySwitch – debounce, detekce hrany (ne držení), enum SwButton

### Systém
- FreeRTOS SMP funkční
- WiFi STA připojení + reconnect každých 30s v loop()
- NTP sync při startu + resync každých 6h + okamžitý resync z UI
- RTC synchronizace z NTP
- Watchdog (8s timeout, aktivován po dokončení bootu)
- Boot sekvence: Boot → Logo → Main
- BootScreen linux-style se HW scroll ST7789V
- ConfigManager::loadDefaults() funkční (loadFromFram/saveToFram TODO)
- HDO pin inicializován v setup() (GPIO 2, INPUT_PULLUP)

### UI stack
- ScreenManager – navigační stack hloubka 8, switchTo/replaceTo/goBack
- Header – záhlaví (čas, puntíky AP/STA/INV, alarm) + spodní lišta (relé)
- MainScreen – dashboard výroba/baterie/fáze L1/L2/L3, sprity bez blikání
- MenuScreen – scroll (MENU_VISIBLE=4)
- HistoryScreen – týdenní graf, 2 záložky, Sakamotův algoritmus pro den týdne
- DiagnosticScreen – 3 záložky (I/O, HW Status, Alarmy)
- SettingScreen – scroll, inline editace data/času, zápis do RTC, NTP resync z UI
- PasswordScreen – 4-místný PIN, 3 pokusy, přechod na UdPScreen
- UdPScreen – menu Instalace (Řízení aktivní, ostatní brzy)
- ControlScreen – konfigurace BoilerSystem (20 položek, 5 sekcí)
- DiscoveryScreen – auto-discovery zásobníků, statistické měření, progress bar
- BoilerDetailScreen – per zásobník: label, enable, allowedGridW, čas. okno

### Modbus
- ModbusRTUClient – FC03/FC06, CRC-16, DE/RE callback
- ModbusTCPClient – FC03/FC06, MBAP header, reconnect
- InverterDriver – FreeRTOS task, polling, mutex, TCP retry loop
- Solinteg profil – L1/L2/L3 z reálných registrů (10994/10996/10998)
- SolarData – sdílená struktura Core 0 ↔ Core 1, thread-safe mutex
- Python simulátor – TCP + RTU, GUI s presety a dynamic simulací

### Řídicí logika zásobníků
- BoilerConfig.h – BoilerState (8 stavů vč. BOILER_SLOT_DONE), HdoMode (3 režimy),
  BoilerConfig (48B/zásobník), BoilerSystem (vč. slotDurationMin, slotCooldownMin),
  BoilerRuntime (vč. slotFull příznak), BoilerDayStats,
  helper funkce hasSufficientPower() / powerDropped()
- BoilerController.h – kompletní stavový automat, round-robin,
  comfort timer, soft-start, rotační ohřev (slot logika),
  detekce plného zásobníku ze změny fáze,
  recheck STANDBY zásobníků s rozloženými timery, HDO logika,
  zombie detektor, export stavu do SolarModel
- taskBoiler – FreeRTOS task, volá tick() každé BOILER_TICK_MS (2s)
- ControlScreen – sekce Rotacni ohrev: Slot max, Slot cooldown

---

## Rozpracováno 🔧

### FRAM persistence (prioritní)
- `ConfigManager::loadFromFram()` – zatím volá loadDefaults()
- `ConfigManager::saveToFram()` – zatím jen Serial výpis
- Magic byte 0x0000 – detekce prvního spuštění není implementována
- `_shouldBlockHDO()` – vrací vždy false, čeká na statistiky v FRAM
- `_updateStats()` – prázdná implementace, čeká na FRAM mapu
- Runtime stav zásobníků (gBoilerRt[]) se nenačítá z FRAM při startu
- DiscoveryScreen `_saveResult()` – ukládá jen do RAM, ne do FRAM
- BoilerDetailScreen – ukládá jen do RAM, ne do FRAM

### Modbus testování
- TCP komunikace se simulátorem na HW zatím neověřena fyzicky
- DE/RE callback pro RTU je nullptr – doplnit pro RTU provoz:
  ```cpp
  auto cb = [](bool tx){ gMCP.setRS485Transmit(tx); };
  InverterDriver gInverter(gConfig, cb);
  ```

### Screeny napojené na FRAM
- HistoryScreen::loadFromFRAM() – používá testovací data
- PasswordScreen::loadFromFRAM() – PIN vždy výchozí 0000
- SettingScreen – Téma/Timeout/Backlight/NTP se neukládají do FRAM

### DiagnosticScreen
- I/O záložka: pulzní vstupy IO1–IO10 jako placeholder false
- Alarmy záložka: prázdná, AlarmManager není implementován

---

## Next kroky 📋

### Priorita HIGH
- [ ] Přepracovat FRAM mapu (zahrnuje nová data: BoilerConfig×10,
      BoilerSystem, BoilerRuntime×10, statistiky, HDO konfigurace)
- [ ] Implementovat loadFromFram() / saveToFram() pro celý Config
      + BoilerConfig[] + BoilerSystem
- [ ] Otestovat Modbus TCP se simulátorem na fyzickém HW

### Priorita MED
- [ ] DE/RE callback doplnit do main.cpp pro RTU provoz
- [ ] HistoryScreen napojit na FM24CL64 – denní součty do FRAM
- [ ] PasswordScreen napojit na FM24CL64 – load/save PIN
- [ ] SettingScreen – uložení nastavení do FRAM
- [ ] BoilerDetailScreen – uložení do FRAM
- [ ] DiscoveryScreen – uložení výsledků do FRAM
- [ ] DiagnosticScreen I/O – napojit IO1–IO10 na PulseCounter
- [ ] _shouldBlockHDO() – implementovat po přepracování FRAM mapy

### Priorita LOW
- [ ] AlarmManager – struktura + DiagnosticScreen Alarmy záložka
- [ ] LogoScreen – PNG z LittleFS
- [ ] THEME_INDUSTRIAL a THEME_LIGHT
- [ ] UdPScreen podmenu Serial/Network/MQTT/Střídač

---

## Známé pasti a gotchas – WiFi a FreeRTOS na Pico 2W

- **Pořadí inicializace WiFi a tasků** – na Pico 2W earlephilhower core:
  - WiFi.mode() → WiFi.begin() → NTP → SolarModel::begin() → xTaskCreate()
  - Tasky se spouštějí AŽ PO dokončení WiFi připojení a NTP syncu
  - Toto je OPAČNĚ než běžné pravidlo "xTaskCreate před WiFi.begin()"
  - Ověřeno funkční na Pico 2W – nedodržení způsobí zaseknutí při WiFi.mode()

- **PulseCounter způsobuje zaseknutí WiFi** – gpio IRQ handlery registrované
  v PulseCounter::begin() kolidují s CYW43 WiFi inicializací.
  PulseCounter byl z projektu odstraněn – měření spotřeby bytů se řeší
  přes fázový tok ze Solinteg měniče (phaseL1/L2/L3 v SolarData).

## Známé pasti a gotchas

- **Theme.splitline** – 9. pole struct Theme (0xFD20), přidáno pro
  dělící čáry v MainScreen. Starší verze Theme struct toto pole nemají!
- **MainScreen !d.valid** – pomlčky "---" mají barvu t->text (bílá)
- **DiagnosticScreen _drawHW()** – "Invertor" zobrazuje "OK"/"OFFLINE"
- **InverterDriver TCP retry** – begin() maže starý _client před novým
- **Config.h includovat POUZE v main.cpp** – definuje přímé instance
  gConfig, gBoilerCfg[], gBoilerSys → multiple definition jinak
- **taskHeartbeat nesmí kreslit** – výhradně uiLoop()
- **BoilerController recheck** – rozložení timerů: T + recheckMin + idx×2min
  zabraňuje simultánnímu rechecku všech zásobníků
- **Discovery podmínky** – zásobníky musí být studené (termostat nesepnut),
  jinak delta fáze = 0 a měření selže
- **BOILER_TICK_MS** – musí být shodné s invPollMs v gConfig (oboje 2000ms)
  jinak tick() pracuje se staršími daty než čeká
