# OpenDTU

[🇬🇧 English](README.md) | 🇩🇪 **Deutsch**

[![cpplint](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnlint.yml)
[![Yarn Prettier](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnprettier.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnprettier.yml)

## Worum geht es in diesem Fork?

Dieser OpenDTU-Fork unterstützt neben den ursprünglichen Hoymiles-nRF24-Wechselrichtern auch Deye Sun (WLAN), Hoymiles W-Series (WLAN) und Hoymiles WB-Series (Bluetooth).
Er erweitert OpenDTU um mehrere zusätzliche Funktionen, die über Build-Flags in [platformio.ini](platformio.ini) gesteuert werden — das entsprechende Flag auf `1` oder `0` setzen aktiviert bzw. deaktiviert die jeweilige Funktion.

### Neue Funktionen

- **Deye SUN** — Deye SUN Micro-Wechselrichter über WLAN/Netzwerk (UDP, TCP, Modbus)
- **Hoymiles W-Series** — HMS-*W-Wechselrichter (1T / 2T / 4T) über WLAN/Netzwerk (TCP, Protobuf)
- **HiFlow BLE** — Hoymiles HiFlow-Wechselrichter (HMS-\*-WB / HF-\*-WB) per Bluetooth Low Energy (ESP32-S3)
- **REST Push Service** — Überträgt Wechselrichterdaten an einen externen Monitoring-Server
- **Servo Engine** — Physischer Servo-Ausgang, der die Solarleistung anzeigt
- **Start Time** — Betrieb ohne Internet durch Festlegen einer Fallback-Zeit

### Bestehende Funktionen (jetzt optional abschaltbar)

- **Hoymiles nRF24/CMT2300** — Unterstützung der ursprünglichen Hoymiles-Wechselrichter per RF-Funkmodul
- **MQTT** — Veröffentlichung der Wechselrichterdaten an einen MQTT-Broker
- **Ethernet (W5500)** — Kabelgebundene Netzwerkanbindung für DTU-Boxen mit W5500-Chip
- **mDNS** — Namensauflösung im LAN für Wechselrichter-Hostnamen
[LICENSE](LICENSE)
## Build-Flags

Nicht alle Funktionen passen in die 2-MB-OTA-Firmware, daher werden sie über Build-Flags gesteuert — nur das Nötige zu aktivieren hält die Firmware klein genug für Web-OTA-Updates.
Alle Feature-Flags werden im `build_flags`-Abschnitt von [platformio.ini](platformio.ini) gesetzt.

| Flag             | Standard | Beschreibung                                                    |
|------------------|----------|-----------------------------------------------------------------|
| `-DHOYMILES`     | `1`      | Unterstützung der ursprünglichen Hoymiles-NRF24-Wechselrichter  |
| `-DDEYE_SUN`     | `1`      | Unterstützung der Deye SUN-Wechselrichter (Netzwerk, WLAN)      |
| `-DHOYMILES_W`   | `1`      | Unterstützung der Hoymiles W-Series-Wechselrichter (Netzwerk, WLAN) |
| `-DHIFLOW_BLE`   | `1`      | Unterstützung der Hoymiles HiFlow BLE-Wechselrichter (ESP32-S3) |
| `-DSERVO`        | `0`      | Servo-Ausgang, der die Solarleistung anzeigt                    |
| `-DTOST`         | `0`      | REST Push Service, der Wechselrichterdaten an einen Monitoring-Server sendet |
| `-DMQTT`         | `0`      | Veröffentlichung der Wechselrichterdaten an einen MQTT-Broker   |
| `-DENABLE_W5500` | `0`      | Kabelgebundenes Ethernet über den W5500-Chip                    |
| `-DENABLE_MDNS`  | `0`      | mDNS-Namensauflösung im LAN                                     |

> **Hinweis zu den Frontend-Build-Flags:** Das Frontend wird mit den Vite-
> Flags `VITE_TOST`, `VITE_MQTT` und `VITE_MDNS` kompiliert, die mit den Firmware-
> Flags `-DTOST`, `-DMQTT` und `-DENABLE_MDNS` übereinstimmen müssen. Die Web-App
> wird mit `yarn build` im Verzeichnis `webapp/` gebaut
> (z. B. `VITE_TOST=1 VITE_MQTT=1 VITE_MDNS=1 yarn build`).

## Funktionen

### Weitere Hersteller

Dank dieses Forks können auch weitere Wechselrichter-Typen neben den Hoymiles-nRF24-Geräten ausgelesen werden.
Die zusätzlichen Wechselrichter-Typen werden über Netzwerk (WLAN) oder Bluetooth abgefragt. Bei den WLAN-Wechselrichtern muss der ESP im selben Netzwerk wie der Wechselrichter sein.

Alle Wechselrichter-Typen sind eigenständige Implementierungen und können über Build-Flags aktiviert oder deaktiviert werden — siehe den Abschnitt [Build-Flags](#build-flags) oben.

> **Hinweis zu den Geräte-Modellen:** Nicht alle Wechselrichter werden anhand ihrer
> Seriennummer bereits dem richtigen Modell zugeordnet. Geräte mit unbekannter Seriennummer
> werden als unbekannt angezeigt (oder auf ein Standardmodell zurückgefallen), das Modell
> kann aber in den Wechselrichtereinstellungen manuell überschrieben werden.
> Falls dein Gerät unbekannt ist oder falsch zugeordnet wird, wäre es hilfreich,
> Seriennummer und Modell in [Issue #6](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/6) zu ergänzen.
> Die im Code hinterlegten Seriennummern sind in
> [docs/known_inverter_serials.md](docs/known_inverter_serials.md) aufgelistet.

### Status Deye Sun

Der ESP verbindet sich über das Netzwerk ((UDP, 48899), (TCP, 8899, Modbus)) mit dem konfigurierten Wechselrichter (IP/Hostname/MAC bei Verbindung zum AP des ESP) und dessen Port.
Er liest alle 5 Minuten alle Daten aus. Häufigeres Auslesen ist wegen der Einschränkungen der Wechselrichter nicht möglich.
Health-Checks werden häufiger durchgeführt; der Abstand ist in den erweiterten Wechselrichtereinstellungen konfigurierbar.

Die aktuelle Firmware-Version wird ausgelesen und der Neustart-Befehl per HTTP-REST-Call ausgelöst.
Benutzername und Passwort dafür können in den erweiterten Geräteeinstellungen konfiguriert werden.

Aktuell gibt es drei Arten der Verbindung zum Wechselrichter, abhängig von der installierten Firmware.

|                       | At-Commands                | Custom Modbus                | Modbus                      |
|-----------------------|----------------------------|------------------------------|-----------------------------|
| Firmware              | Alte Firmware (1.0 - 2.32) | Neue Firmware, z. B. 1.0B, 5.0C | Bei Micro-Wechselrichtern nicht beobachtet |
| Entwicklungsstand      | Lesen und Schreiben        | Lesen und Schreiben          | Nicht implementiert         |

Getestet mit den Modellen: SUN300G3-EU-230, SUN-M60/80/100G4-EU-Q0, Deye SUN600G3-EU-230

#### Funktioniert

- Daten auslesen
- Firmware-Version des Wechselrichters auslesen
- Ein- und Ausschalten (dient auch als "Reset" bei dauerhaft roter LED; bei unbekannten Geräten muss die Funktion dazu auf der Seite DTU/Allgemeine Einstellungen aktiviert werden)
- Leistungslimit setzen (bei unbekannten Geräten muss die Funktion dazu auf der Seite DTU/Allgemeine Einstellungen aktiviert werden)
- Wechselrichter neu starten

#### Funktioniert nicht

- Logs zeigen Hardware-Fehler des Wechselrichters

#### Zusätzliche Funktionen

Wenn der Deye Sun-Wechselrichter keine Internetverbindung (Remote-Server) hat, wird der tägliche kWh-Wert nicht zurückgesetzt. Um das in der
OpenDTU-Anzeige zu beheben, kann in den erweiterten Wechselrichtereinstellungen das "Deye Sun offline yield day flag" aktiviert werden.
Dann erfasst und berechnet OpenDTU den Tages-Ertrag selbst.

### Status Hoymiles W-Series

Der ESP verbindet sich über das Netzwerk (TCP, Protobuf) mit dem konfigurierten Wechselrichter (IP/Hostname/MAC bei Verbindung zum AP des ESP) und dessen Port.

Die DTU-Verbindung basiert auf dem Befehlssatz des [hoymiles-wifi](https://github.com/suaveolent/hoymiles-wifi)-Projekts
(Konstanten und Befehle wurden aus dessen Python-Implementierung portiert).

> **Wechselrichter-Firmware ab v1.01.01 wird noch nicht unterstützt:** Die Inverter-/DTU-Firmware ab v1.01.01 funktioniert derzeit nicht —
> siehe [Issue #12](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/12).
> Die neue Firmware verwendet ein neues Protokoll mit Verschlüsselung, das noch nicht implementiert ist (in einem späteren Release geplant).

Getestet mit dem Modell: HMS-800W-2T

Der Abstand zwischen den Datenabrufen lässt sich in den erweiterten Wechselrichtereinstellungen konfigurieren.
Das ist nötig, da bei den ersten Firmware-Versionen des Geräts (wie 1.\*.\*) nur alle 31 Sekunden abgefragt werden durfte.
Der Standardwert liegt daher bei 31 Sekunden. Mit neueren Firmware-Versionen (2.\*.\* und neuer) ist häufigeres Auslesen möglich — 20 Sekunden
scheinen ein guter Wert zu sein (am besten selbst ausprobieren).

#### Funktioniert

- Daten auslesen
- Firmware-Version von Wechselrichter und DTU auslesen
- Leistungslimit setzen
- Ein- und Ausschalten
- Wechselrichter neu starten
- DTU neu starten

#### Funktioniert nicht

- Logs zeigen Hardware-Fehler des Wechselrichters
- Unterstützung für 6T-Wechselrichter ist noch nicht vorhanden.

#### Netzwerk-Betrieb als Gateway

Wenn du deinen Wechselrichter vom Internet trennen möchtest, ohne die Verbindungen am Router blockieren zu müssen (damit er keine unerwarteten Updates
durchführt, wie kürzlich beobachtet), kann OpenDTU in den dauerhaften AP-Modus versetzt werden: Auf der Netzwerksseite die Zeit auf null
stellen und die Wechselrichter direkt mit dem OpenDTU-AP verbinden.
Bitte ändere das Passwort dafür in etwas komplexeres. In den Netzwerkeinstellungen der Wechselrichter kann dann die MAC-Adresse
für die Verbindung verwendet werden — die DTU löst sie in die korrekte IP-Adresse um. Verbundene Geräte sind auf der Network-Info-Seite
der DTU zu sehen. Eine genauere Beschreibung dieses Betriebs gibt es in diesem [Blogpost](https://itgrufti.de/solar/monitoring/opendtu-als-gateway-zum-wechselrichter/).

### HiFlow BLE (HMS-\*-WB / HF-\*-WB Series)

Der ESP32-S3 verbindet sich per Bluetooth Low Energy (BLE) mit Hoymiles HiFlow-Wechselrichtern (HMS-\*-WB, HF-\*-WB-Serie).
Zusätzliche Funk-Hardware (nRF24/CMT2300) ist nicht nötig — es reicht das eingebaute BLE-Modul des ESP32-S3.

Die BLE-Protokoll-Implementierung basiert auf der Reverse-Engineering-Arbeit aus [hiflow-ble](https://github.com/TheTiEr/hiflow-ble/blob/main/README.md).

Getestet mit den Modellen: HMS-800-2WB (Seriennummern-Präfix 0x1610, 0x4161), HF-800-WB (Seriennummern-Präfix 0x1650)

#### Build-Konfiguration

HiFlow-BLE-Unterstützung wird über das Build-Flag `-DHIFLOW_BLE=1` in `platformio.ini` aktiviert.

Für einen **nur-BLE-Build** (ohne Hoymiles-nRF24/CMT2300-Funkunterstützung) gibt es eine dedizierte PlatformIO-Umgebung, die ungenutzte
Funk-Bibliotheken weglässt und so Flash und RAM spart:

```
platformio run -e generic_esp32s3_hiflow
```

Für **beides — Hoymiles-nRF24-Funk und HiFlow BLE — in einer Firmware** gibt es zwei Optionen:
- Ein ESP32-S3-Board mit größerem Flash (8 MB/16 MB) und der passenden Partitionstabelle verwenden (z. B. `partitions_custom_16mb.csv`)
- Oder die Ein-Partition-Konfiguration ohne OTA-Update-Unterstützung (`partitions_custom_4mb one_partition.csv`) auf einem Standard-4-MB-Board verwenden

#### Funktioniert

- Echtzeitdaten auslesen (AC-Leistung, PV-Port-Spannungen/-Ströme/-Leistungen)
- Firmware-Version von Wechselrichter und DTU auslesen (aus der V0-Pairing-Antwort)
- Leistungslimit setzen
- Ein- und Ausschalten
- Wechselrichter neu starten
- DTU neu starten
- BLE-Pairing (V0) und Session-Handshake (V1) mit AES-Verschlüsselung
- PIN über das Frontend konfigurieren (Standard: `123456` — funktioniert bei manchen Geräten nicht; dann muss die PIN zuerst über die offizielle Hoymiles-App geändert werden)

#### Funktioniert nicht / Einschränkungen

- Logs zeigen Hardware-Fehler des Wechselrichters
- Erfordert einen ESP32-S3 (BLE 5.0-fähig)
- Erfordert eine NTP-Zeitsynchronisierung vor der Verbindung (der Wechselrichter lehnt veraltete Zeitstempel ab)

### Start Time

Mit dieser Funktion lässt sich OpenDTU auch an Orten ohne aktive Internetverbindung betreiben. OpenDTU liest nur dann Daten von den
Wechselrichtern aus, wenn die Uhrzeit korrekt gesetzt ist — nach einem Neustart oder Stromausfall würde es ansonsten erst wieder
funktionieren, wenn jemand die Zeit manuell einstellt.
Dafür kann auf der NTP-Einstellungsseite eine Startzeit festgelegt werden, ab der OpenDTU den Wechselrichter ausliest.
Das Datum ist dabei natürlich nicht korrekt, aber zumindest zeigt die DTU wieder Daten an.

### Rest push service

Ich habe eine Anwendung entwickelt, die Solaranlagen auf einem Server mit Diagrammen, Statistiken und allem drum und her überwacht.
Das kann OpenDTU aufgrund der Einschränkungen des ESPs nicht selbst.

Dieser Fork sendet dafür über eine neue Funktion die aktuellen Wechselrichterdaten per REST an die Anwendung.
Die Übertragung läuft über plain HTTP — HTTPS wird bewusst vermieden, weil TLS auf dem ESP32 einen großen Heap-Overhead verursacht —
die Vertraulichkeit wird stattdessen über symmetrische AES-256-GCM-Verschlüsselung der Nutzlast vor dem Versand gewährleistet.

Falls du Interesse an der Anwendung hast oder die REST-Definition für eine eigene Anwendung nutzen willst,
schau dir das [Projekt](https://github.com/tost11/solar-monitoring) an.
Eine öffentliche Instanz gibt es unter [solar.pihost.org](https://solar.pihost.org),
eine Einrichtungsanleitung gibt es [hier](https://itgrufti.de/solar/monitoring/online-monitoring-fuer-opendtu-und-weitere-systeme/).

> **Hinweis:** Diese Funktion ist standardmäßig deaktiviert (`-DTOST=0`). Wenn das `TOST`-Flag geändert wird, muss auch das Frontend
> neu gebaut werden — siehe den Abschnitt [Build-Flags](#build-flags) für Details.

### Servo Engine

Mit dieser Funktion kann ein Servomotor am ESP betrieben werden. Er wird über Pin-Mapping und Hardware-Einstellungen konfiguriert.
Der Wert 0 als Eingang bedeutet, dass die volle Leistung des Wechselrichters verwendet wird; jede andere Zahl entspricht dem Index des verwendeten Eingangs.

Ein Beispielprojekt (Solar-Leistungsanzeige für eine Kita, auf Deutsch) gibt es in diesem [Blogpost](https://itgrufti.de/solar/monitoring/eine-solar-anzeige-fuer-die-kita/).

## Builds

Vorkompilierte Firmware-Builds für alle unterstützten Funktionskombinationen gibt es als
[GitHub Releases](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/releases)
(siehe auch [builds](builds)). Es gibt 14 Varianten: 10 für 4-MB-Boards (Dual Slot,
Web-OTA-Update), 2 für 8-MB-Boards und 2 für 16-MB-Boards.

- Komplette Matrix mit Funktionen, Add-ons, Log-Level und gemessenen Größen:
  [docs/FeatureSizes.md](docs/FeatureSizes.md) (Abschnitt "Release build matrix")
- So werden die Builds erzeugt und wie der CI-Workflow funktioniert:
  [docs/ReleaseBuilds.md](docs/ReleaseBuilds.md)

> **Hinweis:** Die 8-MB- und 16-MB-Builds sind noch nicht auf Hardware getestet
> (kein Testgerät vorhanden). Die 16-MB-Builds sind zusätzlich auf den IRAM-Region-
> Override in `ld/memory_16mb_override.ld` angewiesen.

## Weiterführende Doku

Eine ausführlichere Dokumentation auf Deutsch gibt es auf dieser [Seite](https://itgrufti.de/solar/).

## Original-README

Das vollständige Original-README (Upgrade-Hinweise, Hintergrund, Breaking Changes, unterstützte Wechselrichter) gibt es nur auf Englisch in der [README.md](README.md).
