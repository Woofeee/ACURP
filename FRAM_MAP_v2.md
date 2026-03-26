# FRAM MAPA – NÁVRH v2
# FM24CL64: 8192 bajtů (0x0000–0x1FFF)
# Každý blok: fixní adresa, magic+verze na začátku, rezerva na konci
# Přidání pole = zvýšení verze bloku, adresa dalšího bloku se NEMĚNÍ

# ═══════════════════════════════════════════════════════════════
#  ANALÝZA – co všechno potřebuje FRAM
# ═══════════════════════════════════════════════════════════════

# ─── BLOK 0: SYSTÉM ──────────────────────────────────────────
# Globální identifikace, téma, PIN, flagy
#
#   magic            1B   (0xAC = platná FRAM, jinak factory reset)
#   schemaVersion    1B   (globální verze celé mapy)
#   themeIndex       1B
#   pin[4]           4B   (PIN kód, 4 číslice 0–9)
#   displayTimeout   1B   (podsvícení timeout [min])
#   displayBright    1B   (podsvícení [%])
#   rtcCalOffset     1B   (int8_t, kalibrační offset RTC)
#   numBoilers       1B
#   ─────────────────────
#   Aktuální:       12B
#   Rezerva:       116B
#   Blok celkem:   128B

# ─── BLOK 1: WiFi + NTP ──────────────────────────────────────
# Síťová konfigurace – STA, AP, NTP, hostname
#
#   blockMagic       1B
#   blockVersion     1B
#   --- STA ---
#   wifiStaEn        1B
#   wifiStaSsid     32B
#   wifiStaPass     48B
#   wifiStaDhcp      1B
#   wifiStaIp        4B
#   wifiStaMask      4B
#   wifiStaGw        4B
#   wifiStaDns       4B
#   --- AP ---
#   wifiApEn         1B
#   wifiApSsid      20B
#   wifiApPass      16B
#   wifiApChannel    1B
#   wifiApHidden     1B
#   wifiApIp         4B
#   wifiApMask       4B
#   wifiApDhcpStart  4B
#   wifiApDhcpEnd    4B
#   --- NTP ---
#   ntpEn            1B
#   ntpServer       32B
#   ntpTz           48B
#   ntpResyncSec     4B
#   --- Hostname ---
#   hostname        24B
#   ─────────────────────
#   Aktuální:      261B
#   Rezerva:       251B
#   Blok celkem:   512B

# ─── BLOK 2: MODBUS / STŘÍDAČ KOMUNIKACE ─────────────────────
# SerialScreen konfigurace
#
#   blockMagic       1B
#   blockVersion     1B
#   invProfileIndex  1B
#   invTransport     1B
#   invSlaveId       1B
#   invBaudRate      4B
#   invIp            4B
#   invTcpPort       2B
#   invPollMs        2B
#   invDataBits      1B
#   invParity        1B
#   invStopBits      1B
#   ─────────────────────
#   Aktuální:       22B
#   Rezerva:       106B
#   Blok celkem:   128B

# ─── BLOK 3: ELEKTRÁRNA ──────────────────────────────────────
# InverterScreen konfigurace – parametry FVE a baterie
#
#   blockMagic       1B
#   blockVersion     1B
#   pvPowerKwp10     2B   (0.1 kWp)
#   batteryKwh10     2B   (0.1 kWh)
#   pvPhaseCount     1B
#   maxExportW       2B
#   minSocGlobal     1B
#   nightCharge      1B
#   ─────────────────────
#   Aktuální:       13B
#   Rezerva:       115B
#   Blok celkem:   128B

# ─── BLOK 4: MQTT ────────────────────────────────────────────
# MqttScreen konfigurace
#
#   blockMagic       1B
#   blockVersion     1B
#   mqttEn           1B
#   mqttBrokerIp     4B
#   mqttPort         2B
#   mqttUser        24B
#   mqttPass        24B
#   mqttTopic       32B
#   mqttIntervalSec  2B
#   ─────────────────────
#   Aktuální:       93B
#   Rezerva:        35B
#   Blok celkem:   128B

# ─── BLOK 5: BOILER SYSTEM ───────────────────────────────────
# ControlScreen konfigurace – systémové parametry řízení
#
#   blockMagic       1B
#   blockVersion     1B
#   numBoilers       1B   (kopie, synchronizováno s Blok 0)
#   minOnTimeSec     2B
#   minOffTimeSec    2B
#   switchDelaySec   2B
#   slotDurationMin  2B
#   slotCooldownMin  2B
#   recheckIntervalMin 2B
#   recheckDurationSec 2B
#   solarMinSurplusW 2B
#   maxHeatTimeMin   2B
#   minSocForBoilers 1B
#   seasonWinter     1B
#   hdoMode          1B
#   hdoPinGpio       1B
#   hdoStart1        1B
#   hdoEnd1          1B
#   hdoStart2        1B
#   hdoEnd2          1B
#   hdoThresholdKwh  4B   (float)
#   hdoAdaptiveDays  1B
#   hdoTopupEnable   1B
#   hdoTopupStart    1B
#   hdoTopupEnd      1B
#   ─────────────────────
#   Aktuální:       38B
#   Rezerva:        90B
#   Blok celkem:   128B

# ─── BLOK 6: BOILER CONFIG × 10 ─────────────────────────────
# BoilerDetailScreen + DiscoveryScreen – per zásobník konfigurace
# 10 × BoilerConfig (48B each) = 480B
#
#   blockMagic       1B
#   blockVersion     1B
#   boilerConfig[10]:
#     phase          1B
#     powerW         2B
#     discoveryDone  1B
#     enabled        1B
#     label         16B
#     allowedGridW   2B
#     timeStart      1B
#     timeEnd        1B
#     reserved      23B   (do 48B per zásobník)
#   ─────────────────────
#   Aktuální:      482B
#   Rezerva:        30B
#   Blok celkem:   512B

# ─── BLOK 7: BOILER RUNTIME × 10 ────────────────────────────
# Provozní stav zásobníků – přežívá restart
# Zapisuje se při každé změně stavu (řídké, max desítky za den)
#
#   blockMagic       1B
#   blockVersion     1B
#   boilerRuntime[10]:
#     state          1B
#     lastHeatedAt   4B   (Unix timestamp)
#     lastOffAt      4B   (Unix timestamp)
#     stateEnteredAt 4B   (Unix timestamp)
#     recheckIndex   1B
#     slotFull       1B
#     reserved       1B   (do 16B per zásobník)
#   ─────────────────────
#   Aktuální:      162B
#   Rezerva:        94B
#   Blok celkem:   256B

# ─── BLOK 8: ŘÍZENÍ PERSIST ─────────────────────────────────
# Globální runtime řízení – přežívá restart
# Soft-start timery, HDO monitoring, denní akumulátory
#
#   blockMagic           1B
#   blockVersion         1B
#   --- Soft-start ---
#   lastSwitchOnAt[3]   12B   (Unix timestamp per fáze)
#   --- HDO monitoring ---
#   lastHdoState         1B
#   lastHdoActiveAt      4B   (Unix timestamp)
#   --- Denní akumulátory (flush do stats bloků při novém dni) ---
#   accumSolarWh[10]    20B   (energie ze soláru per zásobník [Wh])
#   accumGridWh[10]     20B   (energie ze sítě per zásobník [Wh])
#   accumSwitchCnt[10]  10B   (počet sepnutí per zásobník)
#   accumPvWh            2B   (celková výroba FVE [Wh])
#   accumLoadWh          2B   (celková spotřeba [Wh])
#   accumGridBuyWh       2B   (koupeno ze sítě [Wh])
#   accumGridSellWh      2B   (prodáno do sítě [Wh])
#   --- Stats buffer index ---
#   statsDayIndex        1B   (0–13, pozice v BoilerDayStats bufferu)
#   summaryDayIndex      1B   (0–6, pozice v DaySummary bufferu)
#   statsDay             1B   (den posledního flushe)
#   statsMonth           1B   (měsíc)
#   statsYear            2B   (rok)
#   ─────────────────────
#   Aktuální:           85B
#   Rezerva:           171B
#   Blok celkem:       256B

# ─── BLOK 9: BOILER DAY STATS ───────────────────────────────
# Denní statistiky per zásobník – cyklický buffer 14 dní
# 10 zásobníků × 14 dní × 8B = 1120B
#
#   blockMagic       1B
#   blockVersion     1B
#   stats[14][10]:           (14 dní × 10 zásobníků)
#     energySolarWh  2B
#     energyGridWh   2B
#     switchCount    1B
#     alarmCount     1B
#     reserved       2B     (do 8B per záznam)
#   ─────────────────────
#   Aktuální:     1122B
#   Rezerva:       158B
#   Blok celkem:  1280B

# ─── BLOK 10: DAY SUMMARY ───────────────────────────────────
# Denní souhrn elektrárny – cyklický buffer 7 dní
# 7 × 16B = 112B
#
#   blockMagic       1B
#   blockVersion     1B
#   summary[7]:
#     pvWh           2B
#     loadWh         2B
#     gridBuyWh      2B
#     gridSellWh     2B
#     boilerSolarWh  2B
#     boilerGridWh   2B
#     reserved       4B     (do 16B per den)
#   ─────────────────────
#   Aktuální:      114B
#   Rezerva:       142B
#   Blok celkem:   256B

# ─── BLOK 11: SSR (budoucnost) ───────────────────────────────
# Konfigurace SSR výstupů pro plynulou regulaci
# 3 × SsrConfig
#
#   blockMagic       1B
#   blockVersion     1B
#   ssrConfig[3]:
#     gpio           1B
#     phase          1B
#     maxPowerW      2B
#     enabled        1B
#     pwmPeriodMs    2B
#     label         16B
#     reserved       9B     (do 32B per SSR)
#   ─────────────────────
#   Aktuální:       98B
#   Rezerva:       158B
#   Blok celkem:   256B

# ─── BLOK 12: REZERVA ───────────────────────────────────────
# Volný prostor pro budoucí rozšíření
# Web server config, InfluxDB, nové funkce...


# ═══════════════════════════════════════════════════════════════
#  ADRESNÍ MAPA
# ═══════════════════════════════════════════════════════════════
#
#  Blok  Název               Adresa      Velikost  Využito  Rezerva
#  ────  ──────────────────  ──────────  ────────  ───────  ───────
#   0    Systém              0x0000      128B       12B     116B
#   1    WiFi + NTP          0x0080      512B      261B     251B
#   2    Modbus / Serial     0x0280      128B       22B     106B
#   3    Elektrárna          0x0300      128B       13B     115B
#   4    MQTT                0x0380      128B       93B      35B
#   5    Boiler System       0x0400      128B       38B      90B
#   6    Boiler Config ×10   0x0480      512B      482B      30B
#   7    Boiler Runtime ×10  0x0680      256B      162B      94B
#   8    Řízení Persist      0x0780      256B       85B     171B
#   9    Boiler DayStats     0x0880     1280B     1122B     158B
#  10    Day Summary         0x0D80      256B      114B     142B
#  11    SSR (budoucnost)    0x0E80      256B       98B     158B
#  12    REZERVA             0x0F80     4224B        0B    4224B
#  ────  ──────────────────  ──────────  ────────  ───────  ───────
#                            CELKEM     8192B     2502B    5690B
#
#  Využití: 30.5% (2502B z 8192B)
#  Volný prostor: 69.5% (5690B)
#
# ═══════════════════════════════════════════════════════════════
#  PRAVIDLA
# ═══════════════════════════════════════════════════════════════
#
#  1. Startovní adresa bloku je FIXNÍ – nikdy se nemění
#  2. Přidání pole do bloku = zvýšit blockVersion
#  3. Čtení: stará verze → načti co znáš, nová pole = defaults
#  4. Magic byte bloku (0xAC) se zapisuje JAKO POSLEDNÍ
#  5. Globální magic 0x0000 = 0xAC → FRAM je platná
#     Nesedí → factory reset celé FRAM → loadDefaults + saveAll
#  6. Per-blok magic nesedí → reset JEN toho bloku
#  7. Zápis akumulátorů (Blok 8) max 1× za 5 min (ne každý tick)
#  8. BoilerRuntime (Blok 7) zapisovat jen při změně stavu
#  9. Konfigurace (Bloky 0–6) zapisovat jen při uložení z UI
