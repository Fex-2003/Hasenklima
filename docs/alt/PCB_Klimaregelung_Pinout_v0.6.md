# Klimaregelung – PCB-Bauteilübersicht, Anschlussplan und ESP32-Pinout

**Dokumentversion:** 0.6 – konsolidierte Anschlussplanung  
**Datum:** 15.11.2026  
**Grundlage:** KiCad-Screenshot `image(4).png`, Anschlussliste v0.5 und Pflichtenheft Klimaregelung v1.2  
**Status:** **Anschlussplanung aktualisiert; Schaltplan noch NICHT fertigungsfreigegeben.** Die ESP32-GPIOs sind im Screenshot noch unbeschaltet. Der ADC-Schutz muss korrigiert und die offenen Hardwareprüfungen müssen abgeschlossen werden.

## 0. Verbindlichkeitskennzeichnung

- **IST (Sichtprüfung):** Im Screenshot erkennbar; ein Screenshot ersetzt keine KiCad-Netzliste und keinen ERC.
- **SOLL:** Verbindlich vorgeschlagene Verdrahtung, im Screenshot noch nicht realisiert.
- **DNP:** Bestückungsplatz vorsehen, zunächst **nicht** bestücken.
- **PRÜFEN:** Abhängig von exakten Modulen, Leitungen, Lasten oder Footprints.
- Alle als Widerstand, Kondensator, Transistor oder Diode benannten kleinen Kästchen sind im Screenshot noch **generische Anschlussplätze**. Für die Fertigung müssen geeignete Symbole, reale Bauteilwerte, Pin-Nummern und Footprints hinterlegt werden.

## 1. Änderungsstand gegenüber v0.5

| Thema             | Aktueller Stand / Entscheidung                                                                                                                                                                                                  |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| DS18B20           | **Zwei getrennte 1-Wire-Busse**: `DATA_Plate` (U8, neun Fühler) und `DATA_Tank` (U9, ein Fühler). Keine elektrische Verbindung der Datenleitungen.                                                                              |
| Pullups           | R11 = 4,7 kΩ nach `3V3` für Platte, R12 = 4,7 kΩ nach `3V3` für Tank. **Beide bestücken.**                                                                                                                                      |
| ESP32             | GPIO13 = Platte, GPIO14 = Tank; bisher noch **SOLL**. Andere geplante Pins aus v0.5 bleiben erhalten.                                                                                                                           |
| ADC               | R13 als Serienwiderstand hinzugefügt; gegenwärtig hängen D4/C10 noch **vor** R13. **SOLL:** D4/C10 hinter R13 an `VIN_ADC` legen.                                                                                               |
| ADC-Schutz        | D4 für **2,5-V-Präzisions-Shuntreferenz** (z. B. TI LM4040D25IDBZR) vorsehen; aktueller zweipoliger Platz ist dafür nicht passend.                                                                                              |
| BME280            | Zwei Module auf demselben I²C-Bus; Adressen 0x76/0x77 werden am konkreten Modul getestet. Noch keine Freigabe.                                                                                                                  |
| Heizung           | Übertemperaturabschaltung derzeit **ausschließlich über ESP32**; ein unabhängig wirkender thermischer Abschalter ist **nicht vorhanden**. Das ist ein bewusst offenes Sicherheitsrisiko, keine erfüllte Sicherheitsanforderung. |
| Buck U4/U5        | EN-Eingänge (je Pin 4) sind `NC` und sollen intern auf „Ein“ liegen; tatsächliches Modulverhalten prüfen.                                                                                                                       |
| 12-V-Bypass       | J7 überbrückt U5 nur bei 12-V-VIN und **elektrisch getrenntem/entferntem U5**.                                                                                                                                                  |
| Reserveversorgung | J2 (12 V) oder J3 (VIN), niemals gleichzeitig.                                                                                                                                                                                  |
| Gate-Widerstände  | Aufdruck `22O` in der Zeichnung bedeutet **22 Ω**, nicht 220 Ω. Im KiCad-Wertefeld vorzugsweise `22R`.                                                                                                                          |

## 2. Kritische Punkte vor einer Fertigungsfreigabe

1. **ADC-Schutz (SOLL):** Im Screenshot liegt `VIN → R10 → VIN_DIV → R13 → VIN/16 (ADC)`; **D4 und C10 liegen derzeit noch am Knoten `VIN_DIV`**. Für den vorgesehenen Schutz gehören **D4 und C10 an `VIN_ADC` hinter R13**. R13 begrenzt dann auch den Strom bei einem Fehler des oberen Teilerwiderstands. Mit einem 2-poligen Platz ist der bevorzugte LM4040-SOT-23 noch nicht korrekt abgebildet.
2. **GPIO-/I²C-Verbindungen (SOLL):** Die ESP32-Signalpins sind im Bild absichtlich noch unbeschaltet. Die vorgeschlagenen Labels und GPIOs aus Kapitel 5 müssen tatsächlich verbunden werden. Die Signallabels der drei I²C-Sensoren sind Anschlussplanung, keine nachgewiesene durchgehende Busverdrahtung.
3. **Sensormodule (PRÜFEN):** Der **nackte** SHT45/BME280 ist nicht mit 5 V versorgbar. Nur Module mit geeigneter Spannungsregelung dürfen an `5V` liegen. Alle I²C-Pegel inklusive Modul-Pullups müssen für ESP32 **3,3 V** betragen. Zwei BME280 benötigen `0x76` und `0x77`, sonst anderer Bus/MUX.
4. **Heizung ohne unabhängigen Schutz:** Ein MOSFET-Kurzschluss oder dauerhaftes HIGH kann am 4-Ω-Heizdraht bei 24 V **144 W Dauerleistung** verursachen. Software und Temperatursensoren verhindern dies nicht. **Eine unabhängige Thermosicherung/Temperaturbegrenzung wird vor unbeaufsichtigtem Betrieb dringend empfohlen.** Das ist derzeit **nicht erfüllt**.
5. **24-V-Zuleitung/Verbraucher:** Eingangssicherung (vorzugsweise auch an der Einspeiseseite im Haus), Kurzschluss-/Transientenschutz, Strombelastbarkeit der Kabel, Steckverbinder, Leiterbahnen und 12-V-Wandler müssen nach konkreter Hardware ausgelegt werden.
6. **Dioden D1–D3:** Es sind nur Anschlusspunkte. Schutzprinzip und Diode hängen von der jeweiligen Last ab; insbesondere bei BLDC-Pumpen mit Elektronik nicht pauschal eine Freilaufdiode als universelle Lösung annehmen.
7. **Bauteile/PCB:** Realer TC4427CPA benötigt **DIP-8**, die Q1–Q3-IRLZ44N **TO-220AB**. Erst mit definitiven Footprints/Pin-1-Markierungen, ERC/Netzliste und Layoutkontrolle lässt sich die PCB-Verschaltung abschließen.

## 3. Versorgungsschienen und Netznamen

| Netz           | Beschreibung                                                                      |
| -------------- | --------------------------------------------------------------------------------- |
| `VIN`          | Versorgungseingang am PCB, vorgesehen 24 V DC; bei alternativer Versorgung 12 V   |
| `12V`          | Ausgang des U5-Buck oder J7-Bypass aus 12-V-VIN bei entferntem U5                 |
| `5V`           | Ausgang U4: ESP32-DevKitC, **geeignete** Sensormodule, DS18B20-VDD                |
| `3V3`          | 3,3 V **vom** ESP32-DevKitC-Regler; Logikpullups und ggf. kompatible Sensormodule |
| `GND`          | Gemeinsamer Bezug von Lasten/Controllern; Leistungsmassen niederimpedant führen   |
| `Q1_TRIG`      | ESP GPIO18 → U6 IN A / R6                                                         |
| `Q2_TRIG`      | ESP GPIO23 → R8 → Q2 Gate                                                         |
| `Q3_TRIG`      | ESP GPIO19 → U6 IN B / R7                                                         |
| `HEATER_SW`    | Q1 Drain / Heizdraht Minus J1-2 / D1-1                                            |
| `COOL_SW`      | Q2 Drain / Pumpe Minus J4-2 / D2-1                                                |
| `RESERVE_SW`   | Q3 Drain / Reserve Minus J5-2 / D3-1                                              |
| `RESERVE_PLUS` | J5-1 / D3-2 / gewählte Versorgung über J2 **oder** J3                             |
| `DATA_Plate`   | Separater 1-Wire-Bus U8, R11 → ESP GPIO13                                         |
| `DATA_Tank`    | Separater 1-Wire-Bus U9, R12 → ESP GPIO14                                         |
| `SDA`/`SCL`    | Gemeinsamer I²C-Bus U2/U3/U7 → ESP GPIO21/22, **3,3-V-Logik**                     |
| `VIN_DIV`      | R10/R9-Teilermitte und Eingang R13 (**SOLL**, derzeit mit D4/C10 verbunden)       |
| `VIN_ADC`      | Ausgang R13, D4/C10 und ESP GPIO34 (**SOLL**, ersetzt bisheriges `VIN/16`)        |

## 4. Bauteile: jedes Referenzelement einzeln

### 4.1 ICs, Transistoren und externe Sensoranschlüsse

| Ref.   | Typ / Spezifikation                                                    | Anwendung                                                                      |
| ------ | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| **U1** | ESP32-DevKitC, klassisches ESP32-WROOM, genaue Platinenrevision prüfen | 5-V-Versorgung, eigene 3,3-V-Logik, Steuerung und Netzwerk                     |
| **U2** | 4-poliger SHT45-**Modul**-Stecker, Modultyp offen                      | Raumlufttemperatur und Luftfeuchte (Klimabodenraum); I²C `0x44`                |
| **U3** | 4-poliger BME280-**Modul**-Stecker                                     | Außenklima; I²C `0x76` **vorläufig**                                           |
| **U4** | Buck `VIN → 5 V`, ausreichend dimensioniert für WLAN-Stromspitzen      | ESP32 und Sensoren; 4-poliger Modulsockel inkl. EN/NC                          |
| **U5** | Buck `VIN → 12 V`, 3 A max. Ausgang                                    | Pumpe, TC4427 und Reserve bei 12 V; alternativ J7-Bypass                       |
| **U6** | **TC4427CPA** dual, nichtinvertierend, VDD 4,5–18 V, **PDIP-8**        | A: Heiz-PWM; B: Reserve-PWM/EIN-AUS; VDD = 12 V; CPA 0 bis +70 °C spezifiziert |
| **U7** | Zweiter 4-poliger BME280-Modulstecker                                  | Stallklima; I²C `0x77` **vorläufig**                                           |
| **U8** | 3-poliger Verteileranschluss für **9 × DS18B20** (3×3 Platte), 5-V-VDD | Bus `DATA_Plate`, externe DS18B20-Versorgung                                   |
| **U9** | 3-poliger Anschluss für **1 × DS18B20** (Tank), 5-V-VDD                | Bus `DATA_Tank`, unabhängig von Platte                                         |
| **Q1** | **IRLZ44NPBF**, N-MOSFET, 55 V, TO-220AB                               | 4-Ω-Heizdraht, Low-Side-PWM bis 20 kHz                                         |
| **Q2** | **IRLZ44NPBF**, N-MOSFET, 55 V, TO-220AB                               | Kühlkreispumpe, Low-Side EIN/AUS; 3,3-V-Gateansteuerung unter Last prüfen      |
| **Q3** | **IRLZ44NPBF**, N-MOSFET, 55 V, TO-220AB                               | Reserve 12 V oder VIN; über TC4427-Kanal B                                     |

### 4.2 Widerstände – Werte, Pins und Funktion

Alle Widerstände zunächst **0,25 W**, sofern nicht anders dimensioniert. Bei R9/R10 **1-%-Metallfilm** für den Spannungsteiler.

| Ref.    | Wert             | Pin 1            | Pin 2               | Funktion                                                                                                |
| ------- | ---------------- | ---------------- | ------------------- | ------------------------------------------------------------------------------------------------------- |
| **R1**  | 10 kΩ            | GND              | Q1 Gate             | Gate-Source-Pulldown Heizdraht                                                                          |
| **R2**  | 10 kΩ            | GND              | Q2 Gate             | Gate-Source-Pulldown Kühlpumpe                                                                          |
| **R3**  | 10 kΩ            | GND              | Q3 Gate             | Gate-Source-Pulldown Reserve                                                                            |
| **R4**  | **22 Ω (`22R`)** | U6 OUT A / Pin 7 | Q1 Gate             | Serieller Gatewiderstand 20-kHz-Heiz-PWM                                                                |
| **R5**  | **22 Ω (`22R`)** | U6 OUT B / Pin 5 | Q3 Gate             | Serieller Gatewiderstand Reserve                                                                        |
| **R6**  | 10 kΩ            | GND              | `Q1_TRIG` / U6 IN A | TC4427-Eingang A LOW beim Start                                                                         |
| **R7**  | 10 kΩ            | GND              | `Q3_TRIG` / U6 IN B | TC4427-Eingang B LOW beim Start                                                                         |
| **R8**  | 220 Ω            | `Q2_TRIG`        | Q2 Gate             | GPIO-Gatewiderstand Kühlpumpe                                                                           |
| **R9**  | 10 kΩ, 1 %       | GND              | `VIN_DIV`           | Unterer Widerstand des 1:16-Teilers                                                                     |
| **R10** | 150 kΩ, 1 %      | `VIN_DIV`        | VIN                 | Oberer Widerstand des 1:16-Teilers                                                                      |
| **R11** | **4,7 kΩ**       | 3V3              | `DATA_Plate`        | Eigener 1-Wire-Pullup **Platte**; bestücken                                                             |
| **R12** | **4,7 kΩ**       | 3V3              | `DATA_Tank`         | Eigener 1-Wire-Pullup **Tank**; bestücken                                                               |
| **R13** | **3,3 kΩ**       | `VIN_DIV`        | `VIN_ADC`           | ADC-Serienwiderstand; Schutzbauteile **auf ADC-Seite** positionieren                                    |
| **R14** | 4,7 kΩ           | 3V3              | SDA                 | **SOLL/OPTIONAL:** I²C-Pullup (nur falls keine passende Modulbestückung); im Screenshot nicht vorhanden |
| **R15** | 4,7 kΩ           | 3V3              | SCL                 | **SOLL/OPTIONAL:** I²C-Pullup; im Screenshot nicht vorhanden                                            |

**Hinweis:** Die generischen zweipoligen Symbole haben momentan kein belastbares elektrisches „Widerstand“-Modell; die Werte müssen beim Umstieg auf echte R-Symbole übernommen werden.

### 4.3 Kondensatoren – Einzelbelegung

| Ref.    | Typ / Wert                                     | Plus / Pin 1       | Minus / Pin 2 | Verwendung                                                          |
| ------- | ---------------------------------------------- | ------------------ | ------------- | ------------------------------------------------------------------- |
| **C1**  | 470 µF / 50 V Aluminium-Elko                   | 12V                | GND           | 12-V-Ausgangspuffer                                                 |
| **C2**  | 100 nF / ≥25 V X7R Keramik                     | 12V                | GND           | HF-Entkopplung der 12-V-Schiene                                     |
| **C3**  | 220 µF / 16 V Aluminium-Elko                   | 5V                 | GND           | 5-V-Puffer, WLAN-/Logiklasten                                       |
| **C4**  | 33 nF / ≥16 V Keramik                          | 5V                 | GND           | Ergänzende 5-V-HF-Entkopplung (100 nF zusätzlich optional sinnvoll) |
| **C5**  | **2200 µF / 50 V** Aluminium-Elko, ChongX VEHT | VIN                | GND           | Eingangspuffer 24 V; Rippelstromwert unbekannt                      |
| **C6**  | **2200 µF / 50 V** Aluminium-Elko, ChongX VEHT | VIN                | GND           | Parallel zu C5; Rippelstrom und Temperatur prüfen                   |
| **C7**  | 100 nF / ≥50 V X7R Keramik                     | VIN                | GND           | Lokale schnelle VIN-Entkopplung                                     |
| **C8**  | **100 nF / ≥25 V** X7R Keramik                 | 12V / U6 VDD       | GND / U6 GND  | Lokale Gate-Treiber-Entkopplung, direkt an U6                       |
| **C9**  | **1 µF / ≥25 V** Keramik                       | 12V / U6 VDD       | GND / U6 GND  | Zweite Gate-Treiber-Entkopplung, nahe U6                            |
| **C10** | **100 nF / ≥16 V** Keramik                     | **`VIN_ADC` SOLL** | GND           | ADC-Filter, im Bild noch auf `VIN_DIV` vor R13                      |

**VIN-Kondensatorbank:** 2 × 2200 µF = 4400 µF. Bei 24 V, 4 Ω, 60 W, 20 kHz entstehen idealisiert etwa 6 A Pulsstrom am Heizdraht, 3,87 A RMS im Lastpfad und bis ungefähr 3 A RMS Kondensator-Rippelstrom. Die vorhandenen preiswerten Elkos sind **nicht durch ihre Kapazitäts-/Temperaturaufschrift für diesen Rippelstrom freigegeben**; zur dauerhaften Ausführung spezifizierte Kondensatoren bzw. belastbare Mess-/Qualifikationsdaten vorsehen.

### 4.4 Dioden, Sicherung und Schutz

| Ref.     | Typ / Dimensionierung                                                                                         | Pinanschluss und Einbau                                                                                                                   | Status                                     |
| -------- | ------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------ |
| **D1**   | Heizdraht: **DNP** (bei rein ohmscher Last keine Freilaufdiode)                                               | Gehäuseplatz Pin 1 = Q1 Drain (`HEATER_SW`), Pin 2 = VIN; falls später bestückt: Anode an Pin 1, Kathode an Pin 2                         | Anschluss vorhanden, nicht bestücken       |
| **D2**   | Freilauf-/Schutzdiode für Kühlpumpe, je nach Pumpentyp z. B. Schottky **SB560 (5 A / 60 V)** als Prüfkandidat | **Anode** D2-1 = Q2 Drain (`COOL_SW`), **Kathode** D2-2 = 12V                                                                             | Platz vorhanden; Pumpendaten prüfen        |
| **D3**   | Reserve-Schutzdiode **nur** bei passender induktiver Last, Auswahl nach 12-/24-V-Anwendung                    | **Anode** D3-1 = Q3 Drain (`RESERVE_SW`), **Kathode** D3-2 = `RESERVE_PLUS`                                                               | Platz vorhanden; lastabhängig, ggf. DNP    |
| **D4**   | **2,5-V-Präzisions-Shuntreferenz**, vorgeschlagen **TI LM4040D25IDBZR**, SOT-23                               | **SOLL:** Kathode `VIN_ADC`, Anode GND, Shield-Pin nach Datenblatt. Das derzeitige 2-polige D4-Symbol/Footprint muss ausgetauscht werden. | **Nicht fertig umgesetzt**                 |
| **F1**   | Eingangssicherung; Nennstrom nach Kabelquerschnitt und realen Last-/Einschaltströmen bestimmen                | **SOLL:** Plusleitung nahe Einspeisepunkt (Haus), ergänzend ggf. lokal an J6                                                              | **Nicht im Screenshot**                    |
| **TVS1** | Eingangsschutz gegen geeignete transiente Überspannungen; erst nach maximaler VIN-Auslegung auswählen         | **OPTIONAL / PRÜFEN:** zwischen VIN und GND nahe J6                                                                                       | **Nicht im Screenshot**                    |
| **TH1**  | Unabhängiger Temperaturbegrenzer/-sicherung für Heizdraht, Lage/Temperatur nach thermischem Test              | **DRINGEND EMPFOHLEN**, unabhängig von ESP und MOSFET in Last-/Versorgungspfad                                                            | **Vom aktuellen Entwurf nicht vorgesehen** |

**D4 – Pinout-Fallstrick:** Der LM4040D25 im **DBZ/SOT-23** besitzt **3 Pins** (TI: 1 = Kathode/`+`, 2 = Anode/`−`, 3 = Shield/`*`). Den Shield-Pin gemäß **konkretem** Herstellerdatenblatt behandeln; **kein** zweipoliges Dioden-Footprint verwenden. Wenn ausdrücklich ein zweipoliger Zener verwendet werden soll, sind Typ, Leckstrom und Klemmbereich separat für alle Fehlerfälle nachzuweisen.

## 5. ESP32-DevKitC – konkretes Pinout und Firmwareparameter

**Wichtig:** Die **GPIO-Nummer** ist nicht die **physische KiCad-Symbolpin-Nummer**. Die folgende Belegung passt zum gezeigten ESP32-DevKitC-Symbol (klassischer ESP32). **Im Screenshot sind alle GPIO-Verbindungen noch SOLL.**

| Funktion                 | GPIO   | KiCad-Symbolpin U1                | ESP-Pinmodus / Schnittstelle   | Ziel / Detail                                                           |
| ------------------------ | ------:| ---------------------------------:| ------------------------------ | ----------------------------------------------------------------------- |
| 5-V-Versorgung           | `5V`   | 19                                | Versorgung, kein GPIO          | U4 Ausgang `5V` (**IST**)                                               |
| 3,3-V-Ausgang            | `3V3`  | 1                                 | Versorgung, kein GPIO          | `3V3` für R11/R12 und ggf. I²C-Pullups (**IST**)                        |
| Masse                    | `GND`  | 14                                | Versorgung, kein GPIO          | Gemeinsame Masse (**IST**)                                              |
| Heizdraht-PWM            | **18** | **30**                            | `OUTPUT`, LEDC, HIGH = Ein     | `Q1_TRIG` → U6 IN A Pin 2, 20 kHz, 10 Bit; Start 0                      |
| Reserve EIN/AUS oder PWM | **19** | **31**                            | `OUTPUT`, HIGH = Ein           | `Q3_TRIG` → U6 IN B Pin 4; bei PWM separater LEDC-Kanal                 |
| Kühlkreispumpe           | **23** | **37**                            | `OUTPUT`, HIGH = Ein           | `Q2_TRIG` → R8 → Q2 Gate; Start LOW                                     |
| I²C SDA                  | **21** | **33**                            | I²C Open-Drain                 | `SDA` → U2/U3/U7, 100 kHz als Startwert                                 |
| I²C SCL                  | **22** | **36**                            | I²C                            | `SCL` → U2/U3/U7, 100 kHz                                               |
| VIN-Messung              | **34** | **5**                             | `INPUT` / **ADC1_CH6**, 12 Bit | `VIN_ADC` **nach** R13, D4 und C10; geeignete Dämpfung und Kalibrierung |
| 1-Wire: Platte           | **13** | **15**                            | Bidirektionales 1-Wire-GPIO    | `DATA_Plate`, R11 4,7 kΩ an 3V3, **9 Sensoren**                         |
| 1-Wire: Tank             | **14** | **12**                            | Bidirektionales 1-Wire-GPIO    | `DATA_Tank`, R12 4,7 kΩ an 3V3, **1 Sensor**                            |
| Gehäusesensor optional   | **32** | **7**                             | frei / optionales GPIO         | Falls später zusätzlicher Temperatursensor vorgesehen                   |
| Reserve                  | **33** | **8** | frei                           | Zukünftige Erweiterung                                                  |

**Pinwahl:** GPIO34 ist input-only und Teil von **ADC1** (während WLAN nutzbar). GPIO13/14 sind reguläre GPIOs, können aber mit JTAG-Nutzung kollidieren. GPIO6–11 sind dem Flash vorbehalten; GPIO0, 2, 4, 5, 12, 15 sind Boot-Strapping-Pins und werden für die vorgesehenen Aktorausgänge vermieden. UART0-GPIO1/GPIO3 bleiben für die serielle Konsole frei.

### 5.1 Firmware-Konfiguration – Arduino-ESP32, Beispiel

```cpp
// ESP32-DevKitC / klassischer ESP32
constexpr int PIN_HEATER_PWM    = 18;
constexpr int PIN_RESERVE       = 19;
constexpr int PIN_COOL_PUMP     = 23;
constexpr int PIN_I2C_SDA       = 21;
constexpr int PIN_I2C_SCL       = 22;
constexpr int PIN_VIN_ADC       = 34;  // ADC1_CH6; INPUT ONLY
constexpr int PIN_ONEWIRE_PLATE = 13;  // 9x DS18B20
constexpr int PIN_ONEWIRE_TANK  = 14;  // 1x DS18B20
constexpr int PIN_CASE_TEMP     = 32;  // optional; im aktuellen Schaltplan NICHT vorhanden

constexpr uint32_t HEATER_PWM_HZ   = 20000;
constexpr uint8_t  HEATER_PWM_BITS = 10;
constexpr uint16_t HEATER_PWM_TOP  = (1u << HEATER_PWM_BITS) - 1u;
constexpr float    HEATER_R_OHM    = 4.0f;    // tatsächlichen Widerstand messen
constexpr float    HEATER_P_DEFAULT_W  = 36.0f;
constexpr float    HEATER_P_LIMIT_W    = 60.0f; // nur nach thermischer Freigabe

constexpr float    VIN_DIVIDER_GAIN = 16.0f;  // 150k/10k
constexpr float    VIN_VALID_MIN_V = 10.0f;  // vorläufig, an Netzteil anpassen
constexpr float    VIN_VALID_MAX_V = 30.0f;  // vorläufig, an Netzteil anpassen
constexpr uint32_t ADC_SAMPLE_INTERVAL_MS = 100;
constexpr int      ADC_SAMPLES_PER_READING = 64;

constexpr uint8_t SHT45_ADDR = 0x44;
constexpr uint8_t BME280_OUTSIDE_ADDR = 0x76; // NACH MODULTEST
constexpr uint8_t BME280_STALL_ADDR   = 0x77; // NACH MODULTEST
constexpr uint8_t DS18B20_PLATE_COUNT = 9;
constexpr uint8_t DS18B20_TANK_COUNT  = 1;
constexpr uint8_t DS18B20_RESOLUTION  = 12;
constexpr uint32_t I2C_CLOCK_HZ       = 100000;

// Initialisierung (Beispiel; realen Board-Core verifizieren):
// pinMode(PIN_HEATER_PWM, OUTPUT); digitalWrite(PIN_HEATER_PWM, LOW);
// pinMode(PIN_RESERVE, OUTPUT);    digitalWrite(PIN_RESERVE, LOW);
// pinMode(PIN_COOL_PUMP, OUTPUT);  digitalWrite(PIN_COOL_PUMP, LOW);
// Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
// OneWire busPlate(PIN_ONEWIRE_PLATE), busTank(PIN_ONEWIRE_TANK);
// analogSetPinAttenuation(PIN_VIN_ADC, ADC_11db); // klassischer ESP32
// ledcAttach(PIN_HEATER_PWM, HEATER_PWM_HZ, HEATER_PWM_BITS); // Arduino-Core 3.x
// ledcWrite(PIN_HEATER_PWM, 0);
```

**ADC-Auswertung:** Mit der zur installierten Core-Version passenden kalibrierten ADC-Funktion (z. B. `analogReadMilliVolts`) `U_ADC` ermitteln und `VIN ≈ 16 × U_ADC` berechnen. Den Widerstandsteiler und ADC gegen ein Multimeter kalibrieren. Bei ungültigen Messwerten **Heiz-PWM sofort 0**. Hardwarefehler können durch diese Software nicht sicher ausgeschlossen werden.

**PWM-Leistungsbegrenzung (ohmscher Draht):**

```text
D_max = min(1, P_max * R_heater / (VIN_measured * VIN_measured))
PWM_max = round(D_max * 1023)
```

Bei 24 V und 4 Ω gilt: 36 W → 25 % PWM, 60 W → ca. 41,7 %. Beim Einschalten liegen **6 A** am Heizdraht, auch wenn der gemittelte Strom bei 60 W nur 2,5 A beträgt. Der letzte sichere Zustand nach Reset/Watchdog ist **AUS**; MQTT/WLAN-Verlust ändert keine lokalen Sicherheitsentscheidungen. Heizdraht und Pumpen sind softwareseitig gegenseitig verriegelt.

## 6. Vollständiges PCB-Anschluss-Pinout

### 6.1 Steckverbinder, Jumper und Diodenplätze

| Ref.                 | Pin 1                              | Pin 2                    | Bemerkung                                         |
| -------------------- | ---------------------------------- | ------------------------ | ------------------------------------------------- |
| **J1** Heizdraht     | VIN                                | `HEATER_SW`              | 4 Ω, 6-A-Pulsstrom bei 24 V; Q1 schaltet Minus    |
| **J2** Reserve 12 V  | `RESERVE_PLUS`                     | 12V                      | Stecken **nur** wenn J3 offen                     |
| **J3** Reserve VIN   | VIN                                | `RESERVE_PLUS`           | Stecken **nur** wenn J2 offen                     |
| **J4** Kühlpumpe     | 12V                                | `COOL_SW`                | Q2 schaltet Minus                                 |
| **J5** Reserve       | `RESERVE_PLUS`                     | `RESERVE_SW`             | Q3 schaltet Minus                                 |
| **J6** Eingang       | VIN                                | GND                      | 12-/24-V-Versorgung, Sicherung separat vorsehen   |
| **J7** 12-V-Bypass   | VIN                                | 12V                      | **Nur bei VIN=12 V und elektrisch getrenntem U5** |
| **D1** Freilaufplatz | `HEATER_SW` (Anode falls bestückt) | VIN (Kathode)            | Reiner Heizdraht: **DNP**                         |
| **D2** Freilaufplatz | `COOL_SW` (Anode)                  | 12V (Kathode)            | Motor-/Pumpentyp prüfen                           |
| **D3** Freilaufplatz | `RESERVE_SW` (Anode)               | `RESERVE_PLUS` (Kathode) | Bei passender induktiver Last                     |

### 6.2 Sensorstecker

| Ref.                  | Pin 1 | Pin 2        | Pin 3 | Pin 4 | Beschreibung                                                  |
| --------------------- | ----- | ------------ | ----- | ----- | ------------------------------------------------------------- |
| **U2 SHT45**          | 5V*   | GND          | SCL   | SDA   | Innenraum / Klimabodenraum, `0x44`                            |
| **U3 BME280**         | 5V*   | GND          | SCL   | SDA   | Außenluft, Adresse zu testen                                  |
| **U7 BME280**         | 5V*   | GND          | SCL   | SDA   | Stallluft, andere Adresse zu testen                           |
| **U8 DS18B20 Platte** | 5V    | `DATA_Plate` | GND   | –     | Neun parallel angeschlossene Sensoren, **eigener Bus GPIO13** |
| **U9 DS18B20 Tank**   | 5V    | `DATA_Tank`  | GND   | –     | Ein Sensor, **eigener Bus GPIO14**                            |

\* **Nur bei dafür geeignetem 5-V-VIN-Breakout.** Falls nackte Sensorschaltung oder Modul ohne Logikpegelwandlung vorhanden, auf 3V3 umstellen und I²C-Pegel prüfen. Die endgültigen SHT45/BME280-**Modul**-Pinfolgen vor Anschluss anhand der realen Breakouts abgleichen.

### 6.3 Buck-Converter und Gate-Treiber

| Ref.            | Pin 1     | Pin 2 | Pin 3     | Pin 4                       | Weitere Pins |
| --------------- | --------- | ----- | --------- | --------------------------- | ------------ |
| **U4 5V-Buck**  | VO+ = 5V  | GND   | IN+ = VIN | EN = NC (nach Modulprüfung) | —            |
| **U5 12V-Buck** | VO+ = 12V | GND   | IN+ = VIN | EN = NC (nach Modulprüfung) | —            |

**U6 TC4427CPA (DIP-8):**

| Pin | Signal | Anschluss                                     |
| ---:| ------ | --------------------------------------------- |
| 1   | NC     | offen                                         |
| 2   | IN A   | `Q1_TRIG` → ESP GPIO18, R6 10 kΩ nach GND     |
| 3   | GND    | GND                                           |
| 4   | IN B   | `Q3_TRIG` → ESP GPIO19, R7 10 kΩ nach GND     |
| 5   | OUT B  | R5 22 Ω → Q3 Gate                             |
| 6   | VDD    | 12V, C8 100 nF + C9 1 µF unmittelbar nach GND |
| 7   | OUT A  | R4 22 Ω → Q1 Gate                             |
| 8   | NC     | offen                                         |

### 6.4 Q1–Q3 – IRLZ44NPBF, TO-220AB

| Anschluss          | Q1 Heizdraht                        | Q2 Kühlpumpe                                | Q3 Reserve                          |
| ------------------ | ----------------------------------- | ------------------------------------------- | ----------------------------------- |
| **Gate (Pin 1)**   | R4 nach U6 OUT A, R1 10 kΩ nach GND | R8 220 Ω nach ESP GPIO23, R2 10 kΩ nach GND | R5 nach U6 OUT B, R3 10 kΩ nach GND |
| **Drain (Pin 2)**  | J1-2, D1-1                          | J4-2, D2-1                                  | J5-2, D3-1                          |
| **Source (Pin 3)** | GND                                 | GND                                         | GND                                 |
| **Metalllasche**   | Drain-Potenzial!                    | Drain-Potenzial!                            | Drain-Potenzial!                    |

**Hinweis Q2:** Der IRLZ44N besitzt keine garantierte niedrige `R_DS(on)`-Spezifikation bei 3,3 V. Die Kühlpumpenbelastung und MOSFET-Temperatur real prüfen; ggf. Q2 über geeignetes Gate-Treiberkonzept oder einen bei 2,5–3,3 V spezifizierten MOSFET ausführen.

## 7. ADC-Verschaltung: genaue zu realisierende Topologie

**Aktueller Screenshot (noch falsch positionierter Schutz):**

```text
VIN ── R10(150k) ──┬── R13(3k3) ─── VIN/16 [Richtung ESP]
                   │
                   ├── R9(10k) ── GND
                   ├── C10(100n) ─ GND  <-- derzeit VOR R13
                   └── D4 ───────── GND  <-- derzeit VOR R13
```

**SOLL, zum Nachzeichnen in KiCad:**

```text
VIN ─── R10 150k ──┬── R13 3k3 ──┬──── VIN_ADC → GPIO34
                    │             │
                   R9 10k        C10 100n
                    │             │
                   GND           GND
                                  │
                                  └── D4 (2,5-V-Shunt nach GND)
                                      K → VIN_ADC, A → GND
```

Eigentlich ist D4 ein zweiter **paralleler** Zweig vom `VIN_ADC`-Knoten nach GND, nicht mit C10 in Serie. Der ASCII-Plan ist als Netzliste zu verstehen.

| VIN  | Nominaler ADC-Pegel (1:16)           |
| ----:| ------------------------------------:|
| 12 V | 0,750 V                              |
| 24 V | 1,500 V                              |
| 30 V | 1,875 V                              |
| 40 V | 2,500 V (Übergang zum Schutzbereich) |

D4 ist keine pauschale Garantie gegen unbegrenzt energiereiche VIN-Fehler; Referenztyp, maximaler Shunt-Strom, Kaltstart und Kurzschlussfall müssen zum gewählten realen Bauteil passen. Der hohe R10 begrenzt bei intakter Schaltung den normalen Strom, R13 schützt zusätzlich bei einzelnen Fehlern. Ohne messgültigen ADC-Wert PWM sperren.

## 8. Offene Prüfungen / Abnahmeliste

| Priorität | Prüfung / Änderung                                                                                       | Freigabekriterium                                                                                              |
| --------- | -------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| **P0**    | ESP32-GPIOs (Kap. 5), I²C und beide getrennten 1-Wire-Busse **real** verbinden                           | KiCad-ERC und Netzliste bestätigen die beiden getrennten Busse                                                 |
| **P0**    | D4/C10 hinter R13, D4 mit realem 3-poligem LM4040-Symbol/Footprint oder qualifiziertem Ersatz            | Sicherer ADC-Pegel im spezifizierten VIN-/Fehlerbereich nachgewiesen                                           |
| **P0**    | Reale SHT45/BME280-Breakouts prüfen: 5-V-Tauglichkeit, SDA/SCL-Pullup-Spannung, Adressen 0x76/0x77       | Keine 5 V auf ESP-Pins; alle drei I²C-Geräte erreichbar                                                        |
| **P0**    | Sicherheitskonzept Heizung: unabhängig von MCU durchschaltenden MOSFET/Treiber abfangen                  | Für unbeaufsichtigten Betrieb unabhängige thermische Abschaltung dringend empfohlen; **derzeit nicht erfüllt** |
| **P0**    | J2/J3 gegenseitig ausschließen, J7 nur bei VIN=12 V und U5 komplett getrennt                             | Keine fehlerhafte Verbindung VIN ↔ 12V bei 24 V                                                                |
| **P0**    | Sicherung für langes Erdkabel/Leistungsstrompfad und Verpolschutz festlegen                              | Kabel und PCB auch bei Kurzschluss ausreichend geschützt                                                       |
| **P1**    | U4/U5 genaue Module, EN-Funktion, Ausgangslast, Gegen-/Rückspeisefestigkeit prüfen                       | Herstellerangaben und Lasttest bestätigt                                                                       |
| **P1**    | C5/C6 Rippelstrom (ChongX unbekannt), 20-kHz-PWM-Eingangsspannung, Einschaltstrom messen                 | Kapazität, Lebensdauer und Temperatur geeignet                                                                 |
| **P1**    | 24-V-VIN-Transiente, 15-m-Kabel, MOSFET-Überschwinger, Leiterbahn-RMS dimensionieren                     | Oszilloskop/PCB-Berechnung und Schutzbeschaltung geprüft                                                       |
| **P1**    | Pumpentypen und D2/D3-Dioden, Anlaufstrom, Reservelast/PWM-Betrieb prüfen                                | Geeignete Freilauf-/TVS-Auslegung                                                                              |
| **P1**    | TC4427CPA bei Frost: 0…+70 °C Spezifikation; ggf. TC4427EPA oder anderer temperaturfreigegebener Treiber | Betriebstemperaturbereich des realen Produkts passt                                                            |
| **P1**    | Q2 bei 3,3 V Gate unter Volllast prüfen                                                                  | VDS(on), P-Verlust, Temperatur ausreichend                                                                     |
| **P1**    | Reale R/C/D/Q-Bauteilsymbole und Footprints + Pin-1-Markierung zuordnen; ERC/DRC                         | Ohne Fehler und mechanische/elektrische Verwechslungen                                                         |
| **P2**    | Getrennte 1-Wire-Busse mit allen neun verlegten Plattensensoren testen                                   | Sensoren mit stabilen ROM-IDs ohne Kommunikationsausfälle                                                      |
| **P2**    | ADC kalibrieren, Webserver/MQTT-Verläufe, Software-Fallback und Sensorfehler testen                      | Mess-, Steuer- und Sicherheitsfunktionen wie im Pflichtenheft                                                  |

## 9. Herstellerquellen

- [Espressif: ESP32-DevKitC V4 Pinout und Versorgung](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html)
- [Microchip: TC4426/TC4427/TC4428 Datenblatt](https://ww1.microchip.com/downloads/en/DeviceDoc/20001422G.pdf)
- [Infineon: IRLZ44N Datenblatt](https://www.infineon.com/assets/row/public/documents/24/49/infineon-irlz44n-datasheet-en.pdf)
- [Analog Devices: DS18B20 Datenblatt](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)
- [Texas Instruments: LM4040 Datenblatt](https://www.ti.com/lit/ds/symlink/lm4040.pdf)
- [Sensirion: SHT45 Spezifikationen](https://sensirion.com/products/catalog/SHT45)
- [Bosch Sensortec: BME280 Datenblatt](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)

---

**Ergebnis:** Die getrennten DS18B20-Leitungen sind richtig geplant. **„Finale Pinout-Dokumentation“ ist möglich, aber eine Freigabe des abgebildeten Schaltplans oder eines darauf beruhenden PCBs ist wegen der konkret genannten P0-Punkte noch nicht gerechtfertigt.**
