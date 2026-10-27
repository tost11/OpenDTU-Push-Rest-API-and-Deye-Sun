# OpenDTU

🇬🇧 **English** | [🇩🇪 Deutsch](README_DE.md)

[![cpplint](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnlint.yml)
[![Yarn Prettier](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnprettier.yml/badge.svg)](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/actions/workflows/yarnprettier.yml)

## What's this fork about ?

This fork of OpenDTU supports Deye Sun (Wi-Fi), Hoymiles W-Series (Wi-Fi) and Hoymiles WB-Series (Bluetooth) inverters in addition to the original Hoymiles nRF24 inverters.
It extends OpenDTU with several additional features, controlled via build flags in [platformio.ini](platformio.ini). Enable or disable each feature by setting the corresponding flag to `1` or `0`.

### New Features

- **Deye SUN** — Deye SUN micro inverters via Wi-Fi/network (UDP, TCP, Modbus)
- **Hoymiles W-Series** — HMS-*W inverters (1T / 2T / 4T) via Wi-Fi/network (TCP, Protobuf)
- **HiFlow BLE** — Hoymiles HiFlow (HMS-\*-WB / HF-\*-WB) inverters via Bluetooth Low Energy (ESP32-S3)
- **REST Push Service** — Push inverter data to an external monitoring server
- **Servo Engine** — Physical servo output displaying solar power
- **Start Time** — Operate without internet by setting a fallback start time

### Preexisting Features (made optional to disable)

- **Hoymiles nRF24/CMT2300** — Original Hoymiles inverter support via RF radio
- **MQTT** — Publish inverter data to an MQTT broker
- **Ethernet (W5500)** — Wired network support for DTU boxes with a W5500 chip
- **mDNS** — LAN name resolution for inverter hostnames

## Build Flags

Not all features fit into the 2 MB OTA firmware, so they are controlled by build flags — enabling only what you need keeps the firmware small enough for web-based OTA updates.
All feature flags are set in the `build_flags` section of [platformio.ini](platformio.ini).

| Flag             | Default | Description                                                  |
|------------------|---------|--------------------------------------------------------------|
| `-DHOYMILES`     | `1`     | Support for original Hoymiles NRF24 inverters                |
| `-DDEYE_SUN`     | `1`     | Support for Deye SUN inverters (network, Wi-Fi)              |
| `-DHOYMILES_W`   | `1`     | Support for Hoymiles W-Series inverters (network, Wi-Fi)     |
| `-DHIFLOW_BLE`   | `1`     | Support for Hoymiles HiFlow BLE inverters (ESP32-S3)         |
| `-DSERVO`        | `0`     | Servo engine output displaying solar power                   |
| `-DTOST`         | `0`     | REST push service sending inverter data to a monitoring server|
| `-DMQTT`         | `0`     | Publish inverter data to an MQTT broker                      |
| `-DENABLE_W5500` | `0`     | Wired Ethernet via the W5500 chip                            |
| `-DENABLE_MDNS`  | `0`     | mDNS LAN name resolution                                     |

> **Note on the frontend build flags:** The frontend is compiled with the Vite
> flags `VITE_TOST`, `VITE_MQTT` and `VITE_MDNS`, which must match the firmware
> flags `-DTOST`, `-DMQTT` and `-DENABLE_MDNS`. The webapp is built with
> `yarn build` inside the `webapp/` directory
> (e.g. `VITE_TOST=1 VITE_MQTT=1 VITE_MDNS=1 yarn build`).

## Features

### More manufacturers

With help from this fork, it is possible to read data from more inverter types besides the Hoymiles NRF24 ones.
The additional inverter types will be fetched via Network (Wi-Fi) or Bluetooth. For the Wi-Fi inverters the esp has to be in the same network as the inverter.

All inverter types are separate implementations and can be enabled or disabled via build flags — see the [Build Flags](#build-flags) section above.

> **Note on device models:** Not all inverters are mapped to the correct model by their
> serial number yet. Devices with an unknown serial are shown as unknown (or fall back to
> a default model), but the model can be overridden manually in the inverter settings.
> If your device is unknown or mapped incorrectly, it would be nice to add its serial and
> model on [issue #6](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/6).
> The serial numbers mapped in the code are listed in
> [docs/known_inverter_serials.md](docs/known_inverter_serials.md).

### Status Deye Sun

The esp will connect via Network ((UDP, 48899), (TCP, 8899, Modbus) to the configured IP/hostname/MAC (when connected to AP of esp) and port of the Inverter.
It will read all data every 5 minutes. Reading data more often is not possible due to the limitations of inverters.
Health checks will be done more often. It is configurable on advanced inverter settings.

The current firmware version and restart command is triggered via HTTP-Rest call. Username and password for that can be
configured in advanced device settings.

Currently, there exist three types of connections to the inverter, depending on the installed Firmware.

|                       | At-Commands                | Custom Modbus                | Modbus                      |
|-----------------------|----------------------------|------------------------------|-----------------------------|
| Firmware              | Old Firmwares (1.0 - 2.32) | new firmware e.g. 1.0B, 5.0C | not seen on micro inverters |
| Status of development | reading data and writing   | reading data and writing     | not implemented             |

Tested with models: SUN300G3-EU-230, SUN-M60/80/100G4-EU-Q0, Deye SUN600G3-EU-230

#### Working

- Reading data
- Reading inverter firmware version
- Turn on and off (also "resetting" if led permanent red, for unknown devices it has to be enabled on the DTU/General-Settings page).
- Setting limit (for unknown devices, it has to be enabled on the DTU/General-Settings page)
- Restart inverter

#### Not working

- Logs show hardware inverter errors

#### Additional Features
When the Deye sun inverter is not having a connection to the internet (remote server), the daily KWH will not reset. For fixing that on the
OpenDTU view, it is possible to activate the "Deye Sun offline yield day flag" on advanced inverter settings. So the daily KWH will be
tracked and calculated by OpenDTU itself.

### Status Hoymiles W-Series

The esp will connect via Network (TCP, Protobuf) to the configured IP/hostname/MAC (when connected to AP of the esp) and port of the Inverter.

The DTU-Connection is based on the command set of the [hoymiles-wifi](https://github.com/suaveolent/hoymiles-wifi) project
(constants and commands ported from its Python implementation).

> **New inverter firmware is not supported yet:** Inverter/DTU firmware v1.01.01 and above does not work at the moment —
> see [issue #12](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/12).
> The new firmware uses a new protocol with encryption, which is not implemented yet (planned for a later release).

Tested with model: HMS-800W-2T

It is possible to configure the distance between the data fetches on advanced inverter settings. This is needed because on the first firmware
versions of the device (like 1.\*.\*) it was only possible every 31 seconds. Therefore the
Default value is 31 seconds. With the new firmware (2.\*.\* and above), it is possible more often. 20 seconds seem to be a good value (find out for yourself).

#### Working

- Reading data
- Reading inverter and DTU firmware version
- Setting limit
- Power on/off
- Restart inverter
- Restart DTU

#### Not working

- Logs show hardware inverter errors
- Support for 6T inverters not done yet.

####  Network setup as gateway

If you like to disconnect the inverter from the internet without blocking connections on the router (so they don't do any crazy updates like seen lately).
It is possible to set OpenDTU in continuous AP mode by setting time on the network page to zero and connecting your inverters directly to the OpenDTU AP.
Please change the password therefor to something more complex. Then on the network settings for the inverters, the MAC address
can be used for connection. The DTU will resolve them to the correct IP address. The connected device can be found on the network Info page of the DTU.
A more detailed description of this setup is available in this [blog post](https://itgrufti.de/solar/monitoring/opendtu-als-gateway-zum-wechselrichter/) (in German).

### HiFlow BLE (HMS-\*-WB / HF-\*-WB Series)

The ESP32-S3 connects via Bluetooth Low Energy (BLE) to Hoymiles HiFlow inverters (HMS-\*-WB, HF-\*-WB series). No additional radio hardware (nRF24/CMT2300) is required — only the ESP32-S3's built-in BLE radio.

The BLE protocol implementation is based on the reverse engineering work from [hiflow-ble](https://github.com/TheTiEr/hiflow-ble/blob/main/README.md).

Tested with models: HMS-800-2WB (serial prefix 0x1610, 0x4161), HF-800-WB (serial prefix 0x1650)

#### Build Configuration

Enable HiFlow BLE support with the build flag `-DHIFLOW_BLE=1` in `platformio.ini`.

For a **BLE-only build** (no Hoymiles nRF24/CMT2300 RF support), use the dedicated PlatformIO environment which excludes unused RF libraries to save flash and RAM:

```
platformio run -e generic_esp32s3_hiflow
```

If you want **both Hoymiles nRF24 RF and HiFlow BLE** in the same firmware, you have two options:
- Use an ESP32-S3 board with larger flash (8MB/16MB) and the corresponding partition table (e.g. `partitions_custom_16mb.csv`)
- Or use the single-partition config without OTA update support (`partitions_custom_4mb one_partition.csv`) on a standard 4MB board - the ready-made `all-4mb-no-ota` release build does exactly this with all features enabled

#### Working

- Reading real-time data (AC power, PV port voltages/currents/power)
- Reading inverter and DTU firmware version (from the V0 pairing response)
- Setting limit
- Power on/off
- Restart inverter
- Restart DTU
- BLE pairing (V0) and session handshake (V1) with AES encryption
- PIN configuration via frontend (default: `123456` — does not work on all devices; in that case the PIN must first be changed with the official Hoymiles app)

#### Not Working / Limitations

- Logs show hardware inverter errors
- Requires ESP32-S3 (BLE 5.0 capable)
- Requires NTP time sync before connection (inverter rejects stale timestamps)

### Start Time

With this feature, it is possible to use OpenDTU in places with no active internet connection. OpenDTU will only fetch data from inverters 
if the time is set correctly. So on a restart or power loss, it won't work until someone manually sets the time. 
With this feature, a default time can be set on the NTP settings page on which the inverter will start. The date will obviously not be correct, 
but at least the DTU shows some data.

### Rest push service

I have implemented an application that monitors solar systems on a server with graphs, statistics and all the cool stuff.
OpenDTU is not capable due to the esp limitations.

For getting data on this application, this fork has a new feature that sends the current inverter data via rest to the application.
Data is sent over plain HTTP — HTTPS is intentionally avoided due to the large TLS heap overhead on the ESP32 — but confidentiality is ensured through AES-256-GCM symmetric encryption applied to the payload before transmission.

If you are interested in the application or the Rest definition for your own application
check out the [project](https://github.com/tost11/solar-monitoring).
A public instance is available at [solar.pihost.org](https://solar.pihost.org),
and a setup guide in German can be found [here](https://itgrufti.de/solar/monitoring/online-monitoring-fuer-opendtu-und-weitere-systeme/).

> **Note:** This feature is disabled by default (`-DTOST=0`). When changing the `TOST` flag, the frontend must also be rebuilt — see the [Build Flags](#build-flags) section for details.

### Servo Engine

With this feature, it is possible to add a servo engine to the esp. It can be configured by pin mapping and hardware settings.
Using input 0 zero will result in using the full load of the invertery every other number selects the index of the inputs.

An example project (solar power display for a kindergarten, in German) can be found in this [blog post](https://itgrufti.de/solar/monitoring/eine-solar-anzeige-fuer-die-kita/).

## Builds

Precompiled firmware builds for all supported feature combinations are provided
as [GitHub releases](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/releases)
(see also [builds](builds)). There are 16 variants: 10 for 4 MB boards (dual slot,
web OTA update), 2 for 4 MB boards without OTA (single partition, all features),
2 for 8 MB boards and 2 for 16 MB boards.

The 4 MB no-OTA builds contain all features in a single app partition, so the
firmware can only be updated by flashing the `factory.bin` via USB serial -
the firmware upgrade page of the web UI shows a notice with instructions.

**Flashing (no OTA):** connect the DTU via USB serial and flash the
`factory.bin` at offset `0x0` - e.g.
`esptool.py --port /dev/ttyUSB0 write_flash 0x0 <name>.factory.bin` or any
web-based esptool flasher. The `.factory.bin` always contains bootloader +
partition table + app; use the plain `.bin` only for OTA updates.

- Complete matrix with features, add-ons, log level and measured sizes:
  [docs/FeatureSizes.md](docs/FeatureSizes.md) (section "Release build matrix")
- How the builds are produced and how the CI workflow works:
  [docs/ReleaseBuilds.md](docs/ReleaseBuilds.md)

> **Caveat:** The 8 MB and 16 MB builds are not tested on hardware yet (no test
> device available). The 16 MB builds additionally rely on the IRAM region
> override in `ld/memory_16mb_override.ld`.

## Further documentation

For some more detailed Documentation in German check out this [page](https://itgrufti.de/solar/).

# OpenDTU original README

## !! IMPORTANT UPGRADE NOTES !!

If you are upgrading from a version before 15.03.2023 you have to upgrade the partition table of the ESP32. Please follow the [this](docs/UpgradePartition.md) documentation!

## Background

This project was started from [this](https://www.mikrocontroller.net/topic/525778) discussion (Mikrocontroller.net).
It was the goal to replace the original Hoymiles DTU (Telemetry Gateway) with their cloud access. With a lot of reverse engineering the Hoymiles protocol was decrypted and analyzed.

## Documentation

The documentation can be found [here](https://tbnobody.github.io/OpenDTU-docs/).
Please feel free to support and create a PR in [this](https://github.com/tbnobody/OpenDTU-docs) repository to make the documentation even better.

## Breaking changes

Generated using: `git log --date=short --pretty=format:"* %h%x09%ad%x09%s" | grep BREAKING`

```code
* 8cab3335      2025-08-07      BREAKING CHANGE: WebAPI endpoint `/api/limit/config` requires different parameters
* 8372deaf      2025-04-18      BREAKING CHANGE: Logging newline changed from "\r\n" to "\n"
* 1b637f08      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* e1564780      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* f0b5542c      2024-01-30      BREAKING CHANGE: Web API Endpoint /api/livedata/status and /api/prometheus/metrics
* c27ecc36      2024-01-29      BREAKING CHANGE: Web API Endpoint /api/livedata/status
* 71d1b3b       2023-11-07      BREAKING CHANGE: Home Assistant Auto Discovery to new naming scheme
* 04f62e0       2023-04-20      BREAKING CHANGE: Web API Endpoint /api/eventlog/status no nested serial object
* 59f43a8       2023-04-17      BREAKING CHANGE: Web API Endpoint /api/devinfo/status requires GET parameter inv=
* 318136d       2023-03-15      BREAKING CHANGE: Updated partition table: Make sure you have a configuration backup and completly reflash the device!
* 3b7aef6       2023-02-13      BREAKING CHANGE: Web API!
* d4c838a       2023-02-06      BREAKING CHANGE: Prometheus API!
* daf847e       2022-11-14      BREAKING CHANGE: Removed deprecated config parsing method
* 69b675b       2022-11-01      BREAKING CHANGE: Structure WebAPI /api/livedata/status changed
* 27ed4e3       2022-10-31      BREAKING: Change power factor from percent value to value between 0 and 1
```

## Currently supported Inverters

A list of all currently supported inverters can be found [here](https://www.opendtu.solar/hardware/inverter_overview/)
