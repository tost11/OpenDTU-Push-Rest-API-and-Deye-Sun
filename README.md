# OpenDTU

[![OpenDTU Build](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml)
[![cpplint](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml)

## What's this fork about ?

Currently, I am working on multiple features:

- Push data via rest to extern Application [here](https://github.com/tost11/OpenDTU-Push-Rest-API/tree/feature/push-rest-api)
- Implement Logic to add and Monitor Deye Sun,Bosswerk Inverters, [here](https://github.com/tost11/OpenDTU-Push-Rest-API/tree/feature/deye-sun)
- Both combined, [here](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/push-rest-api)
- manuel startupdate (So opendtu works without npt and manuel setting date after boot,date and time obviously not correct)[here](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/default-start-date)
- servo engine that views solar power (looks like [this](https://itgrufti.de/solar/monitoring/eine-solar-anzeige-fuer-die-kita/)) [here](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/servo)

### Deye Sun Branch

This feature Branch is intended to read out Deye Sun Micro PV Inverters and make use of the original project UI and features

The esp will connect via Network(udp) to the configured ip/hostname and port of the Inverter.
It will read all data every 5 minutes (more is not supported by device)
Reachable check will be done more often.

Original implementation for Hoymiles inverts will work in parallel

Tested with model: SUN300G3-EU-230

#### Working

- Reading data
- Configuring via UI
- Tun on and off (also "resetting" if led permanent red)
- Setting limit
- Showing logs

#### Not working

- Logs show hardware inverter errors
- restart inverter

### Hoymiles W-Series Branch

This feature branch is intended to read out Hoymiles W-Series PV Inverters and make use of the original project UI and features.

The esp will connect via Network(tcp) to the configured ip/hostname and port of the Inverter.

The DTU-Connection code is mostly based on the code of [DTU-Gateway](https://github.com/ohAnd/dtuGateway). I just mapped it to OpenDTU project. So all problems described there with setting limit also exist here.

Original implementation for Hoymiles inverts will work in parallel.

Tested with model: HMS-800W-2T

#### Problems

The only real supported inverter by Serial is currently the 'HMS-800W-2T' because i dont know the other serials-number prefix to recognize them. So every other W-Series inverter will be shown as 4-PV input inverter.

Support for 6T invertes not done yet

#### Working

- Reading data
- Configuring via UI
- Setting limit
- restart inverter

#### Not working

- Showing Logs
- Logs show hardware inverter errors

### Rest push service

I have implemented an application that monitors solar systems on a server
with graphs, statistics and all the cool stuff!

For getting data on this application this fork has a new feature
that sends the current inverter data via rest to the application.

If you are interested in the application or the rest definition for your own application
check out the [project](https://github.com/tost11/solar-monitoring).

### Builds

Check out precompiled builds for dev32 board [here](builds)

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
