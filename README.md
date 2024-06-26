# OpenDTU

[![OpenDTU Build](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/build.yml)
[![cpplint](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/cpplint.yml)
[![Yarn Linting](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml/badge.svg)](https://github.com/tbnobody/OpenDTU/actions/workflows/yarnlint.yml)

## What's this fork about ?

Currently, I am working on two features:

- Push data via rest to extern Application [here](https://github.com/tost11/OpenDTU-Push-Rest-API/tree/feature/push-rest-api)
- Implement Logic to add and Monitor Deye Sun,Bosswerk Inverters, [here](https://github.com/tost11/OpenDTU-Push-Rest-API/tree/feature/deye-sun)
- Both combined, [here](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/push-rest-api)

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
- Tun on and off (theoretically, setting to off doesn't change anything but working for on (rewiring after red led of death))
- Setting limit
- Showing logs

#### Not working

- Logs show hardware inverter errors
- restart inverter

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

| Model                | Required RF Module | DC Inputs | MPP-Tracker | AC Phases |
| ---------------------| ------------------ | --------- | ----------- | --------- |
| Hoymiles HM-300-1T   | NRF24L01+          | 1         | 1           | 1         |
| Hoymiles HM-350-1T   | NRF24L01+          | 1         | 1           | 1         |
| Hoymiles HM-400-1T   | NRF24L01+          | 1         | 1           | 1         |
| Hoymiles HM-600-2T   | NRF24L01+          | 2         | 2           | 1         |
| Hoymiles HM-700-2T   | NRF24L01+          | 2         | 2           | 1         |
| Hoymiles HM-800-2T   | NRF24L01+          | 2         | 2           | 1         |
| Hoymiles HM-1000-4T  | NRF24L01+          | 4         | 2           | 1         |
| Hoymiles HM-1200-4T  | NRF24L01+          | 4         | 2           | 1         |
| Hoymiles HM-1500-4T  | NRF24L01+          | 4         | 2           | 1         |
| Hoymiles HMS-300-1T  | CMT2300A           | 1         | 1           | 1         |
| Hoymiles HMS-350-1T  | CMT2300A           | 1         | 1           | 1         |
| Hoymiles HMS-400-1T  | CMT2300A           | 1         | 1           | 1         |
| Hoymiles HMS-450-1T  | CMT2300A           | 1         | 1           | 1         |
| Hoymiles HMS-500-1T  | CMT2300A           | 1         | 1           | 1         |
| Hoymiles HMS-600-2T  | CMT2300A           | 2         | 2           | 1         |
| Hoymiles HMS-700-2T  | CMT2300A           | 2         | 2           | 1         |
| Hoymiles HMS-800-2T  | CMT2300A           | 2         | 2           | 1         |
| Hoymiles HMS-900-2T  | CMT2300A           | 2         | 2           | 1         |
| Hoymiles HMS-1000-2T | CMT2300A           | 2         | 2           | 1         |
| Hoymiles HMS-1600-4T | CMT2300A           | 4         | 4           | 1         |
| Hoymiles HMS-1800-4T | CMT2300A           | 4         | 4           | 1         |
| Hoymiles HMS-2000-4T | CMT2300A           | 4         | 4           | 1         |
| Hoymiles HMT-1600-4T | CMT2300A           | 4         | 2           | 3         |
| Hoymiles HMT-1800-4T | CMT2300A           | 4         | 2           | 3         |
| Hoymiles HMT-2000-4T | CMT2300A           | 4         | 2           | 3         |
| Hoymiles HMT-1800-6T | CMT2300A           | 6         | 3           | 3         |
| Hoymiles HMT-2250-6T | CMT2300A           | 6         | 3           | 3         |
| Solenso SOL-H350     | NRF24L01+          | 1         | 1           | 1         |
| Solenso SOL-H400     | NRF24L01+          | 1         | 1           | 1         |
| Solenso SOL-H800     | NRF24L01+          | 2         | 2           | 1         |
| TSUN TSOL-M350       | NRF24L01+          | 1         | 1           | 1         |
| TSUN TSOL-M800       | NRF24L01+          | 2         | 2           | 1         |
| TSUN TSOL-M1600      | NRF24L01+          | 4         | 2           | 1         |
