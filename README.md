# OpenDTU

[![OpenDTU Build](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml)
[![cpplint](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml)
[![Yarn Prettier](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnprettier.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnprettier.yml)

## What's this fork about ?

This fork extends OpenDTU with several additional features, controlled via build flags in [platformio.ini](platformio.ini). Enable or disable each feature by setting the corresponding flag to `1` or `0`.

### Feature Overview

- **Hoymiles nRF24/CMT2300** — Original Hoymiles inverter support via RF radio
- **Deye SUN** — Deye SUN micro inverters via Wi-Fi/network (UDP, TCP, Modbus)
- **Hoymiles W-Series** — HMS-*W-2T inverters via Wi-Fi/network (TCP, Protobuf)
- **HiFlow BLE (Beta/Testing)** — Hoymiles HiFlow (HMS-*-WB / HF-*-WB) inverters via Bluetooth Low Energy (unfinished)
- **REST Push Service** — Push inverter data to an external monitoring server
- **Servo Engine** — Physical servo output displaying solar power
- **Start Time** — Operate without internet by setting a fallback start time

## Build Flags

All feature flags are set in the `build_flags` section of [platformio.ini](platformio.ini).

| Flag          | Default | Description                                                  |
|---------------|---------|--------------------------------------------------------------|
| `-DHOYMILES`  | `1`     | Support for original Hoymiles NRF24 inverters                |
| `-DDEYE_SUN`  | `1`     | Support for Deye SUN inverters (network, Wi-Fi)              |
| `-DHOYMILES_W`| `1`     | Support for Hoymiles W-Series inverters (network, Wi-Fi)     |
| `-DHIFLOW_BLE`| `0`     | Beta/Testing: Hoymiles HiFlow BLE inverters (ESP32-S3, unfinished) |
| `-DSERVO`     | `0`     | Servo engine output displaying solar power                   |
| `-DTOST`      | `0`     | REST push service sending inverter data to a monitoring server|

> **Note on `-DTOST`:** Changing this flag also requires rebuilding the frontend.
> - `TOST=1`: build normally with `npm run build` inside the `webapp/` directory.
> - `TOST=0`: build with `VITE_TOST=0 npm run build` inside the `webapp/` directory.

## Features

### More manufacturers

With help from this fork, it is possible to read data from more inverter types besides the Hoymiles NRF24 ones.
The additional inverter types will be fetched via Network (Wi-Fi). The esp therefore has to be in the same network as the inverter.

All inverter types are separate implementations and can be enabled or disabled via build flags — see the [Build Flags](#build-flags) section above.

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
| Status of development | reding data and writing    | reding data and writing      | not implemented             |

Tested with models: SUN300G3-EU-230, SUN-M60/80/100G4-EU-Q0, Deye SUN600G3-EU-230

#### Working

- Reading data
- Configuring via UI
- Tun on and off (also "resetting" if led permanent red, for unknown devices it has to be enabled on the DTU/General-Settings page).
- Setting limit (for unknown devices, it has to be enabled on the DTU/General-Settings page)
- Showing logs

#### Not working

- Logs show hardware inverter errors

#### Additional Features
When the Deye sun inverter is not having a connection to the internet (remote server), the daily KWH will not reset. For fixing that on the
OpenDTU view, it is possible to activate the "Deye Sun offline yield day flag" on advanced inverter settings. So the daily KWH will be
tracked and calculated by OpenDTU itself.

### Status Hoymiles W-Series

The esp will connect via Network (TCP, Protobuf) to the configured IP/hostname/MAC (when connected to AP of the esp) and port of the Inverter.

The DTU-Connection code is mostly based on the code of [DTU-Gateway](https://github.com/ohAnd/dtuGateway). I just mapped it to OpenDTU project. So all problems described there with setting limits also exist here.

Tested with model: HMS-800W-2T

It is possible to configure the distance between the data fetches on advanced inverter settings. This is needed because on the first firmware
versions of the device (like 1.\*.\*) it was only possible every 31 seconds (also mentioned on [DTU-Gateway](https://github.com/ohAnd/dtuGateway)). Therefore the
Default value is 31 seconds. With the new firmware (2.\*.\* and above), it is possible more often. 20 seconds seem to be a good value (find out for yourself).

#### Working

- Reading data
- Configuring via UI
- Setting limit
- restart inverter

#### Not working

- Logs show hardware inverter errors
- Read Firmware information of inverter
- Support for 6T invertes not done yet.

####  Network setup as gateway

If you like to disconnect the inverter from the internet without blocking connections on the router (so they don't do any crazy updates like seen lately).
It is possible to set OpenDTU in continuous AP mode by setting time on the network page to zero and connecting your inverters directly to the OpenDTU AP.
Please change the password therefor to something more complex. Then on the network settings for the inverters, the MAC address
can be used for connection. The DTU will resolve them to the correct IP address. The connected device can be found on the network Info page of the DTU.

#### more serial numbers

To get rid of the unknown devices, recognized by the device serial prefix, it would be helpful to know more for them so they can be mapped correctly.
For doing so, feel free to leave a comment with device type and prefix on this [issue](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/6) so they can be added.

### Beta: HiFlow BLE (HMS-\*-WB / HF-\*-WB Series)

> **Status: Beta / Testing / Unfinished** — This feature is under active development on the `feature/hoymiles-hiflow-bluetooth` branch. It is not considered stable or complete.

The ESP32-S3 connects via Bluetooth Low Energy (BLE) to Hoymiles HiFlow inverters (HMS-\*-WB, HF-\*-WB series). No additional radio hardware (nRF24/CMT2300) is required — only the ESP32-S3's built-in BLE radio.

The BLE protocol implementation is based on the reverse engineering work from [hiflow-ble](https://github.com/TheTiEr/hiflow-ble/blob/main/README.md).

#### Build Configuration

Enable HiFlow BLE support with the build flag `-DHIFLOW_BLE=1` in `platformio.ini`.

For a **BLE-only build** (no Hoymiles nRF24/CMT2300 RF support), use the dedicated PlatformIO environment which excludes unused RF libraries to save flash and RAM:

```
platformio run -e generic_esp32s3_hiflow
```

If you want **both Hoymiles nRF24 RF and HiFlow BLE** in the same firmware, you have two options:
- Use an ESP32-S3 board with larger flash (8MB/16MB) and the corresponding partition table (e.g. `partitions_custom_16mb.csv`)
- Or use the single-partition config without OTA update support (`partitions_custom_4mb one_partition.csv`) on a standard 4MB board

#### Working

- Reading real-time data (AC power, PV port voltages/currents/power)
- BLE pairing (V0) and session handshake (V1) with AES encryption
- Automatic reconnect on connection loss
- Configurable poll interval via UI
- PIN configuration via frontend (default: `123456`)

#### Not Working / Limitations

- **Write commands are not implemented** — only reading data is supported (no limit setting, no power on/off)
- **Not all inverter serial prefixes are mapped** — unknown serials fall back to 4-channel mode. Known prefixes: `0x1610`, `0x4161` (2-channel 800W), `0x1164` (4-channel 1600W). If your device shows as unknown or has wrong channel count, please report your serial prefix and model.
- **Data accuracy not fully verified** — values appear correct but have not been cross-checked against the official app for all fields
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

> **Note:** This feature is disabled by default (`-DTOST=0`). When changing the `TOST` flag, the frontend must also be rebuilt — see the [Build Flags](#build-flags) section for details.

### Servo Engine

With this feature, it is possible to add a servo engine to the esp. It can be configured by pin mapping and hardware settings.
Using input 0 zero will result in using the full load of the invertery every other number selects the index of the inputs.

## Builds

Check out precompiled builds for dev32 board [here](builds)

## Further docuementation

Fore some more detailed Documentation in German check out this [page](https://itgrufti.de/solar/).

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
