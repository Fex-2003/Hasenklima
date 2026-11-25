# Pflichtenheft

## Klimaregelung für einen temperierten Rückzugsraum in einem Kaninchenstall

| Dokumenteigenschaft    | Wert                                                  |
| ---------------------- | ----------------------------------------------------- |
| **Projektbezeichnung** | Klimaregelung Kaninchenstall                          |
| **Systemkomponente**   | ESP32-basierte Heiz-, Kühl- und Überwachungssteuerung |
| **Dokumenttyp**        | Softwareorientiertes Pflichtenheft                    |
| **Dokumentstatus**     | Entwurf zur Softwareerstellung                        |
| **Version**            | 1.3                                                   |
| **Stand**              | 20. November 2026                                     |

---

## 1. Zielsetzung

Ziel ist eine autonome, ESP32-basierte Klimaregelung eines jederzeit frei zugänglichen Rückzugsraums im Kaninchenstall. Die Regelung soll Frostschutz, bedarfsgerechte Kühlung und eine möglichst gleichmäßige Bodenplattentemperatur bei geringem Wartungsaufwand ermöglichen.

Das System muss:

1. die elektrische Heizung mit **20-kHz-PWM** und einer **PI-Temperaturregelung** betreiben; normale Stellgröße ist der **PWM-Tastgrad**, nicht eine vermeintlich exakt gemessene Heizleistung;
2. die Plattenkühlung und Verdunstungskühlung mit separaten 12-V-Pumpen durchführen und Kondensat vermeiden;
3. Einzelwerte, abgeleitete Kennwerte, Zustände, Alarme und Verläufe lokal bereitstellen;
4. Konfiguration und Diagnose per Webserver sowie MQTT bereitstellen, ohne von Netzwerkdiensten für die lokale Regelung abhängig zu sein;
5. einen beaufsichtigten **Systemidentifikationsmodus (PWM-Sprungantwort)** mit Datenaufzeichnung sowie einen **manuellen Testmodus** bereitstellen;
6. kritische Temperatur-, Leistungs- und Ausgangsgrenzen in allen Betriebsarten durchsetzen;
7. bei Neustart, Sensor-, Versorgungs- und Netzwerkausfällen definierte, möglichst sichere Zustände einnehmen.

Die Steuerung ist als nicht blockierender Zustandsautomat auszuführen. Der Heizdraht darf **niemals gleichzeitig** mit einer der beiden Pumpen eingeschaltet werden. Die niedrigleistungsfähige Zusatzheizung für den MOSFET-Gate-Treiber ist davon funktional getrennt und unterliegt eigenen Freigaben.

**Geltungsstand:** Dieses Dokument konkretisiert den **Entwurfsstand v1.3**. Eine dokumentierte Soll-Funktion ist keine automatische Aussage über fertig geprüfte Hardware. Die PCB-Pinout-Dokumentation v0.7 bleibt die Referenz für Anschlussnamen und enthält noch offene Prüfpunkte.

---

## 2. Systemübersicht

### 2.1 Komponenten und Energiefluss

- Externes **24-V-/3-A-Netzteil (72 W)** im Haus; Eignung für die tatsächlich auftretende PWM- und Einschaltbelastung ist **durch Messung zu bestätigen**.
- **15 m NYY-J 3 × 2,5 mm²** Kupfer-Erdkabel zwischen Haus und Stall. Für den 24-V-Stromkreis werden zwei dafür vorgesehene Betriebsadern genutzt; die grün-gelbe Ader darf nicht als beliebiger DC-Leiter zweckentfremdet werden.
- PCB-Eingang `VIN` (24 V nominal) mit zwei parallelen **2200-µF-/50-V-Kondensatoren** und weiterer HF-Entkopplung.
- Direkter VIN-Leistungszweig für den ca. **4-Ω-Stahl-Heizdraht**; Low-Side-MOSFET IRLZ44NPBF mit Gate-Treiber TC4427CPA.
- Zwei DC/DC-Abwärtswandler: **VIN → 12 V** für Pumpen, Gate-Treiber und dessen Zusatzheizung sowie **VIN → 5 V** für ESP32 und geeignete Sensor-Breakouts.
- ESP32-DevKitC, neun DS18B20 in der Platte, ein DS18B20 im Tank, ein SHT45-Breakout im Rückzugsraum, zwei BME280-Breakouts (Außenluft/Stallluft).
- Zwei 12-V-Pumpen: Kühlkreislauf und Wasserumlauf über die Moosfläche; elektrischer Ausgang der zweiten Pumpe muss über den bisher als **Reserve** dokumentierten Q3/J5-Pfad endgültig festgelegt werden.
- Q4 (2N2222) mit zwei oder vier 150-Ω-Widerständen als **offen temperaturgesteuerte Gate-Treiber-Zusatzheizung** ohne lokale Temperatur-Rückmeldung.
- WLAN, lokaler Webserver, MQTT, Fehlerprotokoll, Trends, Service- und Systemidentifikation.

### 2.2 Regelungs- und Bedienebenen

1. **Sicherheits-/Verriegelungsebene:** Vorrangige Prüfung von gültigen Messwerten, kritischen Temperaturgrenzen, Ausgangskombinationen, Leistungsfreigaben und Hardwarezustand.
2. **Automatik:** PI-Heizungsregelung bzw. getrennte Kühl- und Verdunstungslogik.
3. **Service:** Manuell vorgegebene PWM und Pumpenzustände oder zeitlich begrenzte Identifikationsversuche; die Sicherheits-/Verriegelungsebene bleibt wirksam.
4. **Kommunikation:** Webserver/MQTT zur Beobachtung und Parametrierung; Netzwerkverlust beeinflusst die lokale Automatik nicht.

---

## 3. Einsatzumgebung

### 3.1 Rückzugsraum, Dämmung und Platteneinbau

Der temperierte Rückzugsraum hat ungefähr **40 × 40 × 40 cm** (Breite × Tiefe × Höhe), ist nicht luftdicht und verfügt über einen dauerhaft offenen Zugang und eine Deckenlüftungsöffnung. Seine tragenden/abschließenden Wände bestehen aus Pressspanplatten.

**Neuer geplanter Dämmaufbau:**

- **40 mm expandiertes Polystyrol (EPS/Styropor)** an den Raumseiten einschließlich Decke sowie **unter und um die Beton-/Fliesenplatte**; bestehende Zugangs- und Lüftungsöffnungen bleiben funktionsfähig.
- Die Dämmplatten sind **rückseitig offen beziehungsweise ohne eigene rückseitige Abdeckung** ausgeführt und an die vorhandenen Pressspanwände angepasst; sie liegen dort an.
- Raumseitig/vorne schützt eine ungefähr **3–4 mm starke Faserplatte** die Dämmung vor direktem Kontakt und mechanischer Beanspruchung.
- Die Bodenplatte ist ungefähr **35 × 35 cm** groß; ihre genaue Dicke, das Unterbau-/Kontaktmaterial und die Positionen der Heizleiter bleiben zu vermessen.
- Die Tiere können den Raum und die Platte jederzeit betreten und verlassen. Teilweise oder vollständige Bedeckung durch Heu/Einstreu und ein Wassernapf sind zu berücksichtigen.

**Sicherheits- und Bauprüfung:** EPS, Faserplatte, Heu und Pressspan sind nicht als hitzebeständige Barrieren anzusehen. Die maximale Temperatur der Heizleiter und möglicher lokaler Hotspots ist auch bei hoher Dämmung, zusammengedrückter Einstreu, Sensorfehlern und Fehlern des Leistungs-MOSFETs zu bewerten. Offene Dämmkanten sind gegen Annagen, Feuchtigkeit und Verschmutzung zu schützen. Der Dämmaufbau ist vor Tierbetrieb auf zugängliche Kanten, Abdeckungsfestigkeit und Brandsicherheit zu prüfen.

Der neue Dämmaufbau verändert gegenüber früheren Wärmeverlustmodellen die effektive thermische Prozessverstärkung und Zeitkonstante. Die **PI-Reglerparameter müssen deshalb an der fertig eingebauten Platte identifiziert** werden; frühere Modellzahlen dienen nur als Startwerte.

---

### 3.2 Wassertank

Der Wassertank besitzt ein Volumen von etwa 120 l und ist ungefähr 1 m tief in das Erdreich eingelassen.

Die Tankoberkante befindet sich etwa 10 bis 20 cm unterhalb des Bodenniveaus.

Der Standort ist dauerhaft beschattet.

### 3.3 Verdunstungskühlfläche

Zur Rückkühlung des Tankwassers steht eine etwa 1,5 m² große, mit Moos bedeckte und ungefähr 45° geneigte Dachfläche zur Verfügung.

Das Wasser wird am oberen Rand verteilt und am unteren Rand in den Tank zurückgeführt.

---

## 4. Hardwareanforderungen

### 4.1 Steuerung

Ein **ESP32-DevKitC mit klassischem ESP32-WROOM** führt Sensorerfassung, Temperatur- und Taupunktberechnung, Zustandsautomat, PI-Regler, PWM-Ausgabe, Pumpensteuerung, Gate-Treiber-Zusatzheizung, ADC-Diagnose, Datenaufzeichnung, Webserver, MQTT und Fehlerverwaltung aus.

Alle zeitkritischen und sicherheitsrelevanten Funktionen müssen ohne WLAN/MQTT und ohne blockierende Warteschleifen funktionieren. Der externe Aufbau verwendet 3,3-V-Logikpegel.

### 4.2 Netzteil und Spannungsschienen

| Kenngröße | Aktueller Planungsstand / Verbindlichkeit |
|---|---|
| Netzteilstandort | Haus, wettergeschützt |
| Eingangsspannung | **24 V DC nominal** |
| Vorläufiges Netzteil | **24 V / 3 A / 72 W**; zu qualifizieren |
| Alternative | Leistungsstärkeres 24-V-Netzteil bei unzureichender Reserve, Einschalt- oder PWM-Festigkeit |
| Zuleitung | 15 m NYY-J 3 × 2,5 mm²; 30 m elektrische Stromschleife |
| Heizdraht | Direkt an gepufferter Eingangsschiene `VIN`, nicht am 12-V-Buck |
| Buck U5 | `VIN → 12V`, derzeit maximal ca. 3 A, realer Dauer-/Anlaufstrom zu verifizieren |
| Buck U4 | `VIN → 5V`, für ESP32-WLAN-Stromspitzen und Sensorik zu dimensionieren |
| 3,3 V | Über Regler des ESP32-DevKitC, nur innerhalb dessen zulässiger Belastbarkeit |
| Eingangspuffer | **C5 + C6 = 2 × 2200 µF / 50 V = 4400 µF** plus C7 100 nF |

**Leistungsreserve:** Bei einer mittleren Heizdrahtleistung von 60 W stehen bei 72 W Netzteilnennleistung nominell nur **12 W** für Leitungsverluste, Wirkungsgradverluste, ESP32, Sensorik, Gate-Treiber-Zusatzheizung und Reserve zur Verfügung. Diese Betrachtung gilt für den **stationären mittleren** Leistungsbedarf; die 6-A-Pulse des Heizdrahts und Netzteildynamik sind gesondert zu untersuchen. Vor Freigabe der 60-W-Betriebsart ist ein Belastungstest mit betriebsfertigem PCB, Kabel und allen vorgesehenen Eigenverbrauchern erforderlich.

**Einschalten:** Die Kondensatoren speichern bei 24 V idealisiert `E = 0,5 × 0,0044 F × (24 V)^2 ≈ 1,27 J` und benötigen `Q = 0,0044 F × 24 V ≈ 0,106 C` Ladung. Der tatsächliche Einschaltstrom wird nicht allein durch die Netzteil-Nennstromangabe bestimmt. Zu prüfen sind Strombegrenzung, Hiccup/Neustart, Einschwingdauer, Kabel- und Kontaktverluste und ggf. Einschaltstrombegrenzung. Schlägt der Test fehl, wird das Netzteil bzw. die Eingangsbeschaltung angepasst.

**Netzteilprüfung:** 0/36/60 W, PWM-Tastgradwechsel, Einschalten mit entladenen 4400 µF, Kaltstart, Abschalten, Wechsel der Pumpenlast und ungünstige Versorgungsspannungen. Es dürfen keine unzulässigen Spannungseinbrüche, Oszillationen, Übertemperaturen oder wiederholten Netzteil-Neustarts auftreten.

### 4.3 Erdleitung, Aderbelegung und Kabelschutz

- Verbindliche Bauabsicht: **15 m, 3-adriges NYY-J-Erdkabel, 2,5 mm² je Ader**. Das Kabel ist für die vorgesehene Verlegung und Umgebungsbedingungen fachgerecht einzubauen.
- Für den 24-V-Betriebsstrom sind **zwei geeignete Adern** zu verwenden. Bei NYY-J ist die **grün-gelbe Ader als Schutzleiter gekennzeichnet** und nicht als zusätzlicher beliebiger 24-V-Betriebsleiter zu benutzen. Die Verwendung/Behandlung dieses Leiters hängt vom Schutz- und Erdungskonzept ab und ist von einer fachkundigen Person festzulegen.
- Bei 30 m Stromschleife ergibt sich näherungsweise `R_Kabel ≈ 0,22 Ω` bei 20 °C für zwei 2,5-mm²-Kupferadern; bei 3 A folgen ungefähr **0,67 V Spannungsabfall** und **2 W Leitungsverlust**. Anschlüsse und höhere Leitertemperaturen erhöhen den Wert.
- Die Verlegung benötigt geeigneten mechanischen Schutz und Zugentlastung. **NYY-J ist nicht automatisch stahlarmiert oder nagersicher**; Schutz gegen freiliegende, zugängliche Kabelabschnitte ist gesondert festzulegen.
- Eine geeignete Sicherung beziehungsweise strombegrenzende Schutzvorrichtung ist **nahe der Einspeisung im Haus** vorzusehen; Auslegung anhand Stromkreisschutz, Leitung, Netzteil und Einschaltstrom.
- Feuchteschutz, Erdverlegetiefe, Hausein-/Ausführung, Biegeradien, Kennzeichnung und Leitungsabschlüsse sind vor Ausführung zu prüfen.

### 4.4 Hauptheizdraht und PWM

- Material: **lackierter Stahl-Gartendraht**, elektrisch als Widerstandsheizung zweckentfremdet; ein spezifizierter Heizleiter ist bisher nicht nachgewiesen. Nominalwiderstand **ca. 4 Ω**; tatsächlichen Kalt- und Warmwiderstand sowie die Lackisolierung prüfen.
- Versorgt aus `VIN` über Q1 (IRLZ44NPBF, N-MOSFET Low-Side) und U6 (TC4427CPA, nichtinvertierender MOSFET-Gate-Treiber).
- Vorgesehene **PWM-Frequenz 20 kHz**, Startauflösung **10 Bit**. Die Frequenz kann nach EMV-/Schaltverlust-/Rippelstromtest angepasst werden.
- Stellgröße des Temperaturreglers: **PWM-Tastgrad 0–100 % vor Anwendung der Sicherheitsgrenze**. Es wird im Normalbetrieb keine exakte Watt-Leistungsregelung gefordert.
- **Keine zusätzliche externe Serieninduktivität/Freilaufdiode zur Stromglättung vorgesehen.** Der Heizdraht gilt für die Grundschaltung als ohmsche Last; D1 ist dafür normalerweise nicht zu bestücken.

| Modellwert bei 24 V / 4 Ω | Ergebnis |
|---|---:|
| Strom in der EIN-Phase | 6 A |
| Momentanleistung in der EIN-Phase | 144 W |
| 25 % PWM (ideal) | 36 W mittlere Drahtleistung |
| 41,7 % PWM (ideal) | 60 W mittlere Drahtleistung |
| 60-W-Betrieb: Last-RMS-Strom | ca. 3,87 A |
| 60-W-Betrieb: mittlerer Heizstrom | ca. 2,50 A |
| 100 % PWM | 144 W (unzulässig für den geplanten Normalbetrieb) |

Die Tabelle sind **Idealwerte**. Der Spannungsabfall des Kabels, MOSFET-Verluste, Drahttemperatur, Kontaktwiderstände und ungenaue ADC-Messung verändern die reale Leistung. Der PI-Regler ist deshalb **direkt auf PWM** auszulegen und durch einen konservativen, unabhängig vom PI berechneten maximalen Tastgrad zu begrenzen. Der bisherige Standard von ca. **36 W** ist der vorläufige normale Freigabebereich; eine Betriebsgrenze bis **60 W** muss gesondert thermisch und elektrisch qualifiziert werden.

**Bauteilschutz:** Der Heizdraht kann an lokalen Engstellen deutlich heißer als die eingebetteten Plattensensoren sein. Die Beständigkeit von Lack, Stahl, Anschlüssen, Fliese/Beton und benachbarten brennbaren Materialien ist nachzuweisen. Alle Tests zunächst ohne Tiere und unter Aufsicht.

### 4.5 Pumpen und Kühlkreisläufe

- **Kühlkreispumpe** 12 V: Wasser vom Tank durch die Kupferrohrschleife in der Platte.
- **Verdunstungspumpe** 12 V: Wasser vom Tank auf die ca. 1,5 m² große beschattete Moosfläche, zurück in den Tank.
- Beide Pumpen dürfen in der Kühlbetriebsart unabhängig oder gemeinsam laufen; **Heizdraht und eine beliebige Pumpe niemals gleichzeitig**.
- Q2/J4 ist der 12-V-Ausgang für die Kühlpumpe; Q3/J5 ist derzeit als **umschaltbarer Reserveausgang** an 12 V oder VIN beschaltet. Die endgültige Zuordnung Q3/J5 zur Verdunstungspumpe ist **vor Softwarefreigabe** festzulegen. Eine 12-V-Pumpe darf nur mit korrekt auf 12 V gesetztem Versorgungsjumper betrieben werden.
- Pumpentyp, Anlauf-/Blockierstrom und der Einsatz von D2/D3 als Freilaufdiode oder anderer Überspannungsschutz sind anhand der tatsächlichen Pumpenelektronik zu prüfen.

### 4.6 Bodenplatte und Wärmedämmung

- Geometrie: ca. **35 × 35 cm**, Stärke noch offen; mit integrierter Kupferrohrschleife, Stahl-Heizdraht und 3×3-Matrix der Plattensensoren, oberseitig Fliesen.
- **40 mm EPS** unter und an den Seiten der Platte sowie am restlichen Rückzugsraum gemäß Abschnitt 3.1; raumseitig bzw. nach vorne Schutz durch **3–4 mm Faserplatte**.
- Die räumliche Temperaturverteilung muss bei ungleichmäßiger Heuauflage und durch Tiere veränderter Wärmeübertragung beurteilt werden.
- Die Auslegung der maximalen Plattentemperatur und der unabhängigen thermischen Sicherung muss den gedämmten Aufbau berücksichtigen.

### 4.7 Sensoren und Busse

| Sensor | Anzahl | Einbauort | Schnittstelle / Status |
|---|---:|---|---|
| DS18B20 | 9 | Bodenplatte, 3×3-Matrix | Separater 1-Wire-Bus `DATA_Plate`, GPIO13; 5-V-VDD, 3,3-V-Datenpullup R11 |
| DS18B20 | 1 | Wassertank | Eigener 1-Wire-Bus `DATA_Tank`, GPIO14; 5-V-VDD, 3,3-V-Datenpullup R12 |
| SHT45 **Breakout** | 1 | Innerer Rückzugsraum | I²C, Adresse 0x44, Modulspannung/Logikpegel prüfen |
| BME280 **Breakout** | 1 | Stallaußenluft | I²C, vorläufig 0x76 |
| BME280 **Breakout** | 1 | Stallluft | I²C, vorläufig 0x77 |
| IC-Gehäusetemperatur | 0 (optional) | Gate-Treiber-Gehäuse | GPIO32 reserviert; **nicht vorhanden, kein Feedback für Q4** |

Bei den DS18B20 sind die ROM-Adressen einmalig zu erfassen und fest auf Sensorpositionen abzubilden. Die Erkennungsreihenfolge ist kein dauerhafter Positionsbezeichner. Bei den I²C-Modulen dürfen SDA/SCL und ihre Pullups **nie mit 5 V am ESP32** anliegen; eine 5-V-Modulversorgung ist nur bei nachweislich geeigneten Breakouts erlaubt. Die beiden BME280 müssen verschiedene Adressen oder getrennte Busse/MUX besitzen.

### 4.8 Leistungsteil, Gate-Treiber und Eingangspuffer

- Q1, Q2, Q3: **IRLZ44NPBF**, 55-V-N-MOSFET, TO-220AB, Low-Side. Die Gate-/Drain-/Source-Footprints sind vor Fertigung zu prüfen; die Metalllasche liegt auf Drainpotential.
- U6: **TC4427CPA**, Dual-MOSFET-Treiber, **nichtinvertierend**, DIP-8, `VDD=12V`, Input A über GPIO18, Input B über GPIO19, 22-Ω-Ausgangswiderstände und 10-kΩ-Gate-Pulldowns. Direkter 3,3-V-GPIO-Betrieb von Q2 ist wegen der fehlenden garantierten `R_DS(on)`-Werte bei 3,3 V gesondert zu prüfen.
- C8=100 nF und C9=1 µF direkt an U6 `VDD/GND`; kurze niederinduktive Anbindung.
- C5/C6 jeweils 2200 µF / 50 V, ChongX VEHT, parallel an VIN; C7 100 nF / 50 V. Den 20-kHz-Stromrippel, ESR und die Kondensatortemperatur messtechnisch prüfen.
- Für 6 A EIN-Strom gilt im idealisierten Modell des vollständig durch die Kondensatorbank geglätteten Quellenstroms `I_C,RMS = 6 A × sqrt(D×(1−D))`. Bei D=0,5 maximal **3 A RMS für die Bank**, im 60-W-Fall D≈0,417 ca. **2,96 A RMS**; bei symmetrischer Teilung je Elko etwa **1,48 A RMS**. Die tatsächliche Teilung hängt von ESR, ESL und Layout ab.
- Ohne spezifizierten zulässigen Rippelstrom sind C5/C6 **noch nicht für dauerhaften Betrieb qualifiziert**. Bei Unsicherheit spezifizierte Low-ESR-/High-Ripple-Bauteile wählen; die Aufschrift `105 °C` belegt die Eignung allein nicht.
- Die Leiterbahnen und Verbinder des Heizdrahts müssen 6-A-Pulsstrom, Last-RMS-Strom, Einschalt-/Fehlerstrom und Transienten sicher beherrschen. Die Schleife `Eingangspuffer → Heizdraht → Q1 → Puffer-GND` ist kurz und niederinduktiv zu führen.
- F1/Eingangsschutz, Verpolschutz und gegebenenfalls TVS sind zu dimensionieren und vor Freigabe festzulegen.

### 4.9 ADC-Spannungsmessung (nur Diagnose und Plausibilität)

- ADC1 des ESP32: **GPIO34**, Netz `VIN/16` bzw. künftig konsistent `VIN_ADC`.
- R10 = **150 kΩ** von VIN zum Teilermittelpunkt, R9 = **10 kΩ** nach GND; nominaler Teilerfaktor **1:16**. 24 V werden zu ca. 1,5 V ADC-Spannung.
- R13 = **3,3 kΩ** in Reihe zwischen Teilermittelpunkt und ADC-Seite; C10 = **100 nF** zwischen ADC-Seite und GND als Filter; D4 als Über-/Transientenschutz **noch nicht abschließend dimensioniert**.
- Aktueller Referenzkandidat D4: **2,5-V-LM4040-Shuntreferenz** im dreipoligen SOT-23. Ein normaler 2-Pin-Diodenplatz ist nicht identisch mit dem vorgesehenen Gehäuse. Bei der relativ hochohmigen Teilerstufe ist der für präzise Zener-/Referenzklemmung nötige Mindeststrom über den gesamten Fehlerbereich **nicht gewährleistet**. Deshalb keine ungeprüfte 2,5-V-Hartbegrenzung behaupten; Schutz gegen Fehler des R10 sowie gegen höhere VIN-Spannungen elektrisch verifizieren.
- ADC-Umrechnung und Kalibrierung erfolgen gegen ein geeignetes Multimeter; Spannung, Messgültigkeit und Zeitstempel werden protokolliert.
- **Die PI-Regelung benutzt direkt PWM, nicht eine aus ADC und Drahtwiderstand geschätzte Wattzahl.** Der ADC misst weder Strom noch reale Heizleistung. Er darf allein keinen Überstromschutz oder einen unabhängig sicheren 60-W-Grenzwert vortäuschen.
- Eine konservative PWM-Maximalgrenze wird anhand der freigegebenen **maximal möglichen Versorgungsspannung**, des **minimalen Heizdrahtwiderstands** und der hardwarebezogenen Grenzwerte festgelegt. ADC-Werte dürfen zusätzlich die Betriebsfreigabe bei plausibler Unter-/Überspannung unterstützen. Bei ungültiger Spannungsmessung ist ein sicherer Fehlerzustand zu definieren, vorzugsweise Sperre der Hauptheizung.

### 4.10 Zusatzheizung des Gate-Treibers (Q4)

- U6 = **TC4427CPA** ist in der kommerziellen Variante nur für ungefähr **0 bis +70 °C** spezifiziert. Die Heizung dient als **experimentelle lokale Erwärmung**, nicht als Nachweis einer Eignung unter 0 °C.
- Q4 = **2N2222, NPN, Low-Side**; Pin-Funktionen: Emitter=GND, Basis über R15=330 Ω von ESP GPIO33 (`Heat_PWM`), Basis-Pulldown R14=10 kΩ, Kollektor zur Widerstandsbrücke. Reale C/B/E-Beinreihenfolge und Footprint prüfen.
- J8/J9 ermöglichen Bestückung aus **zwei 150-Ω-Widerständen in Reihe (300 Ω gesamt)** oder **vier Widerständen: (150 Ω || 150 Ω) + (150 Ω || 150 Ω) = 150 Ω gesamt**. J8-Pins 1–4 bilden den Mittelpunkt, J9-Pins 1/2 führen zum Q4-Kollektor, J9-Pins 3/4 zu 12 V. Der zuletzt dokumentierte Schaltplan hatte J9-Pin 3 noch **offen**; vor Fertigung ergänzen und mit Netzliste prüfen.
- Bei 12 V: **0,48 W** Heizleistung in Zweierbestückung oder **0,96 W** in Viererbestückung; jeweils **0,24 W pro eingesetztem 150-Ω-Widerstand**. Nennlast **0,25 W** ist thermisch knapp; bevorzugt für die reale Konstruktion **mindestens 0,5-W-Typen**, sofern mechanisch geeignet.
- Ansteuerung über **langsames PWM-Signal, vorläufig 1 Hz**. Keine Rückmeldung durch Messung direkt am IC. Der Tastgrad folgt einer **konfigurierbaren, monotonen Kennlinie aus gültiger Außentemperatur** mit noch festzulegenden Stützpunkten und Tastgradgrenzen.
- Bei Außensensorfehler, ungültiger Kennlinie oder unzulässigem Testzustand ist der Heizausgang definiert auszuschalten. Der Betriebszustand des Haupt-Gate-Treibers bei Frost ist **ohne lokale Temperaturmessung nicht nachweislich sicher**. Kaltstart, Aufwärmphase und Freigabe für die Hauptheizung sind separat zu qualifizieren; vorzugsweise industriellen Treiber verwenden.

### 4.11 Bauteilliste, Stand PCB-Pinout v0.7

**Status:** Bestückungs-/Anschlussplanung, keine automatisch fertigungstaugliche BOM. Bei generischen Schaltplansymbolen sind Werte, Footprints, Polarität und Pin-Nummern noch zu verifizieren.

| Ref. | Vorgesehene Ausführung | Funktion / Hinweis |
|---|---|---|
| U1 | ESP32-DevKitC / ESP32-WROOM | Steuerung; Pinrevision prüfen |
| U2 | SHT45-Breakout, 4-polig | Rückzugsraum, Temperatur/Luftfeuchte |
| U3 | BME280-Breakout, 4-polig | Außenluft; I²C-Adresse 0x76 vorläufig |
| U4 | DC/DC Buck VIN → 5 V | ESP32, geeignete Sensorik |
| U5 | DC/DC Buck VIN → 12 V, vorläufig 3 A max. | Pumpen und Treiber |
| U6 | TC4427CPA, DIP-8 | Zweifach-MOSFET-Treiber, +12 V |
| U7 | BME280-Breakout, 4-polig | Stallklima; I²C-Adresse 0x77 vorläufig |
| U8 | DS18B20-3-Pin-Busanschluss | 9 Sensoren Platte |
| U9 | DS18B20-3-Pin-Busanschluss | 1 Sensor Tank |
| Q1–Q3 | 3 × IRLZ44NPBF, TO-220AB | Heizung, Kühlpumpe, Reserve/Verdunstungspumpe |
| Q4 | 2N2222 NPN, TO-92-Variante prüfen | Gate-Treiber-Zusatzheizung |
| R1–R3 | 3 × 10 kΩ | Gate-Source-Pulldowns Q1–Q3 |
| R4–R5 | 2 × **22 Ω** (`22R`) | Gate-Serienwiderstände an U6 |
| R6–R7 | 2 × 10 kΩ | Pulldowns der U6-Eingänge |
| R8 | 220 Ω | GPIO-Serienwiderstand Q2-Gate |
| R9 | 10 kΩ, 1 % | Unterer ADC-Spannungsteiler |
| R10 | 150 kΩ, 1 % | Oberer ADC-Spannungsteiler |
| R11–R12 | 2 × 4,7 kΩ | Pullups der getrennten DS18B20-Datenbusse nach 3V3 |
| R13 | 3,3 kΩ | ADC-Serien-/Schutzwiderstand |
| R14 | 10 kΩ | Q4-Basis-Pulldown |
| R15 | 330 Ω | Q4-Basisvorwiderstand |
| R16–R17 | je 4,7 kΩ, **optional / DNP** | I²C-Pullups, nur bei Bedarf; im letzten Screenshot noch nicht eingezeichnet |
| C1 | 470 µF / 50 V | 12-V-Ausgangspuffer |
| C2 | 100 nF / ≥25 V, Keramik | 12-V-HF-Entkopplung |
| C3 | 220 µF / 16 V | 5-V-Puffer |
| C4 | 33 nF / ≥16 V | 5-V-HF-Entkopplung |
| C5–C6 | 2 × 2200 µF / 50 V | 24-V-VIN-Puffer, Rippelfestigkeit ungeklärt |
| C7 | 100 nF / ≥50 V | VIN-HF-Entkopplung |
| C8 | 100 nF / ≥25 V | U6-Entkopplung direkt an VDD/GND |
| C9 | 1 µF / ≥25 V | U6-Entkopplung direkt an VDD/GND |
| C10 | 100 nF / ≥16 V | ADC-Filter an ADC-Seite von R13 |
| D1 | **DNP** | Ohmscher Heizdraht: keine Freilaufdiode erforderlich |
| D2 | Dioden-/Schutzplatz | Kühlpumpe, Typ abhängig von Motor |
| D3 | Dioden-/Schutzplatz | Reserve-/Verdunstungspumpe, Typ abhängig von Last |
| D4 | 2,5-V-Shuntreferenz / alternativer Klemmschutz **offen** | ADC-Schutz, derzeit nicht freigegeben |
| J1 | Heizdraht, 2-polig | 24-V-Heizlast |
| J2/J3 | Jumper `RESERVE_PLUS` auf 12 V oder VIN | **Nie beide gleichzeitig** |
| J4 | Kühlpumpe, 2-polig | 12-V-Ausgang |
| J5 | Reserve-/Verdunstungspumpe, 2-polig | endgültige Belegung prüfen |
| J6 | VIN-Eingang, 2-polig | +24 V/GND |
| J7 | 12-V-Bypass-Jumper | nur bei 12-V-VIN und vollständig getrenntem U5 |
| J8 | 4-polige Mitte der Widerstandsbrücke | alle Pins gemeinsames Netz |
| J9 | 4-polige Endanschlüsse der Widerstandsbrücke | 1/2 Kollektor, 3/4 12 V; J9-3-Korrektur prüfen |
| F1 | **Soll / nicht im letzten Screenshot** | Sicherung/Leitungsschutz nahe Einspeisung |
| TVS1 | optional / offen | VIN-Überspannungsschutz |
| TH1 | **offene Sicherheitsanforderung / nicht vorhanden** | unabhängiger Heizdraht-Temperaturbegrenzer |

**Nicht auf dem PCB enthaltene Komponenten:** 24-V-/3-A-Netzteil, 15-m-NYY-J-3×2,5-mm²-Zuleitung, Heizdraht, Pumpen, 10 Temperaturfühler, SHT45-/BME280-Module und Dämmmaterial. Die physischen Widerstandsplätze für die Zusatzheizung sitzen auf/über U6, ihre externe Verdrahtung erfolgt über J8/J9.

### 4.12 ESP32-GPIO-Zuordnung (Sollstand v0.7)

| Funktion | GPIO | KiCad-Symbolpin U1 | Signal / Hinweise |
|---|---:|---:|---|
| Gate-Treiber-Heizdraht A | 18 | 30 | `Q1_TRIG`, 20-kHz-PWM |
| Gate-Treiber-Reserve B | 19 | 31 | `Q3_TRIG`, Reserve/Pumpe |
| Kühlpumpe Q2 | 23 | 37 | `Q2_TRIG`, digital |
| I²C SDA | 21 | 33 | `SDA`, nur 3,3-V-Pegel |
| I²C SCL | 22 | 36 | `SCL`, nur 3,3-V-Pegel |
| ADC-VIN | 34 | 5 | `VIN_ADC` / `VIN/16`, ADC1_CH6, Eingang |
| DS18B20-Platte | 13 | 15 | `DATA_Plate`, 9 Sensoren |
| DS18B20-Tank | 14 | 12 | `DATA_Tank`, 1 Sensor |
| Zusatzheizung Q4 | **33** | **8** | `Heat_PWM`, ca. 1 Hz, **nicht GPIO32** |
| Gehäusesensor optional | **32** | **7** | Noch nicht belegt, kein Feedback vorhanden |
| 5-V-Eingang | 5V | 19 | U4-Ausgang, kein GPIO |
| 3,3 V | 3V3 | 1 | DevKit-Regler, kein GPIO |
| Masse | GND | 14 | Gemeinsame Logik- und Leistungsmassen, geeignete Topologie |

Die **ESP32-Symbol-Pinnummern** sind nicht mit den GPIO-Ziffern gleichzusetzen. Nach Auswahl des realen DevKitC und Footprints ist die Zuordnung per KiCad-Netzliste/ERC zu bestätigen.


### 4.13 Verbindliche Referenzen und Nachweise

- Projektbezogene PCB-Anschlussplanung: `PCB_Klimaregelung_Pinout_v0.7.md` (Stand 16.11.2026). Werte und nicht geprüfte Schaltplanstellen werden in Kapitel 25 gesondert gekennzeichnet.
- Vorgänger: `Pflichtenheft_Klimaregelung_v1.2.md` (10.11.2026); dessen Web-/MQTT-, Kühl-, Verdunstungs- und Taupunkt-Fallbackfunktion wurde grundsätzlich beibehalten.
- [Microchip TC4426/TC4427/TC4428: Datenblatt](https://ww1.microchip.com/downloads/en/DeviceDoc/20001422G.pdf) – Temperaturbereich und Treiberspezifikation.
- [Infineon IRLZ44N: Datenblatt](https://www.infineon.com/assets/row/public/documents/24/49/infineon-irlz44n-datasheet-en.pdf) – Gate-, Drain- und SOA-Grenzen.
- [Prysmian: NYY-Erdkabel](https://dk.prysmiangroup.com/sites/default/files/atoms/files/Prysmian_Group_Wind_Turbine_Cable_Catalogue_Sep-2018.pdf) – NYY-J mit grün-gelbem Schutzleiter und Verlegeeigenschaften.
- [TI LM4040: Datenblatt](https://www.ti.com/lit/ds/symlink/lm4040.pdf) – Schutzelementkandidat, **Mindestkathodenstrom und Package-Pins vor Bestückung nachweisen**.
- [Espressif ESP32-DevKitC: User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html).
- [Analog Devices DS18B20: Datenblatt](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf).

Dieser Referenzabschnitt ist eine Liste von Nachweisen für konkrete Komponenten; die verwendeten Modulausführungen, Seriennummern, Wandlerdaten und Kabelfarben müssen am gelieferten Material nochmals abgeglichen werden.

---

## 5. Softwarearchitektur

Die Firmware ist modular, testbar und ohne blockierende Verzögerungen zu implementieren. Empfohlene Module:

| Modul | Aufgabe |
|---|---|
| `ConfigurationManager` | Validierte, versionierte, nichtflüchtige Konfiguration; Safe Defaults |
| `SensorManager`, `PlateSensorManager` | DS18B20-Identifikation, Mittel-/Min-/Max-/Medianwerte, Fehlerdiagnose |
| `ClimateSensorManager`, `TankSensorManager` | I²C-Klimasensoren und Tankdaten |
| `DewPointCalculator`, `WetBulbCalculator` | Taupunkt und Feuchtkugeltemperatur |
| `StateMachine` | Exklusive Betriebsarten und Übergänge |
| `PIHeatingController` | PI-Temperaturregler mit PWM-Stellgröße und Anti-Windup |
| `HeatingPwmOutput` | 20-kHz-Hardware-PWM, Tastgradbegrenzung, Sofortsperre |
| `CoolingController`, `EvaporationController` | Pumpenfreigaben, Taupunkt-/Tank-/Außenlogik |
| `DriverHeaterController` | Außenluftabhängige Q4-Zusatzheizung ohne lokales Feedback |
| `ActuatorManager`, `SafetyManager` | Ausgangsverriegelung, sicherheitskritische Freigaben, Sperrgründe |
| `ManualControlManager` | Zeitlich limitierter Servicebetrieb mit nichtkritischen Overrides |
| `SystemIdentificationManager` | PWM-Sprünge, Phasen, Abbruch, Metadaten, Aufzeichnung |
| `HistoryManager`, `MeasurementRecorder` | Ringpuffer, Trend-/CSV-Datensätze, Speicherkapazität |
| `PowerDiagnostics` | ADC-VIN, Plausibilität, Spannungsverlauf und Warnungen |
| `FaultManager`, `Diagnostics` | Fehlerstatus, Historie, UART-Diagnose |
| `WebServerManager`, `WiFiManager`, `MqttManager` | Bedienung und Schnittstellen |

**Architekturprinzip:** Alle Anforderer liefern nur **Wünsche** (PWM oder Pumpenstatus). Ausschließlich der `ActuatorManager` nach Prüfung durch `SafetyManager` überträgt diese in physische GPIO-/PWM-Signale. Damit können Automatik, manuelle Bedienung und Identifikationsmodus die Sicherheitsgrenzen nicht durch Umgehung einer Funktionsschicht außer Kraft setzen.

---

## 6. Betriebszustände und Betriebsarten

Der Zustandsautomat kennt mindestens:

- `INITIALISIERUNG`
- `NEUTRAL`
- `HEIZEN_PI`
- `KUEHLEN`
- `VERDUNSTUNGSKUEHLUNG`
- `KUEHLEN_MIT_VERDUNSTUNG`
- `MANUELL` (zeitlich begrenzte Servicebedienung)
- `SYSTEMIDENTIFIKATION` (definierter PWM-Sprungversuch)
- `STOERUNG`

### 6.1 Sicherer Grundzustand

Hauptheizdraht PWM=0 und Ausgang AUS, Kühlkreispumpe AUS, Verdunstungspumpe AUS, Q3/Reserve AUS. Zusatzheizung Q4 AUS bis zu gültiger separater Freigabe. Nach Neustart dürfen weder zuvor gesetzte manuelle Ausgänge noch ein laufender Identifikationsversuch automatisch wiederhergestellt werden.

### 6.2 Ausgangsverriegelung

- **Jeder aktive Hauptheizdraht-PWM-Tastgrad >0 und jede eingeschaltete Pumpe schließen sich gegenseitig aus**; auch im manuellen/Identifikationsmodus.
- Beide 12-V-Pumpen dürfen im Normalfall gleichzeitig laufen, sofern Netzteil-, Wandler- und Ausgangsfreigaben gültig sind.
- Bei Zustandswechsel zuerst Hauptheizungs-PWM auf 0 setzen und den zugehörigen Ausgang zuverlässig sperren; erst danach Pumpen freigeben. Für den umgekehrten Wechsel zuerst alle Pumpen AUS.
- Fehlende oder falsche Q3-Versorgungsjumper-Belegung ist ein Inbetriebnahmefehler; eine 12-V-Pumpe darf nicht auf VIN=24 V betrieben werden.
- `DriverHeaterController` schaltet ausschließlich Q4 als separate Niedrigleistungsheizung. Hauptheizdraht und Gate-Treiber-Heizbrücke sind unterschiedliche Lasten.

### 6.3 Betriebsarten-Priorität

`STOERUNG/Sicherheitsabschaltung` hat stets höchste Priorität, gefolgt von aktiver Service-/Identifikationssitzung innerhalb freigegebener Grenzen und ansonsten der Automatik. Es darf zu einem Zeitpunkt nur **eine** Hauptsteuerquelle geben. Bei Ablauf, Abbruch oder Neustart fallen manuelle und experimentelle Befehle weg; automatischer Wiederanlauf ist erst nach definierter Übergabe und Neuinitialisierung des Reglers zulässig.

---

## 7. Verarbeitung der Plattentemperaturen

Alle neun DS18B20 in der Platte besitzen fest hinterlegte ROM-ID-zu-Position-Zuordnungen. Aus jedem neuen vollständigen beziehungsweise plausiblen Messzyklus werden über die **gültigen** Sensoren berechnet:

- `plate_min`: niedrigste gültige Plattentemperatur;
- `plate_max`: höchste gültige Plattentemperatur;
- `plate_mean`: arithmetischer Mittelwert (**Regelgröße des PI-Heizreglers**);
- `plate_median`: Median zur Robustheits- und Sensorfehlerdiagnose;
- `plate_spread = plate_max − plate_min`: räumliche Temperaturspreizung;
- Anzahl gültiger Sensoren und Alter jedes Einzelwerts.

| Kennwert | Verwendung |
|---|---|
| **Mittelwert** | PI-Heizregelung, Trends, Sprungantwort, MQTT |
| **Maximum** | Kritische Heizabschaltung und Beobachtung lokaler Hotspots |
| **Minimum** | Taupunktprüfung beim Kühlen, Frostschutz-Diagnose |
| **Median** | Plausibilität und Ausreißerdiagnose |
| **Spreizung** | Erkennen veränderter Heu-/Tierbedeckung oder möglicher Sensorfehler |

Die Sicherheitsprüfung verwendet **ungefilterte, frisch gültige** kritische Messwerte; eine etwaige gleitende Filterung für den PI-Regler darf die Notabschaltung nicht verzögern. Fallen alle Plattensensoren aus, werden Hauptheizung und Plattenkühlung gesperrt. Bei Ausfall einzelner Sensoren gilt die bestehende Weiterbetriebsregel mit den verbleibenden gültigen Sensoren, verbunden mit Warnung; die hierfür sicherheitskritisch zulässige Mindestanzahl und räumliche Abdeckung sind vor unbeaufsichtigter Freigabe zu validieren.

---

## 8. Heizungsregelung – PI-Regelung mit PWM als Stellgröße

### 8.1 Grundprinzip

Die ursprüngliche reine **Zweipunkt-Hystereseregelung** für normales Ein/Aus der Hauptheizung wird durch einen **kontinuierlich wirkenden PI-Regler** ersetzt. Ziel ist ein möglichst konstanter Wärmefluss und geringere langsame Temperaturzyklen des Heizdrahts.

- **Regelgröße:** `plate_mean` in °C.
- **Sollwert:** `heating_target` in °C.
- **Stellgröße:** `heater_pwm_duty`, normiert auf `0...1`, vor der abschließenden Sicherheitsbegrenzung.
- **Störgrößen:** Außentemperatur, Stall-/Raumtemperatur, Wind/Luftaustausch, Tiere, Feuchte und Einstreu/Heu. Heu und Tiere können auch die Streckenverstärkung und Zeitkonstante verändern.
- **PWM:** 20 kHz (vorläufig), Hardware-Timer/LEDC, getrennt vom 5-s-PI-Regelzyklus.

Für PI-Parametrierung ist die Spannung nur eine Diagnosegröße. Das System benötigt **keine exakt bestimmte Heizleistung in Watt**; Kabelabfall und Verlustleistung werden bei der späteren PWM-Sprungantwort in der realen Prozessverstärkung erfasst.

### 8.2 Mathematisches Regelgesetz

```text
error_K = heating_target_C - plate_mean_C

u_unlimited = Kp_duty_per_K * error_K + integral_duty
heater_pwm_duty_requested = clamp(u_unlimited, 0, duty_hard_limit)

integral_duty += Ki_duty_per_Ks * error_K * dt_seconds
```

Die letzte Zeile ist nur **schematisch**: In der Firmware ist eine geeignete **Anti-Windup-Strategie** einzusetzen (z. B. bedingte Integration bei Sättigung/Fehlern oder Back-Calculation). Der Integrator darf nicht anwachsen, wenn der Ausgang durch Sicherheitsgrenzen gesperrt ist. Beim Wechsel Automatik ↔ Manuell ↔ Identifikation ist ein stoßfreier Übergang (`bumpless transfer`) bzw. definierter Re-Start des Integrators vorzusehen.

### 8.3 Vorläufige PI-Startwerte (nicht qualifiziert)

Die folgenden Zahlen beruhen auf einem **früheren hypothetischen Ersatzmodell** (angenommene 4-cm-Platte, Wärmeverlust etwa 2 W/K, thermische Zeitkonstante ca. 83 min, angenommene Totzeit). Die reale Plattendicke, Dämmung und Wärmeverluste sind **noch nicht verifiziert**. Nach der neuen 40-mm-EPS-Dämmung ist eine Änderung zu erwarten.

| Parameter | Entwicklungsstartwert |
|---|---:|
| `pi_kp_duty_per_k` | ca. **0,027 1/K** |
| `pi_ti_seconds` | ca. **4980 s** (83 min) |
| `pi_ki_duty_per_k_second` | ca. **5,4 × 10⁻⁶ 1/(K·s)** |
| `pi_update_interval_ms` | **5000 ms** |
| `heater_pwm_frequency_hz` | **20000 Hz** |
| `heater_pwm_resolution_bits` | **10 Bit** |
| Temperaturmessintervall | vorläufig **2–5 s** |
| PI-Temperaturfilter | optional 30–60 s, **nicht für Sicherheit** |
| D-Anteil | **0** |

Die Werte sind nur zum Anfahren unter Aufsicht und mit eingeschränktem Tastgrad bestimmt. Die definitiven Parameter werden anhand der **realen PWM-Sprungantwort der eingebauten und gedämmten Platte** bestimmt (Kapitel 27).

### 8.4 Freigabe und Abschaltung

Der PI-Regler darf nur physikalisch wirken, wenn:

- Heizfunktion aktiviert und Automatikzustand `HEIZEN_PI` zulässig;
- Hauptheizdraht und Pumpen-Verriegelung erfüllt;
- ausreichend gültige, frische Plattensensorwerte vorliegen;
- `plate_max < heating_critical_max` und kein kritischer Sensor-/Hardwarefehler vorliegt;
- das freigegebene PWM-Hard-Limit gültig ist und die Versorgung im qualifizierten Bereich liegt;
- Gate-Treiber und Ausgang nach der tatsächlich qualifizierten Temperatur-/Startstrategie freigegeben sind.

Beim Erreichen einer **kritischen** Maximaltemperatur gilt sofort PWM=0, unabhängig von PI-Anforderung. Eine **Wiedereinschalthysterese** und ggf. Fehlerquittierung dürfen nur die Wiederfreigabe betreffen, nicht die kritische Abschaltung. Der PI-Regler selbst benötigt keine normale Ein-/Aus-Hysterese mehr.

### 8.5 Tastgrad- und Leistungsbegrenzung

Der Ausgang wird nach dem PI-Regler begrenzt:

```text
duty_actual = min(max(0, duty_requested), duty_limit_qualified)
if any_critical_heater_inhibit: duty_actual = 0
```

**Das PWM-Hard-Limit ist unabhängig vom PI-Regler** anhand einer konservativen zulässigen Spannung (z. B. maximal erlaubte Netzteilspannung), eines minimal anzunehmenden Heizdrahtwiderstands und der thermisch qualifizierten Leistungsstufe festzulegen. Eine unkalibrierte ADC-Spannungsmessung darf die Grenze **nicht beliebig erhöhen**.

Bei genau 24 V / 4 Ω sind **25 % PWM ≈36 W** und **41,7 % PWM ≈60 W** idealisiert. Diese Werte sind keine pauschal sicheren Grenzen bei abweichenden Spannungen/Widerständen. Der für 60 W nötige Tastgrad ist erst nach elektrischer und thermischer Prüfung freizugeben; bis dahin gilt ein niedrigeres, qualifiziertes Maximum. Ein höherer Tastgrad im manuellen Modus bleibt ebenfalls verboten.

Ein programmatischer Anstieg der normalen Stellgröße kann über eine **Rampenbegrenzung** reduziert werden. Sicherheitsabschaltungen, Pumpenverriegelung und Watchdog dürfen durch eine Rampe **nicht verzögert** werden.

**Wichtige Einschränkung:** Die Schaltung enthält keinen Laststromsensor. Es gibt daher **keine echte laufende Überstrommessung** durch den ESP32. Der softwareseitige Schutz besteht gegenwärtig aus Ausgangsverriegelung, konservativer Tastgradbegrenzung, Spannungsplausibilität und Temperaturüberwachung; elektrische Kurzschlüsse oder MOSFET-Durchlegieren benötigen unabhängige Hardwaremaßnahmen.

### 8.6 Verhalten bei Sensorfehlern und Störungen

- **Einzelne Plattensensoren ungültig:** ursprüngliche Fallbackregel: Weiterregelung mit verbleibenden gültigen Werten, Warnung und Prüfung der Mindest-Sensorabdeckung; kritische Mindestanforderung in der Inbetriebnahme konkretisieren.
- **Alle Plattensensoren ungültig:** Hauptheizung sofort AUS, keine manuelle Ausnahme.
- **Außenluftsensor ungültig:** PI-Regelgröße bleibt verfügbar, aber Q4-Zusatzheizung kann ihre Temperaturkennlinie nicht bestimmen; Gate-Treiber-Frostfreigabe separat nach Kapitel 4.10 behandeln.
- **Kaninchen/Heu verändern Wärmeübergänge:** PI-Regler kompensiert mittlere Temperatur, kann die Heizdrahttemperatur und lokale Hotspots aber nicht messen. Plattenmaximum und Temperaturspreizung weiter überwachen.
- **Netzteilspannung variiert:** PI-Regler gleicht langsame Leistungsänderung nach, die Hardware-/PWM-Obergrenze wird nicht automatisch aufgeweicht.
- **Kühlbetrieb oder manuelle Bedienung aktiv:** PI-Integrator anhalten/nachführen, keine verdeckte Heizanforderung aufbauen.

### 8.7 Alternative Versorgung und Freigabestand

Eine 12-V-Versorgungsvariante mit J7-Bypass ist im PCB als Sonderfall vorgesehen, aber **nicht die aktuelle Nennausführung**. J7 darf **nie** unter 24 V oder bei elektrisch angeschlossenem 12-V-Buck U5 gesteckt werden. Die Eignung der Wandler, Pumpen und PWM-Grenzen ist für eine solche Variante gesondert nachzuweisen. Die Software darf nicht allein anhand einer ADC-Anzeige eine unsichere Hardware-Jumperkonfiguration freigeben.

---

## 9. Kühlungsregelung

### 9.1 Grundsätzliche Freigabe

Die Plattenkühlung darf nur betrieben werden, wenn:

- die Kühlfunktion aktiviert ist,
- keine Heizung aktiv ist,
- mindestens ein gültiger Plattensensor vorhanden ist,
- bei gültiger Außenmessung die Außentemperatur mindestens 15 °C beträgt; bei Sensorfehler entfällt **nur** diese Sperre,
- keine kritische Störung vorliegt,
- die Taupunktbedingung eingehalten ist,
- die Tanktemperaturbedingung eingehalten ist oder wegen Tanksensorfehler als erfüllt angenommen wird.

### 9.2 Einschaltbedingung

Die Kühlung beginnt, sobald die höchste gültige Plattentemperatur die obere Kühlschwelle überschreitet.

Die obere Kühlschwelle ergibt sich aus:

```text
Kühlschwelle_EIN = Zieltemperatur_Kühlen + Hysterese_Kühlen
```

### 9.3 Ausschaltbedingung

Die Kühlung endet, sobald die höchste gültige Plattentemperatur die konfigurierte Zieltemperatur für den Kühlbetrieb erreicht oder unterschreitet.

Die Kühlung endet außerdem immer, wenn die niedrigste gültige Plattentemperatur den zulässigen Mindestabstand zum Taupunkt unterschreitet.

Abschaltbedingung:

```text
niedrigste_PlattenTemperatur
<= Taupunkt + Mindestabstand_Taupunkt_Platte
```

### 9.4 Außentemperatursperre

Bei einer **gültig gemessenen** Außentemperatur unter 15 °C ist die Plattenkühlung grundsätzlich deaktiviert.

**Seit v1.2:** Bei Ausfall beziehungsweise ungültiger Messung des Außenluftsensors **entfällt ausschließlich diese Außentemperatursperre**. Die Plattenkühlung darf weiterlaufen, sofern alle anderen Kühl-, Tank-/Taupunkt- und Sicherheitsbedingungen erfüllt sind. Die Taupunkt-Fallbacklogik nach Abschnitt 10 bleibt anzuwenden. Die Verdunstungskühlung wird bei Ausfall des Außenluftsensors weiterhin deaktiviert.

### 9.5 Tanktemperaturbedingung

Die Kühlung darf nur aktiviert werden, wenn zwischen Tanktemperatur und Plattentemperatur ein ausreichender Temperaturabstand besteht.

Bedingung:

```text
Tanktemperatur
<= niedrigste_PlattenTemperatur - Mindestabstand_Tank_Platte
```

Damit wird verhindert, dass die Kühlkreispumpe ohne zu erwartenden Kühleffekt betrieben wird.

Fällt der Tanksensor aus, wird diese Bedingung als erfüllt angenommen. Die Plattenkühlung darf weiterlaufen.

### 9.6 Verhalten bei Ausfall von Plattensensoren

Bei Ausfall einzelner Plattensensoren wird die Kühlungsregelung mit den verbleibenden gültigen Sensoren fortgeführt.

Bei Ausfall aller Plattensensoren wird die Plattenkühlung deaktiviert.

---

## 10. Taupunktberechnung und Fallbacklogik

### 10.1 Normalbetrieb

Im Normalbetrieb wird der Taupunkt aus folgenden Innenraumwerten berechnet:

- Innenraumtemperatur,
- relative Innenraumluftfeuchtigkeit.

### 10.2 Ausfall des Innenraumsensors

Fällt der Innenraumsensor aus, werden die Messwerte des Außenluftsensors zur Taupunktberechnung verwendet.

Begründung: Der Rückzugsraum ist offen und gut belüftet, sodass die Luftzustände voraussichtlich nicht stark voneinander abweichen.

### 10.3 Ausfall beider Klimasensoren

Fallen Innenraum- und Außenluftsensor gleichzeitig aus, wird für die Kühlungsregelung ein fester Taupunkt von 23 °C angenommen.

Die Kühlung darf mit diesem Ersatzwert weiterarbeiten.

Dabei bleibt die Bedingung erhalten:

```text
niedrigste_PlattenTemperatur
> 23 °C + Mindestabstand_Taupunkt_Platte
```

### 10.4 Berechnungsverfahren

Zur Taupunktberechnung ist eine geeignete Näherungsformel zu verwenden, beispielsweise die Magnus-Formel.

---

## 11. Verdunstungskühlung

### 11.1 Freigabe

Die Verdunstungskühlung darf nur betrieben werden, wenn:

- die Verdunstungskühlung aktiviert ist,
- keine Heizung aktiv ist,
- der Tanksensor gültige Werte liefert,
- der Außenluftsensor gültige Werte liefert,
- keine kritische Störung vorliegt.

### 11.2 Einschaltbedingung

Die Verdunstungskühlung startet, sobald die Tanktemperatur den konfigurierten Mindestabstand zur theoretischen Feuchtkugeltemperatur überschreitet.

Bedingung:

```text
Tanktemperatur - Feuchtkugeltemperatur_Außen
>= Mindestwert_theoretische_Kühlkapazität
```

### 11.3 Ausschaltbedingung

Die Verdunstungskühlung endet, sobald die Differenz wieder unter den konfigurierten Mindestwert fällt.

Zur Vermeidung häufigen Taktens kann eine zusätzliche interne Hysterese vorgesehen werden.

### 11.4 Sensorfehler

Bei Ausfall des Tanksensors wird die Verdunstungskühlung deaktiviert.

Bei Ausfall des Außenluftsensors wird die Verdunstungskühlung deaktiviert.

Ein Fallbackbetrieb der Verdunstungskühlung ohne Tank- oder Außenluftsensor ist nicht zulässig.

---

## 12. Sensorfehler, Gültigkeit und Fallbackbetrieb

| Fehlerfall | Automatikverhalten | Test-/Manuellbetrieb |
|---|---|---|
| Tanksensor ungültig | Verdunstungskühlung AUS; Plattenkühlung ohne Tank-Platten-Abstandsprüfung weiterhin möglich | Tankabhängige Funktionen ohne gültige Daten nicht freigeben, sofern kritisch; übrige Pumpentests im freigegebenen Bereich möglich |
| Außensensor ungültig | Verdunstungskühlung AUS; **Außentemperatursperre** für Plattenkühlung entfällt; Taupunkt-Fallback bleibt wirksam | Zusatzheizung Q4 mangels gültiger Außenkennlinie AUS; Hauptheizung nur bei gesondert qualifizierter Gate-Treiber-Freigabe |
| Innenraumsensor ungültig | Taupunkt aus gültigem Außensensor | Manueller Taupunkt-Override nur als ausdrücklich angezeigter, zeitlich begrenzter Serviceeingriff |
| Innen- und Außensensor ungültig | Fester Taupunkt **23 °C** für Plattenkühlung (v1.1-Regel) | Manueller Taupunkt-Override nach Servicefreigabe; kritische Ausgangsgrenzen unverändert |
| Einzelne Plattensensoren ungültig | Weiterrechnung aus gültigen Sensoren; Warnung, Abdeckung prüfen | Kritische Temperaturüberwachung muss weiter hinreichend valide sein; sonst Heizbetrieb sperren |
| Alle Plattensensoren ungültig | Hauptheizung und Plattenkühlung AUS | **Keine Freigabe des Hauptheizdrahts** für manuelle PWM oder Sprungantwort |
| Plattenmaximum erreicht | Heizdraht sofort AUS; kritische Störung | **Nicht überschreibbar** |
| Versorgung außerhalb freigegebenem Bereich, ADC grob ungültig | Hauptheizbetrieb sperren; Diagnose ausgeben | **Keine Umgehung** der betreffenden Heiz-/Leistungsfreigabe |
| Nennleistungs-/PWM-Grenze erreicht | Stellgröße begrenzen, Anti-Windup | **Nicht überschreibbar** |
| Gate-Treiber-Freigabe/Kaltstart ungeklärt | Hauptheizung sperren, sofern Betrieb außerhalb qualifiziertem Temperaturbereich möglich | Keine Umgehung der Hardware-/Temperaturfreigabe |
| WLAN/MQTT nicht erreichbar | Lokale Regelung läuft unverändert weiter, Wiederverbindung nicht blockierend | Bereits gestartete Tests folgen lokal ihrem Zeitlimit; manuelle Sitzung läuft nur bis zu ihrer Gültigkeitsfrist |
| Watchdog-Reset/Neustart | Alle Aktoren zunächst AUS, Regler neu initialisieren | Manuelle Sitzung/Identifikation **nicht automatisch wieder aufnehmen** |

**Datenalter und Qualität:** Nicht allein `isConnected()` eines Sensors ist ausreichend. Für alle Freigaben sind Gültigkeit, Plausibilität und Alter des letzten Messwerts zu berücksichtigen. Sicherheitsabschaltungen dürfen nicht durch langsame Filter oder UI-Updates verzögert werden. Die bisherige 23-°C-Taupunkt-Ersatzannahme ist ein bewusstes Verhaltenskonzept und ersetzt keine verlässliche Feuchteinformation.

---

## 13. WLAN-Anbindung

### 13.1 Netzwerkanbindung

Der ESP32 wird per WLAN in ein lokales Netzwerk eingebunden.

Es ist mit einer mittelmäßigen WLAN-Abdeckung und wiederkehrenden Verbindungsabbrüchen zu rechnen.

### 13.2 Statische IP-Adresse

Der ESP32 erhält eine feste IP-Adresse im lokalen Netzwerk.

Die statische IP-Konfiguration muss mindestens folgende Werte enthalten:

- IP-Adresse,
- Subnetzmaske,
- Gateway,
- DNS-Server.

### 13.3 Verhalten bei Verbindungsverlust

Ein WLAN-Verbindungsverlust darf keinen Einfluss auf die lokale Heiz- und Kühlregelung haben.

Insbesondere gilt:

- Aktoren behalten ausschließlich den durch den lokalen Zustandsautomaten bestimmten Zustand.
- Ein Verbindungsverlust darf keinen Aktor ein- oder ausschalten.
- Es dürfen keine undefinierten oder unerwarteten Zustände entstehen.
- Der ESP32 versucht fortlaufend, die WLAN-Verbindung wiederherzustellen.
- Wiederverbindungsversuche dürfen die Regelzyklen nicht blockieren.
- Webserver- und MQTT-Funktionen werden nach erfolgreicher Wiederverbindung automatisch erneut bereitgestellt.

### 13.4 Wiederverbindungsstrategie

Die Wiederverbindung ist nicht blockierend umzusetzen.

Empfohlen wird eine gestufte Wiederholstrategie mit begrenzter Wiederholrate, beispielsweise:

- erste Wiederholungen in kurzen Abständen,
- anschließend längere Wiederholungsintervalle,
- keine Endlosschleife mit blockierenden Wartezeiten.

---

## 14. Lokaler Webserver

### 14.1 Grundlagen

Der ESP32 stellt unter einer festen lokalen IP-Adresse einen Webserver bereit. Der Webserver dient zur Anzeige, Parametrierung, Diagnose und zum beaufsichtigten Servicebetrieb. Die Bedienoberfläche ist nicht Bestandteil des sicherheitskritischen Regelpfads. Ein Verlust des Browsers oder WLANs darf die Automatik nicht blockieren.

### 14.2 Statusübersicht

Die Website zeigt mindestens:

- Systemzustand und aktive Steuerquelle (Automatik/Manuell/Systemidentifikation/Fehler);
- WLAN-, MQTT- und Messgültigkeitsstatus;
- Heizdrahtzustand, **angeforderten und tatsächlich wirksamen Hauptheiz-PWM-Tastgrad**, freigegebene PWM-Obergrenze und aktuell aktive Sperrgründe;
- PI-Sollwert, Plattenmittelwert, PI-Fehler und optional PI-Integratorstatus;
- Status beider Pumpen und des optional als Pumpe genutzten Reserveausgangs;
- **Q4-Gate-Treiber-Zusatzheizung**: aktueller Tastgrad, Außentemperatur und Kennliniengültigkeit;
- lokale Versorgungsspannung aus ADC **mit Qualitätshinweis (geschätzt/kalibriert/ungültig)**, nicht als gemessene Heizleistung;
- alle neun DS18B20-Plattentemperaturen, Min/Max/Mittelwert/Median/Spreizung und Anzahl gültiger Sensoren;
- Tanktemperatur, Raum- und Stalllufttemperatur, Außentemperatur sowie Luftfeuchte;
- Taupunkt und Feuchtkugeltemperatur einschließlich Fallbackhinweis;
- aktive Warnungen und kritische Fehler, letzte Neustartursache sowie ggf. aktive Service-/Test-Restlaufzeit.

### 14.3 Verlauf

Mindestens Raumtemperatur, Außentemperatur, Tanktemperatur und **Plattenmittelwert** sind zeitlich darzustellen. Zusätzlich sinnvoll: Plattenminimum/-maximum, PWM-Tastgrad, ADC-VIN und Pumpenzustände. Anzeigeintervalle und RAM-Speicherdauer sind separat vom detaillierten Systemidentifikationsprotokoll konfigurierbar.

### 14.4 Fehleranzeige

Jeder Fehler besitzt Code, lesbare Meldung, Warn-/Kritischklasse, Erst-/Letztauftreten, Wiederholungszahl, Aktivstatus und Sperrwirkung. Frühere Fehler können in einem Ringpuffer gespeichert werden; ein Neustart darf RAM-Verlauf löschen, sofern keine explizite Persistenz vorgesehen ist.

### 14.5 Konfiguration

Normale Benutzerparameter:

- Heizen/Kühlen/Verdunsten ein- oder ausschalten;
- Heiz- und Kühlsollwerte;
- Kühlungs- und Verdunstungshysterese, Taupunktabstand, Tank-Platten-Abstand;
- **PI-Parameter** (`Kp`, `Ti` oder `Ki`), gegebenenfalls Auswerte-/Regelintervall innerhalb validierter Bereiche;
- Anzeige- und Datenerfassungsintervalle sowie nichtkritische Diagnoseoptionen;
- Q4-Heizkennlinie als dokumentierter **Serviceparameter** mit geprüften Grenzen.

**Keine normale Benutzerfreigabe** für sicherheitskritische maximale Plattentemperatur, ungeprüfte 60-W-Stufe, Hardwaregrenzen, ADC-Schutzeinstellungen, TC4427-Kaltstart-Qualifikation oder einen unbeschränkten manuellen Dauermodus. Ein früher vorhandener Parameter `heating_hysteresis` ist für die PI-Normalregelung **nicht mehr aktiv**; gegebenenfalls wird er ausschließlich zur Fehler-Wiedereinschaltfreigabe verwendet und im UI entsprechend bezeichnet.

### 14.6 Validierung

Alle Eingaben werden typgeprüft, auf Grenzen, Abhängigkeiten und Plausibilität kontrolliert und erst dann atomar übernommen. Änderungen an PI-Parametern müssen den aktuellen Integratorzustand kontrolliert behandeln. Unzulässige Parameter werden abgewiesen, ohne den letzten gültigen Satz zu beschädigen.

### 14.7 Speicherung

Die gültige Konfiguration wird nichtflüchtig im ESP32 gespeichert, z. B. über NVS/Preferences, inklusive Versionsnummer und Migrationslogik für den Übergang von Hysterese- zu PI-Regelung. Aktive Ausgänge, manuelle Steuerbefehle oder laufende Messversuche werden **nicht als automatisch wiederherzustellende Zustände** gespeichert.

### 14.8 Manuelle Steuerung (Serviceoberfläche)

Eine gesondert markierte und gegen unbefugte Aktivierung geschützte Ansicht erlaubt die direkte Vorgabe des **Hauptheiz-PWM-Tastgrades** und die **manuelle Schaltung der Pumpen**. Eine aktive Sitzung benötigt eine Ablaufzeit und soll durch Anzeige von Steuerquelle, Sollwert, effektivem Ausgang und verbleibender Zeit klar erkennbar sein.

Der Servicebetrieb darf gewöhnliche Automatikregeln wie Heizsollwert, Mindest-/Einschaltgrenze der Platte und **Taupunktabstand** übersteuern. Er darf **nicht** die kritische Maximaltemperatur, fehlende sicherheitsrelevante Sensorwerte, die Ausgangsverriegelung, elektrische Freigabegrenzen oder die qualifizierte PWM-Obergrenze umgehen. Die Verdunstungspumpe darf nur über einen dafür elektrisch korrekt zugeordneten 12-V-Ausgang schalten.

### 14.9 Sprungantwort / Systemidentifikation

Eine Testseite bietet mindestens: Test-PWM-Tastgrad, Vorlaufdauer, Heizdauer, Nachlaufdauer, Messintervall, Start, Stop, aktuellen Testfortschritt und Export einer **CSV** mit Zeitstempel, Soll-/Ist-PWM und allen verfügbaren Sensordaten. Ein Test darf nur bei hinreichender Protokollkapazität, aktiver Sicherheitsüberwachung und eindeutiger Steuerhoheit gestartet werden. Nähere Anforderungen in Kapitel 27.

---

## 15. MQTT-Schnittstelle

### 15.1 Grundlagen und Verbindung

MQTT ist eine zusätzliche lokale Schnittstelle, **nicht Voraussetzung für Regelung oder Sicherheitsfunktion**. Nach Verbindungsabbruch setzt der ESP32 lokale Regelung, Diagnosen und Fehlerspeicherung ohne blockierende Wartezeit fort. Er versucht die Wiederverbindung zeitlich gesteuert. Ein MQTT-Last-Will setzt `kaninchenstall/klima/status/availability = offline`, nach erfolgreicher Verbindung wird `online` gesendet.

### 15.2 Konfigurationsschnittstelle

Die per MQTT verfügbaren **gewöhnlichen** Parameter entsprechen der validierten Web-Konfiguration, insbesondere Aktivierungen, Heiz-/Kühlsollwerte, PI-Parameter, Kühlhysterese und Taupunkt-/Tankabstände. Nachrichten dürfen keine unqualifizierten sicherheitskritischen Parameter verändern. Für jede erfolgreiche Änderung ist der tatsächlich wirksame, nichtflüchtig gespeicherte Wert auf einem Status-Topic zu bestätigen.

### 15.3 Status- und Messwerte

Mindestens zu veröffentlichen sind:

- aktueller Gesamtzustand, Modus, Steuerquelle und Sperrgründe;
- Hauptheiz-PWM Soll/Ist, qualifizierter Maximalwert und aktuelle PI-Abweichung;
- Pumpenstatus Kühlung/Verdunstung sowie Q4-Heiztastgrad;
- neun Plattentemperaturen, Minimum/Maximum/Mittelwert/Median/Spreizung und Sensor-Gültigkeit;
- Tank-, Innenraum-, Stall- und Außentemperaturen sowie Luftfeuchtigkeiten;
- Taupunkt, Feuchtkugeltemperatur und verwendete Ersatzwerte;
- ADC-VIN einschließlich Gültigkeit, Fehlermeldungen, WLAN-/MQTT-Status;
- Service-/Identifikationsstatus und gegebenenfalls Versuchskennung/Restlaufzeit.

### 15.4 Vorgeschlagene Topic-Struktur

```text
kaninchenstall/klima/status/availability
kaninchenstall/klima/status/state
kaninchenstall/klima/status/control_mode
kaninchenstall/klima/status/heater_pwm_duty
kaninchenstall/klima/status/heater_pwm_limit
kaninchenstall/klima/status/driver_heater_pwm
kaninchenstall/klima/status/pump/cooling
kaninchenstall/klima/status/pump/evaporation
kaninchenstall/klima/status/faults
kaninchenstall/klima/status/identification

kaninchenstall/klima/sensor/room/temperature
kaninchenstall/klima/sensor/room/humidity
kaninchenstall/klima/sensor/stall/temperature
kaninchenstall/klima/sensor/outside/temperature
kaninchenstall/klima/sensor/outside/humidity
kaninchenstall/klima/sensor/tank/temperature
kaninchenstall/klima/sensor/plate/average
kaninchenstall/klima/sensor/plate/minimum
kaninchenstall/klima/sensor/plate/maximum
kaninchenstall/klima/sensor/plate/1 ... /9
kaninchenstall/klima/sensor/vin_adc

kaninchenstall/klima/config/heating/enabled/set
kaninchenstall/klima/config/heating/target/set
kaninchenstall/klima/config/heating/pi/kp/set
kaninchenstall/klima/config/heating/pi/ti/set
kaninchenstall/klima/config/cooling/enabled/set
kaninchenstall/klima/config/cooling/target/set
kaninchenstall/klima/config/cooling/hysteresis/set
kaninchenstall/klima/config/evaporation/enabled/set
kaninchenstall/klima/config/evaporation/min_capacity/set
kaninchenstall/klima/config/dewpoint/min_distance/set
kaninchenstall/klima/config/tank_plate/min_distance/set
```

Die endgültige Topic-Liste und Retain-Strategie sind zentral zu definieren; die oben gezeigten Namen sind ein Implementierungsvorschlag und enthalten für `/1 ... /9` nur eine Abkürzung, keine wörtliche Topic-Syntax.

### 15.5 Retain-/Replayschutz für Aktoren

Statuswerte dürfen sinnvollerweise retained werden; **manuelle Aktorbefehle und Identifikationsstarts dürfen niemals als retained Steuerbefehle bei Neustart oder MQTT-Wiederverbindung ungeprüft erneut ausgeführt werden**. Serviceaktionen benötigen Autorisierung, eindeutige Sitzungskennung/Nonce und begrenzte Gültigkeitsdauer. Eine Remote-Eingabe ersetzt keine örtliche Aufsicht eines neu entwickelten Heizsystems.

---

## 16. Konfigurationsparameter und Zugriffsrechte

Jeder Parameter benötigt Typ, Einheit, Default, zulässigen Minimal-/Maximalwert, Zugriffsrolle, Persistenzregel und Validierungsfunktion. Vorläufige Startwerte dürfen nicht als qualifizierte Betriebsgrenze verstanden werden.

| Parameter | Einheit | Funktion / Startwert / Verbindlichkeit |
|---|---|---|
| `heating_enabled` | bool | PI-Heizung freigeben; Default nach qualifiziertem Aufbau festlegen |
| `heating_target` | °C | Sollwert Plattenmittelwert |
| `pi_kp_duty_per_k` | 1/K | PI-Proportionalverstärkung; **0,027** nur Entwicklungsstartwert |
| `pi_ti_seconds` | s | PI-Nachstellzeit; **4980 s** nur Entwicklungsstartwert |
| `pi_ki_duty_per_k_second` | 1/(K·s) | Abgeleitet aus Kp/Ti oder explizit, aber nicht widersprüchlich speichern |
| `pi_update_interval_ms` | ms | Reglerzyklus; vorgeschlagen 5000 |
| `pi_temperature_filter_seconds` | s | Optionaler Reglerfilter, **nicht** für Sicherheit |
| `heater_pwm_frequency_hz` | Hz | Hauptheizung vorläufig 20000, nur Service/Firmware |
| `heater_pwm_resolution_bits` | Bit | Vorläufig 10, nur Service/Firmware |
| `heater_duty_limit_qualified` | 0–1 | **Harte qualifizierte Grenze**, nicht per normalem Web/MQTT erhöhbar |
| `heating_qualified_power_class` | W nominell | 36 W initial, 60 W erst nach Freigabe; keine geregelte Watt-Stellgröße |
| `heater_min_resistance_ohm` | Ω | Konservative Sicherheitsauslegung, zu messen |
| `heating_critical_max` | °C | Kritischer Heiz-Stopp, getrennte Freigaberolle |
| `heating_critical_release_hysteresis` | K | Wiederfreigabe nach kritischer Temperatur, nicht PI-Normalhysterese |
| `heating_hysteresis` | K | **Altparameter**; wird migriert/aus Automatik entfernt |
| `cooling_enabled` | bool | Plattenkühlung erlaubt |
| `cooling_target` | °C | Zieltemperatur Kühlung |
| `cooling_hysteresis` | K | Kühlschwellen-Hysterese |
| `evaporation_enabled` | bool | Verdunstungskühlung erlaubt |
| `evaporation_min_capacity` | K | Mindestdifferenz Tank zu Feuchtkugeltemp. |
| `dewpoint_plate_min_distance` | K | Sicherheit gegen Kondensat im Automatikmodus; manuell übersteuerbar |
| `tank_plate_min_distance` | K | Kühlwirksamkeit prüfen |
| `cooling_min_outside_temperature` | °C | Default 15 °C; bei **ungültigem** Außensensor aussetzen |
| `driver_heater_enabled` | bool | Gate-Treiber-Zusatzheizung aktiviert |
| `driver_heater_pwm_hz` | Hz | Vorläufig 1 Hz; nur Service |
| `driver_heater_curve` | °C → 0–1 | Monotone Stützpunkte/Interpolation, **noch zu bestimmen** |
| `driver_heater_max_duty` | 0–1 | Thermisch qualifizierte Grenze, nur Service |
| `vin_adc_calibration` | – | ADC-Offset/Skalierung, qualifiziert |
| `vin_valid_range` | V | Zulässiger Versorgungsbereich, vor Inbetriebnahme festlegen |
| `sensor_cycle_ms` | ms | Sensorintervall, vorläufig 2000–5000 |
| `control_cycle_ms` | ms | Zustands-/Sicherheitsüberwachung, nicht mit PI-Zyklus verwechseln |
| `history_interval_ms` | ms | Allgemeine Verlaufsspeicherung |
| `identification_baseline_seconds` | s | Dauer der Mess-Vorphase ohne Hauptheizung |
| `identification_pwm_duty` | 0–1 | Fester angeforderter Testtastgrad, unter Hard-Limit |
| `identification_heat_seconds` | s | Heizphase des Sprungversuchs |
| `identification_cooldown_seconds` | s | Nachlauf-/Abkühlmessung |
| `identification_log_interval_seconds` | s | Für Messaufzeichnung, z. B. 10 s |
| `manual_control_timeout_seconds` | s | Zwingende maximale Gültigkeitsdauer der Service-Freigabe |
| `wifi_retry_interval_ms`, `mqtt_retry_interval_ms` | ms | Nicht blockierende Wiederverbindung |

**Rollen:** `Bediener` darf gewöhnliche Sollwerte ändern; `Service` darf in einem ausdrücklich bestätigten, zeitbegrenzten Modus manuell steuern oder Identifikationsläufe starten; `Entwicklung/Hardwarefreigabe` darf hardwarebezogene Grenzparameter nach Prüfung ändern. Eine Rollenprüfung allein ersetzt keine Hardwareverriegelung.

---

## 17. Prioritätsregeln

Für jede Ausgabe gilt folgende Reihenfolge, unabhängig davon, ob der Wunsch aus Automatik, Web, MQTT oder einem Identifikationstest stammt:

1. Kritische Fehler und unabhängige Hardware-Schutzmaßnahmen; Hauptheiz-PWM sofort 0.
2. Unzulässige Ausgangskombination: Hauptheizung **niemals gleichzeitig** mit irgendeiner Pumpe.
3. Plattenmaximum, ausreichend gültige Plattensensorik und Signal-Alter.
4. Qualifizierte elektrische Freigabe: Versorgung, Treiber, feste PWM-/Leistungsgrenzen, Pumpenspannung.
5. Hardwarefreigabe der Gate-Treiber-Zusatzheizung und Temperatur-/Kaltstartstrategie.
6. Auswahl der einzigen aktuellen Steuerquelle: Automatik / Manuell / Systemidentifikation.
7. Im **Automatikmodus**: Taupunkt-, Außentemperatur-, Tank-Platten- und Solltemperaturbedingungen für Kühlen/Verdunsten sowie PI-Temperaturregelung für Heizen.
8. Im **Servicemodus**: explizit erlaubte Overrides **nichtkritischer** Freigaben; Zeitlimit und Autorisierung.
9. Anzeige, Verlaufsspeicherung, Webserver, MQTT und Diagnosedaten.

Ein Override darf nur eine zuvor **klassifizierte nichtkritische** Automatikbedingung aussetzen, niemals die zentrale Sicherheits-/Ausgangsschicht. Kommunikationsfunktionen dürfen Heizungs- oder Pumpensperren weder verzögern noch automatisch aufheben.

---

## 18. Fehlerverwaltung

Es sind mindestens die Fehlerklassen `Hinweis`, `Warnung` und `Kritische Störung` zu verwenden. Fehler enthalten: eindeutigen Code, Text, Fehlerklasse, Erst-/Letztzeitpunkt beziehungsweise Uptime, Wiederholungszahl, Aktivstatus, Sperrwirkung und falls sinnvoll Quittierungsbedarf.

Zu erkennen sind mindestens:

- ein oder mehrere DS18B20-Plattensensoren ungültig, fehlerhafte ROM-Zuordnung, alle Plattensensoren ungültig;
- auffällige Differenzen/Temperaturspreizung und unplausible Messwertänderungen;
- Innenraum-/Außenluft-/Stallklimasensor oder Tanksensor ungültig;
- Taupunktgrenze unterschritten, Kühlwirkung nicht gegeben;
- kritische Plattenmaximaltemperatur, unzulässiger gleichzeitiger Pumpen- und Hauptheizwunsch;
- qualifizierte PWM-Obergrenze überschritten (Anforderung wird begrenzt; ggf. zusätzlich Warnung), fehlerhafte PI-Parameter/Integrator;
- ADC-VIN ungültig, zu alt, unter/oberhalb des gültigen Betriebsbereichs;
- Gate-Treiber- oder Q4-Treiberheizungsfreigabe unzulässig, Außenkennlinie ungültig;
- nicht definierte Q3/J2/J3-Zuordnung als Servicefehler;
- Systemidentifikation wegen Sicherheitsgrenze, Zeitüberschreitung oder Speichermangel abgebrochen;
- manuelle Freigabe abgelaufen, unberechtigter Servicebefehl, ungültige Konfiguration;
- WLAN-/MQTT-Verbindung verloren, Webserverstart fehlgeschlagen, Watchdog-Neustart.

**Unterscheidung:** Ein fehlender Stromsensor ermöglicht **keine** automatische Erkennung jedes Heizdraht- oder MOSFET-Kurzschlusses. Fehleranzeigen dürfen solche nicht messbaren Zustände nicht als überwacht darstellen.

---

## 19. Datenhaltung und Testdatenexport

- Nichtflüchtig werden mindestens validierte Konfiguration (mit Schema-Version), Sensor-ROM-Zuordnung, Neustartzähler und bei Bedarf Diagnosezähler gespeichert.
- **Nicht wiederhergestellt:** letzter aktiver Heizdraht-PWM-Tastgrad, Pumpenstatus, manuelle Sitzung, laufender Identifikationstest oder zuletzt aktiver PI-Integrator. Neustart beginnt mit gesperrten Aktoren.
- Gewöhnliche Messwertverläufe und Fehlerhistorie dürfen als begrenzte RAM-Ringpuffer geführt werden. Dokumentation nennt je Puffer Intervall und garantierte Aufbewahrungsdauer.
- Für Sprungantworten ist ein **separater begrenzter Datensatz mit Metadaten und CSV-Export** vorzusehen. Aufzuzeichnen sind Zeit/Uptime, Testphase, vorgegebener und wirksamer PWM-Tastgrad, alle neun Plattentemperaturen samt Qualitätskennzeichen, Plattenmittelwert, Minimum/Maximum, Raum-, Stall-, Außentemperatur, Luftfeuchte, Tanktemperatur, ADC-VIN samt Messgültigkeit, Pumpen- und Fehlerstatus.
- Bei WiFi-Ausfall muss die Messung **lokal weiterlaufen**, sofern Testfreigaben und Aufzeichnungskapazität gelten. Ein Rechner, Home-Assistant-Server oder MQTT-Broker darf optional Archivziel sein, ist aber keine notwendige Voraussetzung für einen laufenden Versuch.
- Der ESP32 besitzt begrenztes RAM und Flash-Schreibfestigkeit. Eine mehrstündige CSV-Rohdatenaufzeichnung im RAM ohne Speicherkapazitätsrechnung ist nicht zulässig; vor Start Speicherreservierung/Maximaldauer, Intervall und Datenstrategie prüfen. Daten können kompakt binär gespeichert und als CSV ausgegeben werden oder in validierten Portionen auf eine geeignete nichtflüchtige Ablage übertragen werden.
- Bei Überschreitung der verfügbaren Kapazität sind Datenverlust und Testabbruch eindeutig zu melden. Messdaten dürfen niemals zulasten der Sicherheitsregelung Speicher erschöpfen oder Watchdogfehler verursachen.

---

## 20. Initialisierung und Neustartverhalten

Beim Einschalten / nach Reset:

1. **Hardwareseitige Pulldowns** halten Q1–Q4 beziehungsweise die Ausgangsanforderungen bis zur gültigen Initialisierung in definiertem Zustand.
2. GPIOs und PWM-Kanäle zuerst auf AUS beziehungsweise **0 %** konfigurieren; keine gespeicherten manuellen Stellwerte übernehmen.
3. Konfiguration mit Versions-/Bereichsprüfung laden, gegebenenfalls alte Hysterese-Parameter migrieren; im Fehlerfall sichere Standardwerte wählen.
4. Sensorbusse initialisieren und ROM-IDs zuordnen; erste Messwerte erfassen, Gültigkeit und Alter beurteilen.
5. ADC-VIN auslesen, Kalibrierung und zulässigen Spannungsbereich prüfen. Der ADC allein beweist keine elektrische Strombelastbarkeit.
6. 12-/5-V-Buck-Versorgung und elektrisch konfigurierte Pumpenausgänge entsprechend dem Inbetriebnahmeprotokoll freigeben.
7. Gate-Treiber-Heizungskennlinie nur bei gültigem Außensensor und freigegebenem Q4 einlesen; bei möglichem Frost ist die **separat zu qualifizierende** Aufwärm-/Treiberfreigabe zu berücksichtigen. Ohne lokalen Temperaturfühler ist die tatsächliche IC-Temperatur nicht unmittelbar nachgewiesen.
8. PI-Reglerzustand sicher initialisieren, PWM-Grenzen berechnen und automatische Steuerquelle festlegen.
9. WLAN-Verbindung, Webserver und MQTT **nicht blockierend** starten, anschließend fortlaufende Wiederverbindung im Hintergrund.
10. Erst nach erfolgreich geprüften Freigaben den Automatikzustandsautomaten in Betrieb nehmen.

Nach Watchdog oder Brownout sind Hauptheizung und Pumpen bis zur vollständigen Neuprüfung AUS. Der letzte PI-Integrator wird nicht unkontrolliert übernommen.

---

## 21. Watchdog und Laufzeitverhalten

Ein Hardware-/Task-Watchdog ist vorzusehen. Kritische Steuerung muss fortlaufen, auch wenn WLAN, MQTT, HTTP, Datenexport oder Sensortreiber verzögert reagieren. Blockierende Endlosschleifen in Wiederverbindungen, Testtimern, DS18B20-Wartezeiten oder CSV-Ausgabe sind nicht zulässig.

- Sicherheitsprüfungen erfolgen ereignis- und zyklusbasiert unabhängig von 5-s-PI- und 20-kHz-PWM-Zeitbasis.
- Jedes aktive Messsignal erhält einen Zeitpunkt; veraltete Daten können nicht stillschweigend als aktuell übernommen werden.
- Manuelle Sitzungen besitzen lokale Ablaufzeit, unabhängig von Browser-/Brokerverfügbarkeit.
- Der Identifikationsmodus wird über monotone Zeit und definierte Phasen gesteuert, nicht über blockierende `delay()`-Sequenzen.
- Nach Watchdog-/Brownout-Neustart sind die Aktoren zunächst aus und manuelle/testweise Steuerungen beendet.

---

## 22. Serielle Diagnoseausgabe

Mindestens auszugeben sind: Uptime/Zeitbasis, Neustartgrund, aktueller Zustand/Steuerquelle, WLAN-IP/MQTT-Status, alle Sensor-/Qualitätswerte, Plattenminimum/-maximum/-mittelwert/-spreizung, Taupunkt und Feuchtkugeltemperatur, PI-Soll-/Istwert/Kp/Ti/Integrator, Haupt-PWM-Soll/Ist und Hard-Limit, Q4-Tastgrad/Kennlinie, Pumpen- und Reserveausgangszustände, ADC-VIN/Gültigkeit, aktive Fehler, Sicherheits-Sperrgründe und Zustandswechsel.

Im Identifikationsmodus sind auf Wunsch zeitstempelbasierte CSV-Datensätze mit fixem Feldschema auszugeben. Serielle Kommunikation darf die Regelung nicht blockieren. Zugangsdaten und WLAN-/MQTT-Secrets dürfen nicht vollständig ausgegeben werden.

---

## 23. Sicherheitsanforderungen

### 23.1 Sicherer Grundzustand und zentrale Verriegelung

Im sicheren Grundzustand sind **Hauptheiz-PWM=0**, Kühlpumpe AUS, Verdunstungspumpe AUS und Reserveausgänge AUS; Q4 zunächst ebenfalls AUS. Der Zustand gilt bei Initialisierung, kritischer Störung, fehlender ausreichend valider Plattensensorik oder gescheiterter Aktorfreigabe. Kein Servicebefehl darf die zentrale Verriegelung außer Kraft setzen.

### 23.2 Kritische Temperaturgrenzen

`plate_max >= heating_critical_max` bewirkt sofortige Heizungsabschaltung, unabhängig von PI, manueller Bedienung oder Identifikationsversuch. Die Freigabe für erneutes Heizen darf erst nach definierter Hysterese, gültigen Messwerten und ggf. Fehlerquittierung erfolgen. Die Temperaturüberwachung muss unabhängig von der normalen PI-Filterung funktionieren.

### 23.3 Elektrische Schutzgrenzen und fehlende Strommessung

Der ESP32 hat **keinen tatsächlichen Heizdraht-Stromsensor**. Softwareseitig sind nur qualifizierte Obergrenzen für PWM, Spannungsplausibilität und Ausgangsverriegelung möglich. Die Aussage „Überlastungsschutz des ESP32“ bezeichnet deshalb derzeit **keinen gemessenen Kurzschluss-/Überstromschutz**. Die Grenzen für Netzteil, Buck-Converter, Leiter, Kontakte und MOSFET müssen hardwareseitig abgesichert bzw. nachgewiesen werden.

### 23.4 Kritischer Fehlerfall: MOSFET dauerhaft leitend

Bei nominell 24 V / 4 Ω beträgt die theoretische **Dauerleistung 144 W**. Weder PI-Regler noch Software-Hard-Limit noch ADC können einen leitend ausgefallenen MOSFET sicher abschalten. **Eine unabhängige, auf den tatsächlichen Aufbau abgestimmte thermische Sicherheitsabschaltung ist vor unbeaufsichtigtem Tierbetrieb dringend erforderlich.** Ein geeignetes bautechnisches Schutzkonzept muss die Lage zwischen Heizdraht, Dämmung und brennbaren Materialien berücksichtigen. Diese Hardware ist im aktuellen PCB-Entwurf **nicht vorhanden**; damit ist die Sicherheitsfreigabe ausdrücklich **offen**. Ein 3-A-Netzteil stellt keine sichere Begrenzung der Heizdrahttemperatur dar.

### 23.5 Manuell/Identifikation: erlaubte und verbotene Overrides

**Erlaubt (nur mit ausdrücklicher zeitbegrenzter Freigabe):** Automatische Heizsollwerte, normale PI-Temperaturanforderung, Kühl-Einschalt-/Ausschalttemperaturen, Kühl-Außentemperatursperre, Taupunktkondensatgrenze, sonstige nichtkritische Komfort-/Wirksamkeitsregeln.

**Niemals erlaubt:** Plattenmaximum, notwendige Temperaturmessgültigkeit, Pumpe bei aktiver Hauptheizung, ungeprüfte PWM-/Leistungsgrenze, bekannte Elektrik-/Treiberfehler, unsichere Pumpenversorgung, Watchdog/Brownout oder erzwungener dauerhafter 100-%-Hauptheizbetrieb. Ein manueller Befehl kann ohne gültige Sicherheitsfreigabe wirkungslos bleiben; die UI muss den tatsächlichen Sperrgrund anzeigen.

### 23.6 Stromversorgung und EMV

Die Eingangssicherung, Verpol-/Transientenschutz, Kabel- und Leiterbahn-Auslegung sind vor Betrieb zu validieren. Der lang geführte Kabelstrom darf weder über die ESP32-Logikmasse noch über ungeeignete Steckverbinder geführt werden. Die Stützkondensatoren benötigen nachgewiesene Rippelstrom- und Einschaltstromfestigkeit; PWM-Spannungs- und MOSFET-Überschwinger sind oszilloskopisch zu kontrollieren.

### 23.7 Gate-Treiber bei Frost

TC4427**CPA** ist nicht für die geplanten negativen Außentemperaturen spezifiziert. Die lokale Q4-Widerstandsheizung ohne Rückmeldung kann eine Erwärmung bewirken, **garantiert aber keine Mindest-IC-Temperatur** und kein sicheres Startverhalten. Ein geeigneter temperaturfreigegebener Treiber (z. B. entsprechende Industrial-Variante) ist die vorzugswürdige technische Lösung. Bis zur Qualifikation ist Heizbetrieb bei möglichem Frost **nicht als freigegeben anzusehen**.

### 23.8 Dämmung, Tierschutz und Brandlast

Heu, Einstreu, EPS, Faser-/Pressspanplatten und die Lackschicht des verwendeten Stahl-Heizdrahts sind im Fehler- und Übertemperaturkonzept zu berücksichtigen. Die Plattensensoren erfassen nicht zwingend die höchste Heizdrahttemperatur. Die mechanische Abdeckung muss vor Annagen, Feuchteeintrag und Berührung von Strompfaden schützen. Messungen und Belastungsversuche erfolgen vor Tierbetrieb unter Aufsicht.

### 23.9 Netzwerk, Konfiguration und Wiederanlauf

WLAN/MQTT dürfen lokale Sicherheitsentscheidungen nicht blockieren. Ungültige Konfiguration wird abgelehnt beziehungsweise auf sichere Standardwerte zurückgesetzt. Manuelle Aktorkommandos dürfen weder retained noch nach einem Neustart erneut ohne Bedienhandlung aktiviert werden. Nach jedem Reset sind die Aktoren bis zum Abschluss der Sicherheitsprüfung aus.

---

## 24. Abnahmekriterien

### 24.1 PI-Heizregelung

- Der ESP32 regelt den **Plattenmittelwert** mit PI und erzeugt 20-kHz-Hardware-PWM; PI-Output ist PWM-Tastgrad.
- Die voreingestellten PI-Parameter sind konservativ; nach realer Systemidentifikation können Kp/Ti über validierte Parameter angepasst werden.
- Anti-Windup verhindert Integrator-Aufladung bei 0/Max-Duty und Sicherheitsabschaltung; manuelle/identifizierende Modi setzen den Integrator definiert aus.
- Das tatsächlich wirksame PWM-Signal bleibt bei jedem Reglerwunsch innerhalb der **qualifizierten** Tastgradgrenze.
- Das **höchste** gültige Plattenmesssignal löst bei kritischer Grenze eine unverzögerte Hauptheizabschaltung aus.
- Bei kompletten Plattensensorausfall oder bekannten kritischen Fehlern bleibt die Hauptheizung aus, auch im manuellen Test.

### 24.2 Kühlungsregelung

- Kühlung beginnt/endet mit der bisherigen Kühlschwellen-Hysterese und richtet sich nach dem Plattenmaximum.
- Kühlung endet, wenn Plattenminimum `<= Taupunkt + Mindestabstand` (außer ausdrücklich aktivem, zeitbegrenztem Service-Override).
- Eine gültig gemessene Außentemperatur unter 15 °C sperrt die automatische Plattenkühlung; bei Ausfall des Außensensors entfällt **nur** diese Außentemperaturprüfung.
- Die Tank-Platten-Abstandsprüfung bleibt erhalten; bei Tanksensorfehler darf die Plattenkühlung nach bisheriger Fallbackregel weiterarbeiten.

### 24.3 Verdunstungskühlung

- Aktivierung nach ausreichender Differenz Tanktemperatur/Feuchtkugeltemperatur, mit sinnvoller Schalthysterese.
- Bei ungültigem Außenluft- oder Tanksensor ist die automatische Verdunstungskühlung aus.
- Beide Pumpen dürfen nur bei nachgewiesener gemeinsamer Versorgung innerhalb der Stromgrenzen gleichzeitig aktiv sein.

### 24.4 Sensoren und Datenqualität

- Neun Plattensensoren werden über feste DS18B20-ROM-IDs identifiziert; ein zehnter DS18B20 nutzt eigenen Tankbus.
- SHT45 und zwei BME280 sind mit korrekten Adressen und **ESP32-kompatiblen 3,3-V-I²C-Pegeln** erreichbar.
- Mittelwert, Minimum, Maximum, Median, Spreizung und Sensor-Gültigkeit sind konsistent auf UART, Web und MQTT.
- Die ursprüngliche Taupunkt-Fallbackkette Innenraum → Außenluft → Ersatzwert 23 °C wird in allen Automatikmodi eingehalten.

### 24.5 Manuelle Steuerung

- Servicefreigabe startet mit explizitem Zeitlimit und klarer Anzeige; Ausgangsstatus wechselt nicht allein durch Browser-/MQTT-Verlust.
- Ein manueller Heiz-PWM-Wert darf normale Heizschwellen übersteuern, überschreitet aber **niemals** die qualifizierte PWM-Grenze oder die kritische Plattenmaximaltemperatur.
- Ein manueller Pumpenwunsch darf nicht gleichzeitig mit aktivem Hauptheizdraht wirksam sein.
- Taupunktprüfung kann für einen ausdrücklich gekennzeichneten Kühltest ignoriert werden, **andere** kritische Grenzen bleiben in Kraft.
- Ablauf/Abbruch/Reset der Sitzung hinterlassen keinen unbeabsichtigt aktiven Ausgang.

### 24.6 PWM-Sprungantwort

- Ein definierbarer Versuch mit Vorlauf, festem PWM-Stellwert, Heizdauer und Nachlauf lässt sich starten/abbrechen.
- Der vorgegebene Tastgrad ist während der Heizphase tatsächlich konstant, **solange keine Sicherheitsbegrenzung eingreift**; jeder Eingriff ist in den Daten markiert.
- Plattenmittelwert, alle neun Sensorwerte, Raum-/Stall-/Außentemperatur, Messzeiten, angeforderter und wirksamer PWM-Tastgrad sowie Sicherheitsereignisse werden erfasst und exportierbar gemacht.
- Der Versuch funktioniert bei kurzzeitigem WLAN-Verlust lokal weiter, sofern Speicher und Freigaben gültig bleiben.
- Bei kritischer Temperatur, ungültiger notwendiger Sensorik, Speichermangel oder Zeitlimit erfolgt definierter Abbruch mit PWM=0; alle Abbrüche werden protokolliert.

### 24.7 Zusatzheizung Gate-Treiber

- Q4-Pinout, J8/J9 und Widerstandsgruppen sind elektrisch korrekt; J9-3/4 führen zu 12 V.
- 1-Hz-PWM der Heizbrücke folgt einer plausibilisierten, monotonen Außenluft-Kennlinie. Bei ungültiger Außentemperatur wird der Q4-Heizausgang definiert ausgeschaltet.
- Für Zwei- und Vierwiderstands-Bestückung werden Widerstands- und Treibertemperaturen gemessen; maximale Last und Frost-/Kaltstartfunktion sind vor Freigabe geklärt.

### 24.8 24-V-Netzteil, 15-m-Erdleitung und PCB

- Der 24-V-/3-A-Netzteiltest umfasst entladene 4400-µF-Kondensatoren, 20-kHz-PWM und maximal freigegebenen Heiztastgrad; keine Hiccup-Neustarts oder unzulässigen Einbrüche.
- Falls 60 W nicht sicher getragen werden, wird ein geeigneteres Netzteil gewählt oder die zulässige Heizgrenze reduziert.
- Die 15-m-NYY-J-Leitung ist entsprechend ihrer Schutzleiterkennzeichnung korrekt angeschlossen; mechanischer/Nagerschutz separat geprüft.
- Eingangssicherung, Verpol-/Transientschutz, Kabel- und PCB-Stromtragfähigkeit sowie Buck-Anlaufströme sind geprüft.
- C5/C6-Rippelstrom und Temperaturen sind unter den relevanten PWM-Tastgraden dokumentiert; Oszilloskopkontrolle der VIN-Welligkeit und Q1-MOSFET-Spannungsüberschwinger ist erfolgt.

### 24.9 ADC und Diagnose

- ADC-VIN-Anzeige stimmt nach dokumentierter Kalibrierung mit einem Referenzmessgerät innerhalb einer festgelegten zulässigen Abweichung überein.
- Der ADC-/D4-Schutz ist anhand konkreter Bauteile und Fehlertests validiert; keine ungesicherte 2,5-V-Hartbegrenzungsbehauptung.
- Keine Stelle der Firmware behandelt die ADC-Schätzung als **Messung des realen Heizdrahtstroms** oder als alleinigen unabhängigen Überlastschutz.

### 24.10 Web, MQTT und Neustart

- Web/MQTT veröffentlichen Zustände, PWM, Sensoren, Fehler und neue Test-/Servicezustände konsistent.
- Konfiguration wird geprüft/nichtflüchtig gespeichert, Servicesitzungen werden nicht automatisch wieder aufgenommen.
- WLAN-/MQTT-Ausfall beeinträchtigt die lokale Automatik nicht; Watchdog-Reset setzt Hauptheizung und Pumpen zunächst auf AUS.

### 24.11 Thermische und mechanische Freigabe

- Messungen des fertig gedämmten Rückzugsraums bei verschiedenen Einstreumengen bestätigen, dass keine zulässigen Bauteil-, Draht-, Platten- oder Dämmmaterialtemperaturen überschritten werden.
- Eine von Software und MOSFET unabhängige thermische Schutzstrategie ist nachgewiesen, bevor unbeaufsichtigter Betrieb mit Tieren freigegeben wird.

---

## 25. Offene Implementierungsentscheidungen und Prüfpunkte

**Hardware / Sicherheit – vor Fertigung bzw. unbeaufsichtigtem Betrieb:**

1. Ersatz beziehungsweise Qualifikation des TC4427CPA für Frost; Q4-Zusatzheizung ist noch keine gesicherte Frostfreigabe; optional lokalen Gehäusesensor an GPIO32 realisieren.
2. **Unabhängiger thermischer Heizdraht-/Plattenbegrenzer** und mechanischer Brandschutz bei EPS, Heu und Holzwerkstoffen; fehlt derzeit.
3. 24-V-/3-A-Netzteil im Betrieb mit 60 W, 4400-µF-Einschaltladung, Buck-Wandlern und Kabel validieren; ggf. leistungsstärker ersetzen.
4. C5/C6 mit spezifiziertem zulässigen Rippelstrom ersetzen oder belastbar qualifizieren, ESR/Temperatur/ESL messen.
5. NYY-J-3×2,5-mm²-Verlegeart, Schutzleiterbehandlung, Sicherung, Nagerschutz und Leitungseinführung verbindlich auslegen.
6. D4-/ADC-Fehlerschutz, Footprint und Spannungsteilerkalibrierung finalisieren; Messfehlergrenzen dokumentieren.
7. Bei J9 **Pin 3 mit 12 V** verbinden; Q4 realen E/B/C-Footprint und widerstandsfeste mechanische Heizbrücke prüfen.
8. Q2-IRLZ44N bei 3,3-V-Gate unter Pumpenlast prüfen; gegebenenfalls MOSFET/Ansteuerung austauschen.
9. Zuweisung von Q3/J5 an Verdunstungspumpe, J2/J3-Spannungswahl und Eignung der D2/D3-Schutzbeschaltung festlegen; J7-Bypass sicher ausschließen.
10. Sensormodule auf 5-V-Eingangstauglichkeit/3,3-V-I²C-Pegel prüfen; BME280-Adressen 0x76/0x77 festlegen.
11. KiCad-ERC/DRC, tatsächliche Netzliste, Pin-1-Markierungen, Footprints, Leitungstrennung und EMV-Layout prüfen.
12. Physische Plattendicke, Heizdrahtwiderstand kalt/warm, Wärmeeinbettung, erlaubte Temperatur von Lack und Dämmung dokumentieren.

**Firmware / Regelung – vor Abschluss der Implementierung:**

13. PI-Regler mit PWM-Stellgröße und Anti-Windup implementieren; Startparameter im eingebauten Aufbau über Sprungantwort identifizieren.
14. Maximalen qualifizierten Tastgrad anhand zulässiger Spannungs-/Drahttoleranzen festlegen; 36-W-Start und 60-W-Stufe explizit unterscheiden.
15. Q4-Außenluft-Kennlinie (Stützpunkte, Bereiche, Max-Duty, Ausfallverhalten) nach Temperaturtests festlegen.
16. Kritische Temperaturgrenze, Freigabe-Hysterese, Mindestzahl/Verteilung gültiger Plattensensoren und Messwertalter festlegen.
17. Systemidentifikationsdatensatz, Kompressions-/Speichermodell, maximale Testdauer und CSV-Export bestimmen.
18. Rollen-/Servicezugang, Zeitlimit, Sicherheitsprüfung manueller Eingriffe und MQTT-Replay-Schutz implementieren.
19. Firmwareplattform/Webframework, vollständige Topics, Pi-Parameter-Grenzen, Server-/MQTT-Authentifizierung und WLAN-Reconnect-Strategie abschließen.
20. Thermische Messreihe mit eingebauter **40-mm-EPS-Dämmung** und variierender Einstreu durchführen; Regler unter Wetter-/Tierbedingungen in kontrollierter Freigabe testen.

**Dokumentenstand:** Das Pflichtenheft beschreibt den aktuellen Entwicklungsplan und identifiziert erforderliche Prüfungen. Eine **Fertigungs- oder Betriebsfreigabe** für unbeaufsichtigtes Heizen liegt damit ausdrücklich nicht vor.

---

## 26. Manuelle Steuerung / Servicebetrieb

### 26.1 Zweck und Abgrenzung

Die manuelle Funktion dient **Diagnose, Funktions- und Pumpentests**, nicht dem dauerhaften ungeregelten Tierbetrieb. Der ESP32 stellt eine übergeordnete Service-Steuerquelle bereit, die normale Temperatur-, Taupunkt- und Komfortbedingungen gezielt außer Kraft setzen kann. Sicherheitsgrenzen bleiben unverändert wirksam.

### 26.2 Bedienbare Ausgänge

- Hauptheizdraht: frei vorgebbarer Tastgrad `manual_heater_duty` im Bereich **0 bis qualifizierte PWM-Grenze**.
- Kühlkreispumpe: explizit AUS/EIN.
- Verdunstungspumpe: explizit AUS/EIN, sobald korrekter 12-V-Ausgang festgelegt und abgenommen ist.
- Optional Q4-Heizbrücke: nur als **separate, begrenzte Servicefunktion** mit geprüfter Widerstandsbestückung und Max-Duty, nicht als Ersatz für die Frost-Qualifikation.

### 26.3 Sicherheitsmatrix

| Bedingung | Manuell ignorierbar? | Verhalten |
|---|---|---|
| Ziel-/Starttemperatur Heizen oder Kühlen | Ja | Nur die normale Automatikbedingung entfällt |
| Normale Kühl-Hysterese | Ja | Pumpe wird über Servicewunsch gesteuert |
| Taupunktabstand / Kondensatwarnung | Ja, ausdrücklich | Der Bediener muss den Kondensat-Teststatus sehen |
| Außentemperatursperre / Tank-Platten-Wirksamkeitsabstand | Ja, für freigegebenen Pumpentest | Nicht ohne Hinweis auf fehlende Kühlwirkung |
| Heizdraht und Pumpe zugleich | **Nein** | Zweiten Ausgang sperren, Fehler melden |
| Kritisches Plattenmaximum | **Nein** | Hauptheizung sofort AUS |
| Keine ausreichend gültigen Plattensensoren | **Nein** | Hauptheizung nicht freigeben |
| Qualifizierte PWM-Obergrenze | **Nein** | Auf Maximalwert begrenzen oder Servicebefehl ablehnen |
| Kritische Versorgungs-/Treiberfehler | **Nein** | Betroffene Lasten sperren |
| Ablauf des Service-Zeitlimits | **Nein** | Manuelle Steuerquelle entziehen, Ausgänge sicher überführen |

### 26.4 Bedienablauf

1. Zugriff autorisieren und deutlich als `MANUELL` kennzeichnen.
2. Dauer und gewünschten Ausgabestatus festlegen; vor jeder Aktivierung die Sicherheitsmatrix prüfen.
3. Tatsächlich wirksame Ausgänge sowie blockierte Befehle auf der Webseite protokollieren.
4. Ausgangsbefehle sind nach Ablauf der lokalen, monotonen Frist ungültig, unabhängig von Browser oder MQTT.
5. Nach Stop/Ablauf/Fehler zunächst Hauptheizung und Pumpen AUS; automatische Regelung erst nach explizit definierter Übergabe mit neu initialisiertem PI-Zustand.

Für Messungen an einem bislang nicht thermisch freigegebenen Heizaufbau ist **örtliche Aufsicht ohne Tiere** Voraussetzung.

---

## 27. Systemidentifikation / PWM-Sprungantwort

### 27.1 Ziel

Die Sprungantwort des fertig eingebauten und gedämmten Rückzugsraums soll das Verhalten vom **elektrischen PWM-Tastgrad** zur **mittleren Plattentemperatur** identifizieren. Es ist keine Strom- oder exakte Wattmessung erforderlich. Die im realen Aufbau wirkenden Kabel-, Schalt- und Wärmeverluste werden als Teil der experimentellen Strecke mit erfasst.

Zu bestimmen sind nach dem Test insbesondere die statische Prozessverstärkung `K_proc = ΔT_mean / ΔD` (in K je normiertem Tastgrad), die thermische Zeitkonstante und eine gegebenenfalls vorhandene erkennbare Verzögerung. Ein Modell erster Ordnung mit Totzeit (FOPDT) ist Startpunkt; bei stark unterschiedlicher schneller/langsamer Erwärmung kann ein Modell höherer Ordnung erforderlich werden.

### 27.2 Konfigurierbare Phasen

Der Test besitzt mindestens:

1. **Vorlauf** `identification_baseline_seconds`: Hauptheizung AUS, Ausgangstemperaturen aufzeichnen, möglichst stationäre Umgebung.
2. **Leistungssprung** `identification_heat_seconds`: Hauptheiz-PWM wird von 0 auf den festen Wert `identification_pwm_duty` gesetzt, sofern keine kritische Begrenzung eingreift.
3. **Nachlauf** `identification_cooldown_seconds`: PWM wieder 0 und Abkühlung protokollieren.
4. **Auswertung/Export:** CSV-Ausgabe, Testprotokoll und Status `abgeschlossen` oder `abgebrochen`.

Die Dauer der Heizphase muss **nicht** bis zu einer stationären Endtemperatur reichen, wenn später ein geeigneter Datenfit eingesetzt wird. Für die direkte 63-%-Auswertung ist eine hinreichende Annäherung an den stationären Wert erforderlich. Testdauer und Ziel-PWM sind begrenzt; kritische Temperaturgrenzen bleiben aktiv.

### 27.3 Startwerte für den ersten Versuch

- **12,5 % PWM** als erster beaufsichtigter Versuch bei nominal 24 V und ca. 4 Ω; entspricht unter idealisierten Annahmen etwa **18 W**. **Nicht** 50 % PWM: dies entspräche nominal etwa 72 W.
- Erfassungsintervall zum Beispiel 10 s, frei konfigurierbar nach Speicherkapazität.
- Vorlauf zunächst mindestens so lange, dass Anfangstrend und Umgebungstemperatur sichtbar sind; Testdauer im Rahmen der thermischen Freigabe einstellen.
- Nachlauf bei ausgeschalteter Heizung erfassen; kein gleichzeitiges Betreiben der Pumpen während der Heizphase.
- Ein zweiter Test mit anderem PWM-Sprung (z. B. 12,5 % auf 25 % im weiterhin erlaubten Bereich) dient zur Prüfung von Nichtlinearität und geänderter Prozessverstärkung.

### 27.4 Aufzuzeichnende Messgrößen

**Pflicht pro Messzeitpunkt:** `elapsed_ms`/monotone Zeit, Testphase, `pwm_requested`, `pwm_actual`, alle neun Platten-DS18B20-Werte und deren Gültigkeit, `plate_mean`, `plate_min`, `plate_max`, Raumtemperatur, Außentemperatur und vorhandene Stalltemperatur. Hinzu kommen sinnvollerweise Tanktemperatur, Luftfeuchten, Taupunkt, gemessene ADC-VIN samt Qualitätsflag, Sicherheits-/Pumpen-/Treiberzustände und aktive Fehler.

**Metadaten pro Test:** Firmware-/Konfigurationsversion, Test-ID, Sensor-ID-zu-Position-Zuordnung, Heizer-Kaltwiderstand, nominelle Netzteilspannung, aktueller freigegebener PWM-Höchstwert, Dämmsituation (40-mm-EPS, Einstreumenge), Startzeit/Uptime und Testabbruchgrund.

### 27.5 Sicherheits-/Netzwerkverhalten

- Der Versuch darf ausschließlich innerhalb der freigegebenen PWM- und Temperaturgrenzen laufen; bei kritischer Störung PWM sofort 0 und Test abbrechen.
- Die Aufzeichnung wird über einen nicht blockierenden Scheduler geführt und darf den ESP32-Watchdog nicht verhungern lassen.
- Bei WLAN/MQTT-Ausfall läuft ein bereits gestarteter autonomer Test **nur innerhalb seiner lokalen Zeit-/Sicherheits- und Speicherkapazitätsgrenzen** weiter. Er wartet nicht blockierend auf externe Aufzeichnung.
- Bei Speichererschöpfung oder ungültiger notwendiger Plattentemperatur wird der Test mit Fehlerstatus beendet, keine stille Teilaufzeichnung als vollständiges Ergebnis ausgeben.
- Tests werden zunächst **ohne Tiere** und unter Aufsicht durchgeführt.

### 27.6 Übertragbarkeit auf spätere Störgrößen

Eine Messung im fertig gedämmten Raum ohne Tiere liefert die **nominale Regelstrecke**, nicht die vollständige Beschreibung aller späteren Zustände. Kaninchen können anfangs Wärme aufnehmen, später Wärme einbringen und die Plattenoberfläche isolieren; Heu/Einstreu ändern örtliche und mittlere Wärmeübergänge; Wetter und Wind verändern Randbedingungen.

Deshalb sind nach der Grundidentifikation robuste PI-Parameter zu wählen und bei unterschiedlich dichter Einstreu sowie verschiedenen Außentemperaturen kontrollierte Vergleichsmessungen vorzusehen. Bei Störungen kann ein PI-Regler die **mittlere** Temperatur ausregeln, **nicht aber** ein lokales Heizdraht-Hotspot-Problem sicher erkennen.

---

## 28. Versionshistorie

| Version | Datum | Änderungen |
|---|---|---|
| 1.0 | Juli 2026 | Erstfassung der Projektbeschreibung und der Grundregeln. |
| 1.1 | 17. Juli 2026 | WLAN, Webserver, MQTT, Sensorfehler-/Taupunktfallbacks, Fehlermeldungen und Kühl-/Verdunstungslogik. |
| 1.2 | 10. November 2026 | 24-V-Versorgungsarchitektur, Heizdraht-PWM mit spannungsabhängiger Watt-Begrenzungsabschätzung, ADC, Gate-Treiber und Pufferkondensatoren; Kühl-Außentemperatursperre bei Sensorausfall ausgesetzt. |
| **1.3** | **20. November 2026** | **15-m-NYY-J-3×2,5-mm²-Kabel**, qualifikationspflichtiges 24-V-/3-A-Netzteil und 4400-µF-Inrush-Test, 40-mm-EPS-Dämmung, konsolidierte Bauteilliste v0.7, konkrete ADC-Diagnose, 20-kHz-Heiz-PWM mit PI-Regler in Tastgrad-Einheiten, manueller Service mit kritischen Sperren, Sprungantwort/CSV-Aufzeichnung, Q4-Gate-Treiber-Zusatzheizung mit Außenluftkennlinie, überarbeitete Sicherheits- und Abnahmekriterien. |

