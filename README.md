# Hasenklima

ESP32-Firmware für die Temperaturregelung eines Rückzugsraums. Die Steuerung nutzt den Median der aktivierten Plattensensoren für den PI-Regler und überwacht deren Einzelwerte auf Übertemperatur. Die Firmware steuert den Heizdraht, zwei Pumpen und die separate Zusatzheizung am Gate-Treiber. Sie stellt Messwerte und Bedienung lokal bereit und kann einen PWM-Sprungversuch mit CSV-Export aufzeichnen.

Die Auslieferungskonfiguration ist ein Diagnosebuild. Solange `MAX_PLATE_TEMP_C` fehlt, bleiben Hauptheizung und beide Pumpen in allen Betriebsarten gesperrt. Die Software dokumentiert Pumpenausgänge und den programmierten PWM-Tastgrad. Heizstrom, Heizleistung und Heizenergie sind Modellwerte aus VIN, Heizdrahtwiderstand und Tastgrad.

## Build und Diagnose

```powershell
pio pkg install -e klima_esp32
pio run -e klima_esp32
pio test -e native
# Optionaler Test der eingebetteten Weboberfläche mit simuliertem Browser und API
node test/browser_smoke.cjs
pio device monitor -b 115200
```

In VS Code ist die PlatformIO-Umgebung `klima_esp32` voreingestellt. Falls `pio` außerhalb von VS Code fehlt, die PlatformIO-Konsole öffnen oder in PowerShell `& "$env:USERPROFILE/.platformio/penv/Scripts/pio.exe"` verwenden. Der native Build benötigt GCC oder einen kompatiblen C++17-Compiler im PATH. Das Firmwareimage liegt nach dem Build unter `.pio/build/klima_esp32/firmware.bin`.

Die Firmware verwendet PlatformIO Espressif 32 **6.13.0**, Arduino-ESP32 **2.0.17**, OneWire **2.3.8** und die gebündelten Komponenten von ESP-IDF **4.4.7**. SHT45 und BME280 verwenden eigene I²C-Adapter mit endlichen Timeouts. Die Feuchtkugelberechnung basiert auf PsychroLib **2.5.0** ([MIT-Lizenz](docs/PsychroLib-LICENSE.txt)). Die Seiten nutzen eingebettetes HTML, CSS und JavaScript.

Beim Entwickeln zeigt die serielle Konsole alle zehn Sekunden Messwerte, Betriebsart, PWM, Sperren und Fehler. `status` fordert die Ausgabe sofort an. `csv` überträgt den letzten gespeicherten Test. `csvlive` gibt laufende Testmesspunkte mit demselben Spaltenschema aus. `stopcsv` beendet die serielle CSV-Ausgabe. Befehle mit Zeilenende senden. `/debug` zeigt GPIOs, feste Plattengrenze, Firmware und Core, Flash, Partitionen, Heap, Messalter, maximale Laufzeiten und Stackreserven.

## Erste Einrichtung

1. **Board bestätigen.** ESP32-Modulaufdruck, externe Antenne, Flashgröße und reale Verdrahtung mit PCB v0.7 abgleichen. `esp32dev` verwendet für den Entwicklungsbuild die 4-MiB-Standardpartitionierung. Die endgültige Tabelle hängt von der tatsächlichen Flashgröße und der OTA-Entscheidung ab.
2. **Lokalen Zugang vorbereiten.** `include/LocalSecrets.example.h` nach `include/LocalSecrets.h` kopieren. SSID, WLAN-Passwort, statische IPv4-Adresse, Maske, Gateway, DNS und ein eigenes Gerätepasswort mit mindestens zwölf Zeichen eintragen. Die Firmware importiert die Daten beim ersten Boot ohne gespeicherten Konfigurationssatz. Header und daraus erzeugtes Firmwareimage privat halten. Es gibt keinen offenen Access Point. Spätere Netzwerkänderungen sind im Setup nach Anmeldung möglich. Nach einem IP-Wechsel mit der neuen Adresse verbinden.
3. **Bei ausgeschalteten Hauptfunktionen Sensoren zuordnen.** In `/setup` anmelden und den Scan starten. Für P1–P9 die physisch überprüften ROM-IDs der Plattenpositionen eintragen und die gewünschten Positionen aktivieren. Die Webansicht zeigt die Platte von oben, Eingang unten. Den Tanksensor seiner eigenen ROM-ID zuordnen. Temperatur, Alter und Sensorfehler im Grid und unter `/debug` prüfen.
4. **VIN und Versorgung einrichten.** Den ADC-Wert mit einer Referenzmessung an der Platine vergleichen, Gain und Offset bestimmen, kalibrierten Wert bestätigen und qualifizierte Min-/Max-Grenzen eintragen. Heizdrahtwiderstand messen, Netzteilnennstrom bestätigen, mittleres Heizstromlimit und Reserve festlegen.
5. **Grenzen und Regelung freigeben.** Nach thermischer Qualifikation `KLIMA_MAX_PLATE_TEMP_C` als Compilerdefinition oder `MAX_PLATE_TEMP_C` als feste Codekonstante in `include/HardwareLimits.h` setzen und neu bauen. Heiz- und Kühlsollwerte, Regelparameter und Q4-Kennlinie im Setup eintragen. Die Funktionstaster schalten Heizung, Plattenkühlung und Verdunstung separat frei. Q4 hat eine eigene Freigabe. Die PI-Startwerte sind Entwicklungswerte, die endgültige Einstellung folgt aus Messreihen am Aufbau.
6. **Optionale Schnittstellen prüfen.** MQTT ist standardmäßig deaktiviert. Im Setup Host, Zugangsdaten, Client-ID oder Gerätekennung, Präfix und Intervall setzen und MQTT bewusst einschalten. NTP ist optional. HTTP und MQTT in dieser Firmware verwenden TCP ohne Transportverschlüsselung und sind für ein vertrauenswürdiges lokales Netz gedacht. Der CSV-Transfer verwendet zusätzlich Port 81.

### LittleFS

LittleFS wird beim Boot ohne automatische Formatierung eingehängt. Bei einem Mountfehler bleiben Diagnose und lokale Regelung verfügbar, die Testfunktion meldet den Speicherfehler. Zur erstmaligen Einrichtung auf bestätigter Hardware kann mit `pio run -e klima_esp32 -t buildfs` ein Anfangsimage erstellt werden. Dessen bewusste Übertragung per `uploadfs` erfolgt als separate Wartungsaktion bei ausgeschalteten Hauptaktoren. Ein Dateisystemupload ersetzt vorhandene Testdaten. Vorher exportieren, falls ein Versuch vorliegt.

## Betrieb und Abnahme

- `/` zeigt Zustand, 3×3-Sensorgrid, Heiz- und Pumpenstatus, Klima und Verlauf.
- `/setup` verwaltet Freigaben, Sensoren, Regler, Strommodell, Q4, Manuellbetrieb, Sprungantwort, Energie, Fehler, WLAN und MQTT. **Hauptfunktionen AUS** beendet Service und Versuch und entzieht die drei Freigaben. Ein einzelner Teststop übergibt anschließend wieder mit Nullstart an die Automatik.
- `/debug` enthält nur lesende Diagnose.
- Der Installationszähler erfasst ausschließlich geschätzte Heizdrahtenergie. Er wird bei geänderter Energie periodisch und beim Heizende gespeichert. Die RAM-Historien beginnen bei jedem Neustart neu. Die Sprungantwort bleibt in LittleFS.

Die vollständigen Einstellungen stehen in [Inbetriebnahmeparameter](docs/Inbetriebnahmeparameter.md). Die Zuordnung zu A01–A32, Buildnachweise und ausstehenden Hardwaretests stehen im [Implementierungsstatus](docs/Implementierungsstatus.md).
