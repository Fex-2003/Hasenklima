# Implementierungsplan

Grundlage sind `Pflichtenheft_Klimaregelung_v1.3.md`, `Softwareentwurf_Klimaregelung_v1.0.md` und für die Verdrahtung PCB-Pinout v0.7. Die im Auftrag verkürzt genannten Dateinamen entsprechen diesen versionierten Originalen.

| Reihenfolge | Baustein | Abhängigkeiten und Nachweis |
|---|---|---|
| 1 | PlatformIO, Pinbelegung, feste Grenzen, ActuatorManager | Arduino 2.0.17. LOW beim Start, LEDC v2, zuerst AUS dann EIN |
| 2 | Busadapter und Kalibrierung | Getrennte 1-Wire-Busse, I²C-Timeout, ADC1. Monotone Erfassung mit Einzelwertqualität |
| 3 | Konfiguration, Messverarbeitung, Safety und Regelung | Hardwareunabhängige Prüfung von Median, Abschaltung, PI, elektrischen Grenzen, Pumpen und Q4 |
| 4 | NVS und LittleFS, Identifikation, Energie und Historien | Getrennte Speichertask, begrenzte Transportpuffer, Präfixprüfung, simulierter 620-Minuten-Test |
| 5 | Task-Verknüpfung und Befehlsdienst | Priorisiertes AUS, Transaktionen, Fristen, Rückmeldungen, Überwachung |
| 6 | WLAN, Web und MQTT | Konsistente Snapshots, lokale Authentifizierung, isolierter CSV-Transfer, keine MQTT-Aktorbefehle |
| 7 | Integration und Abnahme | PlatformIO-Builds, native Tests, Speicherbericht, statische Prüfung aller Ausgangspfade, Abgleich A01–A32 |

Tests an realen Ausgängen, Sensoren, Versorgung, Netzwerk und Flash erfolgen erst bei der Inbetriebnahme. Die Firmware startet als Diagnosebuild, solange die feste Plattengrenze fehlt.
