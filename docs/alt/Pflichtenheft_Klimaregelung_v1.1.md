# Pflichtenheft

## Klimaregelung für einen temperierten Rückzugsraum in einem Kaninchenstall

| Dokumenteigenschaft    | Wert                                                  |
| ---------------------- | ----------------------------------------------------- |
| **Projektbezeichnung** | Klimaregelung Kaninchenstall                          |
| **Systemkomponente**   | ESP32-basierte Heiz-, Kühl- und Überwachungssteuerung |
| **Dokumenttyp**        | Softwareorientiertes Pflichtenheft                    |
| **Dokumentstatus**     | Entwurf zur Softwareerstellung                        |
| **Version**            | 1.1                                                   |
| **Stand**              | 17. Juli 2026                                         |

---

## 1. Zielsetzung

Ziel des Projekts ist die Entwicklung einer autonomen Klimaregelung für einen kleinen, frei zugänglichen Rückzugsraum innerhalb eines Kaninchenstalls.

Das System soll:

1. im Winter einen Frostschutz bereitstellen,
2. im Sommer den Raum nach Möglichkeit in Richtung einer konfigurierbaren Zieltemperatur kühlen,
3. Kondenswasser an der gekühlten Bodenplatte vermeiden,
4. Messwerte und Betriebszustände lokal über eine Website darstellen,
5. Konfigurationswerte über Website und MQTT bereitstellen,
6. bei WLAN-, MQTT- oder Sensorfehlern in definierten und sicheren Betriebsarten weiterarbeiten.

Die Regelung wird auf einem ESP32 umgesetzt und als Zustandsautomat ausgeführt.

Heiz- und Kühlbetrieb schließen sich gegenseitig aus. Es darf keinen Betriebszustand geben, in dem der Heizdraht und eine der beiden Pumpen gleichzeitig aktiv sind.

---

## 2. Systemübersicht

Das Gesamtsystem besteht aus folgenden Komponenten:

- ESP32 als zentrale Steuerung
- 12-V-Netzteil mit 48 W Nennleistung
- 12-V-auf-5-V-Buck-Converter
- Heizdraht in der Bodenplatte
- Kühlwasserschleife in der Bodenplatte
- Kühlkreispumpe
- Verdunstungspumpe
- neun Temperatursensoren in der Bodenplatte
- Innenraumsensor für Temperatur und relative Luftfeuchtigkeit
- Außenluftsensor für Temperatur und relative Luftfeuchtigkeit
- Temperatursensor im Wassertank
- MOSFET-Leistungsschalter für die 12-V-Verbraucher
- WLAN-Verbindung in ein lokales Netzwerk
- lokaler Webserver mit statischer IP-Adresse
- MQTT-Schnittstelle

---

## 3. Einsatzumgebung

### 3.1 Rückzugsraum

Der temperierte Rückzugsraum besitzt ungefähr folgende Abmessungen:

- Breite: 40 cm
- Tiefe: 40 cm
- Höhe: 40 cm

Der Raum ist nicht luftdicht abgeschlossen. Er besitzt:

- einen dauerhaft offenen Zugang zum übrigen Stall,
- eine Lüftungsöffnung in der Decke,
- Wände aus Pressspanplatten,
- zunächst keine zusätzliche Wärmedämmung.

Eine spätere Dämmung der Wände ist möglich.

Der Raum kann von den Tieren jederzeit betreten und verlassen werden.

Im Winter soll ein Wassernapf auf oder nahe der Bodenplatte abgestellt werden können. Die Platte kann teilweise mit Heu bedeckt sein.

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

Als Steuerung wird ein ESP32 eingesetzt.

Der ESP32 übernimmt:

- Sensorerfassung,
- Plausibilitätsprüfung,
- Taupunktberechnung,
- Feuchtkugeltemperaturberechnung,
- Zustandsverwaltung,
- Heizungsregelung,
- Kühlungsregelung,
- Verdunstungskühlungsregelung,
- Aktorsteuerung,
- Fehlerverwaltung,
- Datenaufzeichnung,
- Webserver,
- WLAN-Verbindungsmanagement,
- MQTT-Kommunikation.

### 4.2 Spannungsversorgung

Nennwerte:

- Versorgungsspannung: 12 V DC
- Nennleistung: 48 W
- maximaler Nennstrom: 4 A

Der ESP32 wird über einen 12-V-auf-5-V-Buck-Converter versorgt.

### 4.3 Heizdraht

Der Heizdraht besitzt einen Nennwiderstand von ungefähr 4 Ω.

Bei 12 V gilt näherungsweise:

```text
I = U / R = 12 V / 4 Ω = 3 A
P = U² / R = 144 V² / 4 Ω = 36 W
```

### 4.4 Pumpen

Es werden zwei 12-V-Pumpen verwendet:

1. **Kühlkreispumpe**
   
   - fördert Wasser aus dem Tank durch die Kühlwasserschleife der Bodenplatte.

2. **Verdunstungspumpe**
   
   - fördert Wasser aus dem Tank über die Moosfläche zur Verdunstungskühlung.

Beide Pumpen dürfen im Kühlbetrieb unabhängig voneinander oder gleichzeitig aktiv sein.

Keine Pumpe darf gleichzeitig mit dem Heizdraht aktiv sein.

### 4.5 Bodenplatte

Die Bodenplatte besitzt ungefähr folgende Abmessungen:

- Länge: 35 cm
- Breite: 35 cm

Sie enthält:

- eine Kupferrohrschleife,
- einen Heizdraht,
- neun Temperatursensoren in einer 3×3-Matrix.

Die Oberseite ist mit Fliesen versehen.

### 4.6 Sensoren

Folgende Sensoren sind vorgesehen:

- neun Temperatursensoren in der Bodenplatte,
- ein Innenraumsensor für Temperatur und relative Luftfeuchtigkeit,
- ein Außenluftsensor für Temperatur und relative Luftfeuchtigkeit,
- ein Temperatursensor im Wassertank.

Die Plattensensoren sind über ihre eindeutigen Sensoradressen zu identifizieren. Die logische Zuordnung darf nicht von der Erkennungsreihenfolge abhängen.

---

## 5. Softwarearchitektur

Die Software ist modular aufzubauen.

Empfohlene Module:

- `ConfigurationManager`
- `SensorManager`
- `PlateSensorManager`
- `ClimateSensorManager`
- `TankSensorManager`
- `DewPointCalculator`
- `WetBulbCalculator`
- `StateMachine`
- `HeatingController`
- `CoolingController`
- `EvaporationController`
- `ActuatorManager`
- `SafetyManager`
- `FaultManager`
- `HistoryManager`
- `WebServerManager`
- `WiFiManager`
- `MqttManager`
- `Diagnostics`

Die Software soll ohne blockierende Wartezeiten arbeiten. Zeitabläufe sind über nicht blockierende Zeitstempel oder Timer umzusetzen.

---

## 6. Betriebszustände

Mindestens folgende Zustände sind vorzusehen:

1. `INITIALISIERUNG`
2. `NEUTRAL`
3. `HEIZEN`
4. `KUEHLEN`
5. `VERDUNSTUNGSKUEHLUNG`
6. `KUEHLEN_MIT_VERDUNSTUNG`
7. `STOERUNG`

### 6.1 Sicherer Grundzustand

Der sichere Grundzustand lautet:

- Heizdraht aus,
- Kühlkreispumpe aus,
- Verdunstungspumpe aus.

### 6.2 Ausgangsverriegelung

Die Ausgangslogik muss unabhängig von der Zustandslogik verhindern, dass folgende Kombinationen auftreten:

- Heizdraht und Kühlkreispumpe gleichzeitig aktiv,
- Heizdraht und Verdunstungspumpe gleichzeitig aktiv.

Vor dem Setzen neuer Ausgangszustände sind alle Aktoren zunächst auszuschalten.

---

## 7. Verarbeitung der Plattentemperaturen

Aus allen gültigen Plattensensoren sind zyklisch mindestens folgende Werte zu berechnen:

- niedrigste Plattentemperatur,
- höchste Plattentemperatur,
- mittlere Plattentemperatur,
- Median der Plattentemperaturen,
- Temperaturspreizung.

Formel:

```text
Temperaturspreizung = höchste Plattentemperatur - niedrigste Plattentemperatur
```

Verwendung:

| Kennwert         | Verwendung                                      |
| ---------------- | ----------------------------------------------- |
| Niedrigster Wert | Heizbeginn, Heizende, Taupunktschutz            |
| Höchster Wert    | Kühlbeginn, Kühlende, kritische Heizabschaltung |
| Mittelwert       | Verlauf, Webanzeige, MQTT                       |
| Median           | Diagnose und Plausibilitätsprüfung              |
| Spreizung        | Fehlererkennung und Diagnose                    |

---

## 8. Heizungsregelung

### 8.1 Freigabe

Die Heizung darf nur betrieben werden, wenn:

- die Heizfunktion aktiviert ist,
- keine Kühlfunktion aktiv ist,
- keine Pumpe aktiv ist,
- mindestens ein gültiger Plattensensor vorhanden ist,
- keine kritische Störung vorliegt,
- die kritische Maximaltemperatur nicht erreicht ist.

### 8.2 Einschaltbedingung

Die Heizung beginnt, sobald die niedrigste gültige Plattentemperatur die untere Heizschwelle unterschreitet.

Die untere Heizschwelle ergibt sich aus:

```text
Heizschwelle_EIN = Zieltemperatur_Heizen - Hysterese_Heizen
```

### 8.3 Ausschaltbedingung

Die Heizung endet, sobald die niedrigste gültige Plattentemperatur die konfigurierte Zieltemperatur für den Heizbetrieb erreicht oder überschreitet.

Zusätzlich muss die Heizung immer abgeschaltet werden, sobald die höchste gültige Plattentemperatur die konfigurierte kritische Maximaltemperatur erreicht oder überschreitet.

### 8.4 Verhalten bei Sensorfehlern

Bei Ausfall einzelner Plattensensoren wird mit den verbleibenden gültigen Sensoren weitergeregelt.

Bei Ausfall aller Plattensensoren wird die Heizungsregelung vollständig deaktiviert und der Heizdraht abgeschaltet.

---

## 9. Kühlungsregelung

### 9.1 Grundsätzliche Freigabe

Die Plattenkühlung darf nur betrieben werden, wenn:

- die Kühlfunktion aktiviert ist,
- keine Heizung aktiv ist,
- mindestens ein gültiger Plattensensor vorhanden ist,
- die Außentemperatur mindestens 15 °C beträgt,
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

Bei einer Außentemperatur unter 15 °C ist die Plattenkühlung grundsätzlich deaktiviert.

Diese Sperre gilt unabhängig von der Plattentemperatur.

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

## 12. Sensorfehler und Fallbackbetrieb

| Fehlerfall                                   | Verhalten                                                                                         |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Tanksensor ausgefallen                       | Verdunstungskühlung aus; Plattenkühlung bleibt möglich; Tank-Platten-Abstand gilt als ausreichend |
| Außenluftsensor ausgefallen                  | Verdunstungskühlung aus                                                                           |
| Innenraumsensor ausgefallen                  | Taupunktberechnung mit Außenluftsensor                                                            |
| Innenraum- und Außenluftsensor ausgefallen   | Fester Taupunkt von 23 °C                                                                         |
| Ein oder mehrere Plattensensoren ausgefallen | Regelung mit verbleibenden gültigen Sensoren                                                      |
| Alle Plattensensoren ausgefallen             | Heizungs- und Plattenkühlungsregelung aus                                                         |
| WLAN ausgefallen                             | Lokale Regelung läuft unverändert weiter                                                          |
| MQTT-Verbindung ausgefallen                  | Lokale Regelung und Webserver laufen weiter                                                       |
| Webserver nicht erreichbar                   | Lokale Regelung und MQTT laufen weiter                                                            |

Sensorfehler sind als Fehlerzustände zu protokollieren und auf Website sowie MQTT bereitzustellen.

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

### 14.1 Allgemeine Anforderungen

Der ESP32 stellt unter seiner statischen IP-Adresse einen lokalen Webserver bereit.

Der Webserver dient zur:

- Anzeige aktueller Messwerte,
- Anzeige aktueller Zustände,
- Anzeige von Aktorzuständen,
- Anzeige aktiver Fehler,
- Anzeige eines Fehlerverlaufs,
- Darstellung ausgewählter Messwertverläufe,
- Änderung der Konfiguration.

Die lokale Regelung darf nicht von der Verfügbarkeit des Webservers abhängen.

### 14.2 Statusübersicht

Die Website muss mindestens folgende aktuelle Werte darstellen:

- Systemzustand,
- WLAN-Verbindungsstatus,
- MQTT-Verbindungsstatus,
- Heizdrahtstatus,
- Kühlkreispumpenstatus,
- Verdunstungspumpenstatus,
- Raumtemperatur,
- Raumluftfeuchtigkeit,
- Außentemperatur,
- Außenluftfeuchtigkeit,
- Tanktemperatur,
- niedrigste Plattentemperatur,
- höchste Plattentemperatur,
- mittlere Plattentemperatur,
- Detailwerte aller neun Plattensensoren,
- berechneter Taupunkt,
- berechnete Feuchtkugeltemperatur,
- aktive Fehler.

### 14.3 Verlaufsansicht

Die Website muss mindestens folgende zeitliche Verläufe darstellen:

- Raumtemperatur,
- Außentemperatur,
- Tanktemperatur,
- mittlere Plattentemperatur.

Die Verlaufsspeicherung darf als Ringpuffer ausgeführt werden.

Nach Überschreiten der Speicherkapazität werden die ältesten Werte überschrieben.

Die Anzahl der Messpunkte und das Speicherintervall müssen zentral konfigurierbar oder im Quellcode eindeutig definiert sein.

### 14.4 Fehleranzeige

Die Website muss darstellen:

- aktuell aktive Fehler,
- Zeitpunkt des ersten Auftretens,
- Zeitpunkt des letzten Auftretens,
- Fehlerstatus,
- Verlauf vergangener Fehlermeldungen.

Ein Fehlerverlauf darf ebenfalls als Ringpuffer umgesetzt werden.

### 14.5 Konfiguration über Website

Mindestens folgende Parameter müssen über die Website veränderbar sein:

- Hysterese der Kühlung,
- Hysterese der Heizung,
- Aktivierung der Heizung,
- Aktivierung der Kühlung,
- Zieltemperatur Heizen,
- Zieltemperatur Kühlen,
- Aktivierung der Verdunstungskühlung,
- Mindestwert der theoretischen Kühlkapazität zum Start der Verdunstungskühlung,
- Mindestabstand zwischen Taupunkt und Plattentemperatur,
- Mindestabstand zwischen Tanktemperatur und Plattentemperatur.

### 14.6 Validierung von Konfigurationswerten

Alle Eingaben sind vor Übernahme zu prüfen.

Zu prüfen sind mindestens:

- Zahlenformat,
- zulässiger Wertebereich,
- logische Konsistenz,
- Vollständigkeit.

Ungültige Werte dürfen nicht übernommen werden.

### 14.7 Speicherung der Konfiguration

Änderungen über die Website sind nichtflüchtig im ESP32 zu speichern.

Nach einem Neustart müssen die zuletzt gültig gespeicherten Werte wiederhergestellt werden.

Als Speicher kann beispielsweise NVS beziehungsweise `Preferences` verwendet werden.

---

## 15. MQTT-Schnittstelle

### 15.1 Allgemeine Anforderungen

Der ESP32 stellt eine MQTT-Schnittstelle im lokalen Netzwerk bereit.

Die MQTT-Verbindung darf für die lokale Regelung nicht erforderlich sein.

Bei Verbindungsverlust:

- läuft die lokale Regelung unverändert weiter,
- werden Wiederverbindungsversuche nicht blockierend fortgesetzt,
- werden Statuswerte nach erfolgreicher Wiederverbindung erneut veröffentlicht.

### 15.2 Konfiguration über MQTT

Über MQTT müssen dieselben Konfigurationswerte veränderbar sein wie über die Website:

- Hysterese der Kühlung,
- Hysterese der Heizung,
- Aktivierung der Heizung,
- Aktivierung der Kühlung,
- Zieltemperatur Heizen,
- Zieltemperatur Kühlen,
- Aktivierung der Verdunstungskühlung,
- Mindestwert der theoretischen Kühlkapazität,
- Mindestabstand zwischen Taupunkt und Plattentemperatur,
- Mindestabstand zwischen Tanktemperatur und Plattentemperatur.

Eingehende MQTT-Konfigurationswerte müssen denselben Validierungsregeln unterliegen wie Eingaben über die Website.

Nach erfolgreicher Änderung sind:

- der neue Wert nichtflüchtig zu speichern,
- der aktuell wirksame Wert über ein Status- oder Bestätigungstopic zu veröffentlichen.

### 15.3 Über MQTT bereitzustellende Daten

Mindestens folgende Daten sind zu veröffentlichen:

- Systemzustand,
- Status Heizdraht,
- Status Kühlkreispumpe,
- Status Verdunstungspumpe,
- Tanktemperatur,
- Raumtemperatur,
- Raumluftfeuchtigkeit,
- Außentemperatur,
- Außenluftfeuchtigkeit,
- mittlere Plattentemperatur,
- Detailwert Plattensensor 1,
- Detailwert Plattensensor 2,
- Detailwert Plattensensor 3,
- Detailwert Plattensensor 4,
- Detailwert Plattensensor 5,
- Detailwert Plattensensor 6,
- Detailwert Plattensensor 7,
- Detailwert Plattensensor 8,
- Detailwert Plattensensor 9,
- berechneter Taupunkt,
- berechnete Feuchtkugeltemperatur,
- aktive Fehler,
- WLAN-Verbindungsstatus,
- MQTT-Verbindungsstatus.

### 15.4 Topic-Struktur

Die MQTT-Topic-Struktur ist hierarchisch und eindeutig zu definieren.

Beispiel:

```text
kaninchenstall/klima/status/state
kaninchenstall/klima/status/heater
kaninchenstall/klima/status/pump/cooling
kaninchenstall/klima/status/pump/evaporation

kaninchenstall/klima/sensor/room/temperature
kaninchenstall/klima/sensor/room/humidity
kaninchenstall/klima/sensor/outside/temperature
kaninchenstall/klima/sensor/outside/humidity
kaninchenstall/klima/sensor/tank/temperature
kaninchenstall/klima/sensor/plate/average
kaninchenstall/klima/sensor/plate/1
...
kaninchenstall/klima/sensor/plate/9

kaninchenstall/klima/config/heating/enabled/set
kaninchenstall/klima/config/heating/target/set
kaninchenstall/klima/config/heating/hysteresis/set
kaninchenstall/klima/config/cooling/enabled/set
kaninchenstall/klima/config/cooling/target/set
kaninchenstall/klima/config/cooling/hysteresis/set
kaninchenstall/klima/config/evaporation/enabled/set
kaninchenstall/klima/config/evaporation/min_capacity/set
kaninchenstall/klima/config/dewpoint/min_distance/set
kaninchenstall/klima/config/tank_plate/min_distance/set
```

Die endgültige Topic-Struktur ist zentral zu konfigurieren.

### 15.5 Retain- und Last-Will-Verhalten

Konfigurations- und Statuswerte sollen, soweit sinnvoll, als Retain-Nachrichten veröffentlicht werden.

Ein MQTT Last Will and Testament soll den Online-Status des ESP32 abbilden.

Beispiel:

```text
kaninchenstall/klima/status/availability = offline
```

Nach erfolgreicher Verbindung veröffentlicht der ESP32:

```text
kaninchenstall/klima/status/availability = online
```

---

## 16. Konfigurationsparameter

Mindestens folgende Parameter sind zentral vorzusehen:

| Parameter                         | Einheit | Bedeutung                                      |
| --------------------------------- | -------:| ---------------------------------------------- |
| `heating_enabled`                 | bool    | Heizung aktiviert                              |
| `cooling_enabled`                 | bool    | Plattenkühlung aktiviert                       |
| `evaporation_enabled`             | bool    | Verdunstungskühlung aktiviert                  |
| `heating_target`                  | °C      | Zieltemperatur Heizen                          |
| `heating_hysteresis`              | K       | Einschalthysterese Heizen                      |
| `cooling_target`                  | °C      | Zieltemperatur Kühlen                          |
| `cooling_hysteresis`              | K       | Einschalthysterese Kühlen                      |
| `heating_critical_max`            | °C      | Kritische maximale Plattentemperatur           |
| `dewpoint_plate_min_distance`     | K       | Mindestabstand Platte zu Taupunkt              |
| `tank_plate_min_distance`         | K       | Mindestabstand Tank zu Platte                  |
| `evaporation_min_capacity`        | K       | Mindestdifferenz Tank zu Feuchtkugeltemperatur |
| `cooling_min_outside_temperature` | °C      | Außentemperatursperre, Standard 15 °C          |
| `sensor_cycle_ms`                 | ms      | Sensorabfrageintervall                         |
| `control_cycle_ms`                | ms      | Regelzyklus                                    |
| `history_interval_ms`             | ms      | Speicherintervall der Verlaufsdaten            |
| `wifi_retry_interval_ms`          | ms      | WLAN-Wiederholungsintervall                    |
| `mqtt_retry_interval_ms`          | ms      | MQTT-Wiederholungsintervall                    |

Alle Parameter sind mit:

- Standardwert,
- Minimalwert,
- Maximalwert,
- Einheit,
- Beschreibung

zu definieren.

---

## 17. Prioritätsregeln

Die Software muss folgende Prioritätsreihenfolge einhalten:

1. kritische Abschaltungen,
2. Ausgangsverriegelung,
3. Prüfung gültiger Plattensensoren,
4. kritische Maximaltemperatur,
5. Taupunktschutz,
6. Außentemperatursperre,
7. Tank-Platten-Abstand,
8. Heiz- oder Kühlzustandslogik,
9. Verdunstungskühlung,
10. Webserver,
11. MQTT,
12. Diagnose und Verlaufsspeicherung.

Netzwerkfunktionen dürfen die Regel- und Sicherheitsfunktionen niemals blockieren.

---

## 18. Fehlerverwaltung

Fehler sind mindestens in folgende Klassen einzuteilen:

- Hinweis,
- Warnung,
- kritische Störung.

Mindestens folgende Fehler sind zu erkennen:

- Innenraumsensor ausgefallen,
- Außenluftsensor ausgefallen,
- Tanksensor ausgefallen,
- einzelner Plattensensor ausgefallen,
- alle Plattensensoren ausgefallen,
- unplausible Plattentemperatur,
- kritische Maximaltemperatur erreicht,
- Taupunktgrenze erreicht oder unterschritten,
- WLAN-Verbindung verloren,
- MQTT-Verbindung verloren,
- Webserverstart fehlgeschlagen,
- Konfiguration ungültig,
- unzulässige Ausgangskombination erkannt,
- Watchdog-Neustart.

Fehler müssen enthalten:

- eindeutigen Fehlercode,
- lesbaren Fehlertext,
- Fehlerklasse,
- ersten Zeitstempel,
- letzten Zeitstempel,
- Aktivstatus,
- Anzahl der Wiederholungen.

---

## 19. Datenhaltung

Nichtflüchtig zu speichern sind mindestens:

- alle Konfigurationsparameter,
- optional der letzte gültige Systemzustand,
- Neustartzähler,
- optional ausgewählte Diagnosezähler.

Der letzte Aktorzustand darf nach einem Neustart nicht automatisch wiederhergestellt werden.

Nach jedem Neustart beginnt das System mit ausgeschalteten Aktoren im Zustand `INITIALISIERUNG`.

Verlaufsdaten und Fehlerhistorie dürfen im RAM als Ringpuffer gehalten werden. Ein Neustart darf diese Daten löschen, sofern keine dauerhafte Speicherung ausdrücklich ergänzt wird.

---

## 20. Initialisierung und Neustartverhalten

Beim Start gilt:

1. alle Aktorausgänge als Ausgang konfigurieren,
2. alle Aktoren ausschalten,
3. Konfiguration laden,
4. Konfiguration validieren,
5. Sensorbusse initialisieren,
6. Sensoren identifizieren,
7. erste Messwerte erfassen,
8. WLAN-Verbindung starten,
9. Webserver starten, sobald WLAN verfügbar ist,
10. MQTT-Verbindung starten, sobald WLAN verfügbar ist,
11. Zustandsautomat freigeben.

Die Regelung darf bereits lokal arbeiten, während WLAN oder MQTT noch nicht verbunden sind.

---

## 21. Watchdog und Laufzeitverhalten

Ein Watchdog ist vorzusehen.

Folgende Aufgaben dürfen nicht blockieren:

- WLAN-Verbindungsaufbau,
- MQTT-Verbindungsaufbau,
- Webserververarbeitung,
- Sensorabfrage,
- Verlaufsspeicherung.

Nach Watchdog-Neustart müssen alle Aktoren zunächst ausgeschaltet bleiben.

---

## 22. Diagnoseausgabe

Über die serielle Schnittstelle sind mindestens auszugeben:

- Systemzustand,
- Zeit seit Systemstart,
- WLAN-Status,
- IP-Adresse,
- MQTT-Status,
- Sensorwerte,
- Plattenminimum,
- Plattenmaximum,
- Plattenmittelwert,
- Taupunkt,
- Feuchtkugeltemperatur,
- Aktorzustände,
- aktive Fehler,
- Zustandswechsel,
- Konfigurationsänderungen.

Passwörter, WLAN-Schlüssel und MQTT-Zugangsdaten dürfen nicht vollständig ausgegeben werden.

---

## 23. Sicherheitsanforderungen

### 23.1 Allgemeiner sicherer Zustand

Der sichere Zustand ist:

- Heizdraht aus,
- Kühlkreispumpe aus,
- Verdunstungspumpe aus.

### 23.2 Kritische Maximaltemperatur

Erreicht die höchste gültige Plattentemperatur die kritische Maximaltemperatur, muss der Heizdraht unabhängig vom aktuellen Zustand sofort abgeschaltet werden.

### 23.3 Netzwerkfehler

WLAN-, Webserver- und MQTT-Fehler dürfen keine Aktorumschaltung auslösen.

### 23.4 Konfigurationsfehler

Bei ungültiger gespeicherter Konfiguration sind sichere Standardwerte zu laden.

Die fehlerhafte Konfiguration ist als Fehler zu melden.

### 23.5 Unzulässige Ausgangskombination

Wird eine unzulässige Ausgangskombination erkannt, sind alle Aktoren sofort auszuschalten und eine kritische Störung zu setzen.

---

## 24. Abnahmekriterien

### 24.1 Heizungsregelung

- Heizung startet, wenn die niedrigste Plattentemperatur die Heizschwelle unterschreitet.
- Heizung endet, wenn die niedrigste Plattentemperatur den Heizsollwert erreicht.
- Heizung endet sofort, wenn die höchste Plattentemperatur die kritische Maximaltemperatur erreicht.
- Pumpen bleiben während des Heizbetriebs ausgeschaltet.

### 24.2 Kühlungsregelung

- Kühlung startet, wenn die höchste Plattentemperatur die Kühlschwelle überschreitet.
- Kühlung endet, wenn die höchste Plattentemperatur den Kühlsollwert erreicht.
- Kühlung endet, wenn die niedrigste Plattentemperatur den zulässigen Taupunktabstand unterschreitet.
- Kühlung bleibt bei Außentemperaturen unter 15 °C ausgeschaltet.
- Kühlung bleibt ausgeschaltet, wenn der Tank-Platten-Abstand zu gering ist.
- Bei Tanksensorfehler darf die Plattenkühlung weiterlaufen.

### 24.3 Verdunstungskühlung

- Verdunstungskühlung startet bei ausreichender Differenz zwischen Tank- und Feuchtkugeltemperatur.
- Bei Ausfall des Außenluftsensors wird sie deaktiviert.
- Bei Ausfall des Tanksensors wird sie deaktiviert.

### 24.4 Sensorfallback

- Bei Ausfall des Innenraumsensors wird der Außensensor zur Taupunktberechnung verwendet.
- Bei Ausfall beider Klimasensoren wird mit einem Taupunkt von 23 °C weitergeregelt.
- Bei Ausfall einzelner Plattensensoren wird mit den übrigen Sensoren weitergeregelt.
- Bei Ausfall aller Plattensensoren werden Heizung und Plattenkühlung deaktiviert.

### 24.5 WLAN und Webserver

- Die Regelung läuft bei WLAN-Verlust unverändert weiter.
- Der ESP32 versucht automatisch, die Verbindung wiederherzustellen.
- Nach Wiederverbindung ist der Webserver erneut erreichbar.
- Die Website zeigt aktuelle Zustände, Messwerte, Fehler und Verläufe an.
- Konfigurationsänderungen werden validiert und nichtflüchtig gespeichert.

### 24.6 MQTT

- Status- und Sensordaten werden veröffentlicht.
- Konfigurationswerte können über MQTT geändert werden.
- Ungültige Konfigurationswerte werden abgelehnt.
- Bei MQTT-Ausfall läuft die lokale Regelung unverändert weiter.
- Nach Wiederverbindung werden aktuelle Statuswerte erneut veröffentlicht.

---

## 25. Offene Implementierungsentscheidungen

Vor Beginn der Implementierung sind noch festzulegen:

- konkrete Sensortypen,
- GPIO-Belegung,
- genaue Sensoradressen und Sensorpositionen,
- Webframework,
- Darstellungsbibliothek für Verlaufsdiagramme,
- MQTT-Brokeradresse und Zugangsdaten,
- endgültige Topic-Struktur,
- Standard-, Minimal- und Maximalwerte aller Parameter,
- Größe der Ringpuffer,
- Mess- und Speicherintervalle,
- Authentifizierung der Website,
- Authentifizierung und Verschlüsselung der MQTT-Verbindung.

---

## 26. Versionshistorie

| Version | Datum         | Änderungen                                                                                                                                                                   |
| ------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1.0     | Juli 2026     | Erstfassung des Pflichtenhefts                                                                                                                                               |
| 1.1     | 17. Juli 2026 | Ergänzung um WLAN, statische IP, Webserver, Verlaufsspeicherung, Fehlerhistorie, MQTT, Konfiguration, detaillierte Heiz-, Kühl- und Verdunstungsregeln sowie Sensorfallbacks |
