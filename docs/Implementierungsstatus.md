# Implementierungsstatus

Der bisherige DS18B20-Test ist durch die integrierte Firmware ersetzt. Maßgeblich sind die beiden vorhandenen versionierten Originaldateien, ergänzt um das PCB-Pinout v0.7. Der Softwareentwurf hat bei seinen ausdrücklich beschriebenen Änderungen Vorrang.

## Implementierter Umfang

- Feste Boardbelegung, sichere Ausgangsinitialisierung, LEDC für Q1 und Zeitproportionierung für Q4. Sämtliche Aktorzugriffe laufen über `ActuatorManager`. Abschaltung der Heiz-PWM über LEDC-Idle-LOW mit Trennintervall vor dem Pumpenstart.
- Getrennte 1-Wire-Busse, asynchrone Konvertierung, ROM-Zuordnung und CRC, SHT45 mit Kanal-CRC sowie BME280-Festadressierung und Kompensation. ADC1 liest millivoltkalibrierte Werte mit Geräte-Gain/Offset und Gültigkeitsprüfung.
- Vollständige aktivierte Messmenge für Median, Mittel, Minimum, Maximum und Spreizung. Einzelwerte aus laufenden Sweeps greifen früher in die Sicherheitsbewertung ein. Latches, bedingte Quittierung, PI und Nullstart, Kühl- und Verdunstungshysterese, Taupunktfallback und Q4.
- Zeitbegrenzter Manuellbetrieb mit expliziter Kondensatoption sowie Sprungantwort mit Vor-, Heiz- und Nachlauf. Starttransaktion, Schreibpuffer, Phasenmarker, CRC, Erhalt des Präfixes, Wiederherstellung nach Reset und CSV über Web oder serielle Konsole.
- Getrennte Steuerungs-, Sensor-, Speicher- und Netzwerktasks, monotone Uhr, Task-Watchdog für Steuerung/Sensorik/Speicher. Feste Befehls- und Recorderbudgets. Stop und Funktions-AUS vor dem Speicherpfad.
- Versionierte NVS-Slots mit CRC für Gerätekonfiguration und Energie. Historie und Fehlerhistorie ausschließlich im RAM. MQTT 3.1.1 mit Last Will, Retain und konfigurierbaren Intervallen, standardmäßig AUS.
- Lokal eingebettete Info-, Setup- und Debugseiten, authentifizierte Schreibzugriffe, zentrale Parameter- und Befehlsprüfung, Revisionsprüfung, wiederholbare Auftrags-IDs, zeitlich begrenzte Sitzung und eigener HTTP-Prozess für CSV.

## Technische Entscheidungen

- **Feste Plattengrenze:** UNSET bleibt im Debug sichtbar und sperrt alle Hauptaktoren. Keine Methode zum Ändern über HTTP oder MQTT. Heizstromgrenze und geschätzte Leistung folgen dem neueren VIN-Modell. Die Drehzahl, Wasserförderung, Treibertemperatur und elektrische Abschaltfunktion des MOSFET sind ohne Rückmeldung unbekannt.
- **Sensoren:** Unzugeordnete Plätze bleiben inaktiv, bis der Bediener eine gültige ROM zuordnet und sie aktiv schaltet. Es gibt keine Softwaremindestzahl über die erforderliche Anzahl eins hinaus. Firmware prüft gültige vollständige Messsätze und liefert bei deaktivierten Plätzen weiterhin verfügbare Einzeldiagnose.
- **VIN:** Faktor 16/Offset 0 liefern nominale Diagnosedaten. `vin_calibration_confirmed` ist eine explizite Bestätigung der gelösten Inbetriebnahmebedingung. Die Bestätigung und ein gültiger Spannungsbereich sind Voraussetzung für die Versorgungseinrichtung.
- **Treiber:** 0 °C ist die Untergrenze der vorgesehenen kommerziellen TC4427CPA-Variante. Unterhalb davon bleibt der Heizdraht gesperrt. Q4 besitzt seine eigene Freigabe und Temperaturkennlinie. Das Aufwärmzeitfenster ist keine Temperaturmessung.
- **Aufnahme:** Die freigegebene LittleFS-Datei wird über einen CRC-gesicherten Selektor referenziert. Der Kandidat wird zuerst synchronisiert, dann wird der geschlossene Selektor atomar ersetzt. Bis dahin bleibt der alte Test verfügbar. LittleFS wird beim Mountfehler nicht formatiert.
- **PWM:** Core 2.0.17 behandelt das oberste Arduino-LEDC-Argument als volle Aussteuerung. Die Teilwerte enden daher bei 1022/1024. Der Adapter verwendet die geprüften darunterliegenden LEDC-Aufrufe für Ausgabe und Abschaltung. Per Software geprüfte Quantisierung ersetzt die Oszilloskopmessung nicht.
- **Konfiguration:** Alle dauerhaften Änderungen an der aktiven Regelkonfiguration verwenden einen sicheren Übergang. Ein fehlgeschlagener Schreibvorgang belässt den älteren dauerhaften Satz und sperrt die Hauptaktoren bis zu erfolgreicher Speicherung und zulässiger Quittierung. Eine vorab ausgeschaltete Funktion bleibt während der Laufzeit AUS.
- **Plattform:** Entwicklungsprofil mit Standardpartitionen für 4 MiB. App-Slot 1.310.720 Byte, LittleFS-Partition 1.441.792 Byte. Endgültige Partitionen und eine eventuelle OTA-Funktion sind offen. LittleFS nutzt das Arduino-Standardlabel `spiffs` der Partition.

## Verifikation nach Softwareentwurf §20

| ID | Nachweis im Projekt | Am Aufbau offen |
|---|---|---|
| A01 | Startpfad setzt Q1–Q4 LOW; flüchtige Sitzungen werden neu initialisiert; Recorder rekonstruiert beendeten Versuch | Warmstart, Watchdog und Brownout mit Ausgangsaufzeichnung |
| A02 | Feste Grenze außerhalb des Registers; Registry- und Schreibpfadprüfung | API-Schreibversuch am Gerät und Debugvergleich |
| A03–A08 | Native Prüfung von Teilmesswertfehler, Quittierung, bewusstem Disable, leerer Menge und hohem Einzelwert | Abziehen/CRC provozieren, ROM-Wechsel, persistierter Enable-Zustand, Abschaltzeit |
| A09–A12 | Native Median-, Strom- und Quantisierungsprüfung; zentraler Ausgangspfad statisch geprüft | PWM-Frequenz, Extremwerte, Verriegelung und LOW-Pegel messen |
| A13–A15 | Native Freigabe-, PI- und Anti-Windup-Prüfungen; Stop und Nullstart im Laufzeitpfad geprüft | Test mit realer Last sowie Funktionstaster/Verbindungsabbruch |
| A16–A18 | Native Hysterese, Grenzgleichheiten, Feuchte-/Tankfallbacks, Q4 und Vorwärmung | Kondensat, Tankwirksamkeit, Q4-PWM und Kaltstart |
| A19–A22 | Native 0/1/500-min-Grenzen, 620-min-Uhr, 3721 Punkte, Phasen, Sensor- und VIN-Abbruch | Realer Langzeittest, belegter LittleFS-Platz, Sensorfehler zwischen zwei Messzeitpunkten |
| A23 | Recorder und lokale Fristen ohne WLAN-/MQTT-Abhängigkeit im Code | Funk und Broker während Versuch trennen |
| A24 | Native CRC-/Präfix- und Resetmetadaten-Prüfungen | Stromunterbrechung beim Header, Phasen-Flush, Datensatz und Testabschluss |
| A25–A26 | Startvalidierung, getrennte Transferlease und atomarer Selektor im Code abgeglichen | Parallel-Download, ungültiger und gültiger Folgestart im tatsächlichen Flash |
| A27 | Getrennte CSV- und Bedieninstanzen; kein gemeinsamer Lock bei Netz/I/O | Langsamer Download und paralleler Stop mit Zeit-/Ausgangsmessung |
| A28 | Gebundenes ESP-MQTT und deaktivierte Auslieferungskonfiguration; Publish-Topics geprüft | Broker, Last Will, Reconnect und retained Fehlerlöschung |
| A29 | Native Ringpuffer-, Raster- und Lückenprüfung; Uptime statt UTC im Steuerpfad | 24-h-Betrieb, NTP-Sprung und Neustart |
| A30 | Native Codec-/CRC-Prüfung; kompletter Satz pro NVS-Slot | NVS-Stromunterbrechung und Auswahl des vollständigen Slots |
| A31 | Native Energieintegration für konstante/wechselnde Leistung und Messlücke; Checkpoint-Codec | Zuordnung zur realen PWM, Weiterführung nach Neustart, Verlustfenster |
| A32 | Laufzeit-, Stack-, Heap- und Queue-Diagnose eingebaut | Gleichzeitig Web, MQTT und Recorder belasten, reale Latenz und Reserven messen |

## Inbetriebnahme und offene Punkte

- Zahlenwerte für Plattenobergrenze, Heiz-/Kühlziele, Heizdrahtwiderstand, VIN-Kalibrierung/-Grenzen, definitive PI-Werte, Q4-Parameter, WLAN/MQTT-Zugang, Gerätemodell und Flash/OTA sind in [Inbetriebnahmeparameter](Inbetriebnahmeparameter.md) zusammengefasst.
- Hardwarefreigaben aus Pflichtenheft §§23–25 und PCB v0.7 bleiben offen. Netzteil mit 4400 µF und PWM-Last, Erdkabel/Sicherung, Rippel, Layout, Schutzbeschaltung, Pumpenversorgung, ADC/D4, Footprints, I²C-Pegel, Q4-Kaltstart und thermische Praxisversuche mit Einstreu sind am Aufbau zu prüfen.
- Die Software kann einen dauerhaft leitenden MOSFET, lokale Hotspots zwischen Sensoren oder eine falsche 12-V-Jumperstellung nicht erkennen. Die Schutzeigenschaften des Aufbaus für unbeaufsichtigten Tierbetrieb sind gesondert nachzuweisen.
- Flashoperationen können beide ESP32-Cores und damit die CPU-gesteuerte Abschaltung verzögern. Die Hardware-PWM läuft währenddessen mit dem zuletzt gesetzten Tastgrad weiter. Festgelegte Reaktionszeit sowie Stack und Heap unter Last bleiben Hardware-Abnahmekriterien.
- Die Implementierung kennt keine frühere persistente Firmwareversion in diesem Projekt. Unbekannte oder beschädigte Schemata werden abgelehnt. Bei anfänglichem LittleFS-Mountfehler ist eine ausdrückliche Dateisystemeinrichtung erforderlich.

## Build- und Testprotokoll

| Prüfung | Ergebnis |
|---|---|
| `pio run -e klima_esp32` | Erfolgreich mit PlatformIO Espressif 32 6.13.0, Arduino-ESP32 2.0.17 und OneWire 2.3.8. |
| Flash für die Anwendung | 1.168.829 von 1.310.720 Byte (89,2 % des App-Slots). |
| Statischer RAM laut Build | 119.816 von 327.680 Byte (36,6 %). Der Laufzeitbedarf von Tasks und Netzwerk wird am Gerät gemessen. |
| `pio run -e klima_esp32 -t buildfs` | Erfolgreich, initiales LittleFS-Image mit 1.441.792 Byte. |
| `pio test -e native` | Drei Gruppen erfolgreich, 7 gemeldete PlatformIO-Testfälle. Die Regelungsgruppe prüft darin 1.433 Einzelbedingungen. Der Speichertest simuliert 620 min und genau 3.721 Datensätze. Fünf Einzeltests prüfen den JSON-Writer. |
| `node test/browser_smoke.cjs` | Alle drei Seiten mit simuliertem DOM/API ausgeführt. Sensorgrid, Schalter, CSV nach Abbruch und bestätigte Sitzungsverlängerung geprüft. Die Syntax des eingebetteten JavaScripts wurde separat geprüft. |
| Ausgabepfad | Schreibzugriffe für Aktor-GPIOs und LEDC per Quelltextsuche geprüft. Sie liegen ausschließlich in `src/hardware/ActuatorManager.cpp`. |
| Reale Hardware | Firmware und LittleFS-Image wurden lokal gebaut. Es fand kein Upload und kein Aktor- oder Hardwaretest statt. |

Der vollständige Neuaufbau meldet zwei Präprozessorwarnungen in der gepinnten OneWire-3rd-Party-Quelle. Kompilierung und Linken sind erfolgreich. Der Testplan und die fünf verbleibenden Hardwarebereiche (Versorgung, Thermik, Sensorbusse, nichtflüchtige Speicherung und Zeitverhalten unter Last) gelten für die Inbetriebnahme.
