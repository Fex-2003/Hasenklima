# Inbetriebnahmeparameter

Die Setup-Seite und `GET /api/v1/parameters` zeigen die Validierungsgrenzen und Regeln aus demselben Register. `UNSET` bedeutet, dass ein Wert noch einzutragen ist. Werte mit Entwicklungsdefault sind beim Aufbau zu bestätigen. Die Eingabe in typischen deutschen Browsern verwendet die Browser-Zahlenfelder, die API verwendet JSON-Zahlen.

| Bereich | IDs oder Felder | Auslieferung und Prüfung |
|---|---|---|
| Code | `MAX_PLATE_TEMP_C`, Builddefinition `KLIMA_MAX_PLATE_TEMP_C` | UNSET. Nur im Build setzen und thermisch qualifizieren. |
| Hardware | Modul, externe Antenne, Flash, Partitionen, OTA | `esp32dev` und generischer 4-MiB-Flashplan für Entwicklung. Tatsächliche Hardware bestätigen. |
| Ausgänge | `heating_enabled`, `cooling_enabled`, `evaporation_enabled` | Alle AUS. Je Ausgang nach Prüfung bewusst freigeben. |
| Sensoren | `plate[i].rom_id`, `plate[i].enabled`, `tank.rom_id` | Keine Zuordnung. Aktivierung erfordert eine gültige eindeutige ROM-ID. |
| Plattenziele | `heating_target_c`, `cooling_target_c` | UNSET. Heizsollwert < Kühlsollwert. Feste Obergrenze und Hysteresen beachten. |
| Plattenkühlung | `cooling_hysteresis_k`, `cooling_min_outside_c`, `dewpoint_margin_k`, `tank_plate_margin_k` | 1 K, 15 °C, 2 K, 1 K. Wirksamkeit, Kondensat und Fallbacks am Aufbau prüfen. |
| Verdunstung | `evaporation_min_capacity_k`, `evaporation_hysteresis_k` | 2 K, 0,5 K. Abgeleitete Feuchtkugeltemperatur anhand der Außenmessung. |
| PI | `pi_kp_duty_per_k`, `pi_ti_s`, `pi_integral_enabled`, `pi_update_interval_ms` | 0,027 1/K, 4980 s, EIN, 5000 ms als Entwicklungswerte. Final mit Median-Sprungantwort bestimmen. |
| Heizdraht und Strom | `heater_resistance_ohm`, `supply_rated_current_a`, `heater_input_current_limit_a` | UNSET, 3 A, 1,5 A. Drahtwiderstand messen, Netzteil bestätigen, Reserve für Nebenlasten und Unsicherheit berücksichtigen. |
| VIN | `vin_valid_min_v`, `vin_valid_max_v`, `vin_gain`, `vin_offset_v`, `vin_calibration_confirmed` | Grenzen UNSET. 16 und 0 V nominal. Kalibrierung mit Referenzmessgerät bestätigen. |
| Safety | `overtemp_release_hysteresis_k`, `sensor_max_age_ms`, `vin_max_age_ms` | 2 K, 6000 ms und 1000 ms. Hysterese verändert ausschließlich die Wiederfreigabe. |
| Sensordiagnose | `plate_spread_warning_k`, `plate_change_warning_k_per_s` | UNSET. Optionale Warnschwellen erst aus Messungen ableiten. |
| Q4 | `driver_heater_enabled`, `driver_heater_on_temp_c`, `driver_heater_slope_pct_per_k`, `driver_warmup_s` | AUS, UNSET, UNSET, 0 s. Kennlinie und Kaltstart mit Messung qualifizieren. Die Standardausführung sperrt die Hauptheizung bei Außentemperatur unter 0 °C und bei fehlender Außentemperatur. |
| Manuell | `manual_timeout_s`, Sitzung, PWM, Pumpen, Taupunkt-Override | Default 300 s. 60–3600 s pro Sitzung, Override AUS und als Sitzungsoption sichtbar. |
| Sprungantwort | Vor-, Heiz- und Nachlauf, Test-PWM | 0–60 / 1–500 / 0–60 min. PWM-Vorschlag 12,5 % sofern zulässig. Abtastraster 10 s. |
| Energie | `energy_checkpoint_s` | 900 s. Kürzere Intervalle erhöhen Flashzugriffe. Reset gesondert bestätigen. |
| WLAN | SSID, Passwort, IP, Maske, Gateway, DNS | Leer. Initial über lokalen Header, später über Setup. |
| Lokaler Zugang | Eigenes Gerätepasswort | Kein Auslieferungspasswort. Nur gesalzener Prüfwert im NVS. |
| MQTT | Enable, Host, Port, Benutzer, Passwort, Client-ID, Präfix, Intervall | AUS. Port 1883, Präfix `kaninchenstall/klima`, 10 s. Bei mehreren Geräten eindeutige Identität/Präfix vergeben. |
| Uhr | NTP-Server, Anzeigezeitzone | NTP leer, `Europe/Berlin` Anzeigevorgabe. Absolute CSV-Zeit in UTC, Ablaufzeiten monoton. |

## Abfolge für die Regelung

Mit gesperrten Hauptausgängen Sensorzuordnung und Spannung prüfen. Für die Platzierung der Sensoren die Ansicht von oben mit Eingang unten bestätigen. Ein deaktivierter Sensor wird weiter gelesen, falls erreichbar. Seine Position entfällt ab wirksamer Änderung in Statistik und Abschaltung. Eine nach Fehler aktivierte verbleibende Menge braucht einen neuen vollständigen Satz und gegebenenfalls eine Fehlerquittierung.

Heizstromlimit und gemessenen Widerstand so eintragen, dass die vorgesehene Leistungsstufe und Netzteilreserve eingehalten werden. Beispielwerte aus den Spezifikationen bleiben Rechenbeispiele. Erst bei elektrisch und thermisch qualifiziertem Aufbau die Hauptfunktionen freigeben. Die Abnahme der Pumpen umfasst die 12-V-Zuordnung, J2 gesteckt und J3/J7 offen bei 24-V-Einspeisung, gemeinsame Pumpenlast und Schutzbeschaltung. Die Firmware besitzt keine Jumper-, Pumpenlauf- oder Stromrückmeldung.

Bei freigegebener Heizung und ruhender Automatik startet ein bestätigter Versuch erst nach Speicherprüfung und dauerhaftem Dateikopf. Vorlauf, Heizphase und Nachlauf laufen auch ohne WLAN weiter. Die Heizphase hält die bestätigte quantisierte PWM. Bei einer gesunkenen zulässigen PWM oder einem Fehler endet sie. CSV verwendet Dezimalpunkt und Kommentare im Format `# key=value`. Der Verlauf enthält die sieben Spalten aus dem Softwareentwurf.

Vor regulärer Freigabe die reale Messtrecke mit Median, thermischer Zeitkonstante, Totzeit und Prozessverstärkung auswerten. Die Entwicklungsparameter werden dann ersetzt. Tests der Heizlast und der Sicherheit am aufgebauten Rückzugsraum erfolgen vor Tierbetrieb.
