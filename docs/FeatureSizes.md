# Firmware Feature Sizes (2 MB app partition)

Which combinations of features fit into the 2 MB app slot (`0x1E0000` = 1,966,080 B).
Measured 2026-10-25 on env `generic_esp32` (platform espressif32@6.10.0).

## How the numbers were produced

- **Metric:** size of `firmware.bin` (the file `create_factory_bin.py` enforces).
  Do NOT use the PlatformIO `Flash: x%` number - that is the ELF size and is
  ~6.6 KB smaller than `firmware.bin` (section alignment). The ELF number can
  say "fits" while `firmware.bin` overruns the partition.
- **Log level:** all values measured with `-DLOG_LOCAL_LEVEL=5 -DCORE_DEBUG_LEVEL=5`
  (full detail). Tight builds can be pulled under the limit by dropping to
  level 4 (see [Log level lever](#log-level-lever)).
- **Webapp:** the webapp is embedded into the firmware (~214 KB, part of BASE).
  Features with a webapp card (TOST, MQTT, mDNS) have their card rebuild
  included in the measured cost.
- **BLE flags:** `MYNEWT_VAL_BLE_SM_LEGACY=0`, `MYNEWT_VAL_BLE_SM_SC=0` and
  `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=0`, `CONFIG_BT_NIMBLE_ROLE_BROADCASTER=0`
  are always on (no-op unless HIFLOW_BLE is enabled).
- **Feature costs are additive in this table, but the formula is conservative:**
  shared dependencies (e.g. the vendored HTTPClient used by both DEYE_SUN and
  TOST) are counted more than once. Verified against real builds the formula
  overpredicts by 8-79 KB. **Anything within ~75 KB of the limit must be
  verified with a real build** (build it, check `firmware.bin`).

To build a combination: set the flags in `[env] build_flags` in `platformio.ini`
and rebuild the webapp with matching `VITE_TOST` / `VITE_MQTT` / `VITE_MDNS`
for the TOST / MQTT / ENABLE_MDNS firmware flags.

## BASE and measured feature costs (log level 5)

BASE (no features at all): **1,448,080 B**

| Feature | Flag | Cost (B) | Notes |
|---|---|---:|---|
| Hoymiles (RF) | `-DHOYMILES=1` | 107,824 | nRF24 / RF24 |
| Deye (WiFi) | `-DDEYE_SUN=1` | 112,240 | AT + custom modbus, incl. shared HTTP client |
| Hoymiles (WiFi) | `-DHOYMILES_W=1` | 59,296 | nanopb |
| Hoymiles (BLE) | `-DHIFLOW_BLE=1` | 298,896 | NimBLE (SM disabled), protobuf |
| TOST push | `-DTOST=1` | 68,432 | vendored HTTPClient (no TLS) + webapp card |
| MQTT | `-DMQTT=1` | 215,376 | espMqttClient (incl. TLS) + webapp card |
| Servo | `-DSERVO=1` | 8,928 | |
| Ethernet | `-DENABLE_W5500=1` | 36,144 | W5500 (LAN8720) via esp_eth; flag was renamed from `W5500` which collided with the `W5500` class name and did not compile |
| mDNS | `-DENABLE_MDNS=1` | 28,512 | + webapp card |

Formula: `estimated(combo) = 1,448,080 + sum of feature costs`

## Verified combinations (measured firmware.bin)

| Combination | log 5 (B) | log 4 (B) | Status |
|---|---:|---:|---|
| (none) | 1,448,080 | - | fits |
| HOYMILES | 1,555,904 | - | fits (410 KB free) |
| DEYE_SUN | 1,560,320 | - | fits (406 KB free) |
| HOYMILES_W | 1,507,376 | - | fits (459 KB free) |
| HIFLOW_BLE | 1,746,976 | - | fits (219 KB free) |
| TOST | 1,516,512 | - | fits (449 KB free) |
| MQTT | 1,663,456 | - | fits (302 KB free) |
| SERVO | 1,457,008 | - | fits (509 KB free) |
| W5500 | 1,484,224 | - | fits (482 KB free) |
| MDNS | 1,476,592 | - | fits (489 KB free) |
| H + D + BLE | 1,932,704 | - | fits (33,376 B free) |
| BLE + MQTT | 1,954,640 | - | fits (11,440 B free) |
| H + W + BLE + TOST | 1,946,192 | - | fits (19,888 B free) |
| D + W + BLE + TOST | 1,908,096 | - | fits (57,984 B free) |
| H + D + W + MQTT | 1,898,160 | - | fits (67,920 B free) |
| H + D + W + BLE (all 4) | **1,971,856 (5,776 over)** | 1,958,336 | needs log 4 (7,744 B free) |
| H + D + W + BLE + SERVO (all 4) | **over (ELF 1,972,745)** | 1,965,760 | needs log 4, only 320 B free |

H = HOYMILES (RF), D = DEYE_SUN, W = HOYMILES_W, BLE = HIFLOW_BLE.

## Calculated combinations (formula, conservative)

| Combination | Estimated (B) | Margin (B) | Status |
|---|---:|---:|---|
| H + D + W | 1,727,440 | 238,640 | fits |
| H + D + W + TOST | 1,795,872 | 170,208 | fits |
| BLE + TOST | 1,815,408 | 150,672 | fits |
| H + W + MQTT | 1,830,576 | 135,504 | fits |
| D + W + MQTT | 1,834,992 | 131,088 | fits |
| H + D + MQTT | 1,883,520 | 82,560 | fits (verify if critical) |
| D + W + TOST | 1,628,752 | 337,328 | fits |
| H + D + W + MDNS | 1,755,952 | 210,128 | fits |
| H + W + BLE | 1,914,096 | 51,984 | fits (verify if critical) |
| D + W + BLE | 1,918,512 | 47,568 | fits (verify if critical) |
| D + W + BLE + W5500 | 1,954,656 | 11,424 | tight - verify |
| H + W + BLE + W5500 | 1,950,240 | 15,840 | tight - verify |
| H + D + BLE + MDNS | 1,995,552 | -29,472 | formula says over; anchored on the measured H+D+BLE build it is 1,961,216 (fits, 4,864 B) - verify |

## Not possible (over limit, also at log level 4)

| Combination | Estimated (B) | Why |
|---|---:|---|
| H + D + W + BLE + TOST | 2,094,768 | ~61 KB over at log 4 |
| H + D + W + BLE + W5500 | 2,064,224 | ~28 KB over at log 4 |
| H + D + W + BLE + MDNS | 2,054,592 | ~21 KB over at log 4 |
| H + D + BLE + TOST | 2,035,744 | ~56 KB over at log 4 |
| BLE + TOST + MQTT | 2,030,784 | over |
| H + D + W + TOST + MQTT | 2,011,248 | ~31 KB over at log 4 |
| H + D + W + BLE + MQTT | 2,241,712 | over |
| BLE + MQTT + TOST + MDNS etc. (any 3 add-ons on BLE) | - | over |
| all features | 2,383,728 | over |

Rule of thumb: **MQTT only fits with at most 2 non-BLE backends** (or BLE +
nothing else), and **TOST + MQTT together only fit with at most 2 backends**.

## Log level lever

| Level pair | Effect on size (measured on all-4 build) |
|---|---|
| `LOG_LOCAL_LEVEL=5` + `CORE_DEBUG_LEVEL=5` | full detail (baseline for this doc) |
| `LOG_LOCAL_LEVEL=4` + `CORE_DEBUG_LEVEL=4` | saves 13,520 B (all-4: 1,971,856 -> 1,958,336); compiles out `ESP_LOGV` only |
| `LOG_LOCAL_LEVEL=3` + ... | compiles out `ESP_LOGD` as well - not measured, last resort |

`LOG_LOCAL_LEVEL` is the per-file compile-time ceiling for project code (the
size driver); `CORE_DEBUG_LEVEL` is the arduino-core level and the fallback for
files that do not define their own `LOG_LOCAL_LEVEL`. The runtime per-device
log filter (web UI) works regardless of the compile level - levels above the
compiled one are simply empty.

## Release build matrix

Firmware variants offered as downloads (GitHub release). Each variant is a
predefined, self-contained env in the "RELEASE BUILDS" section at the bottom
of `platformio.ini` (never modified at runtime).

- Build everything: `scripts/release_builds.sh` (stages artifacts in `release/`)
- Build one: `platformio run -e <env>`

**Status: built and verified 2026-10-26 - all sizes below are measured
`firmware.bin` values from a full `scripts/release_builds.sh` run.**
Every build must pass the hard `firmware.bin` <= app-slot check in
`create_factory_bin.py`.

16 MB + PSRAM builds need the IRAM region override in
`ld/memory_16mb_override.ld` (applied by `pio-scripts/ld_override_16mb.py`)
- without it the link fails with `iram0_0_seg overflowed` (pre-existing
issue, also affects `generic_esp32_16mb_psram`).

Naming: MQTT is the default for TOST builds - builds without MQTT carry a
`-no-mqtt` suffix. `no-nrf` = no RF Hoymiles backend (boards without the
nRF24 module, where that type cannot work anyway). Flash size is part of the
name for 8/16 MB builds because the `factory.bin` contains the partition
table - flashing the wrong size to a board bricks it.

### 4 MB flash boards (dual slot, web OTA, app slot 1,966,080 B)

| Artifact | Env | Backend(s) | Add-ons | Log | Est. size (B) | Margin (B) |
|---|---|---|---|---|---:|---:|
| `opendtu-all-no-eth-mqtt-mdns` | `rel_all_no_eth_mqtt_mdns` | H + D + W + BLE | - | 4 | 1,958,448 | 7,632 |
| `opendtu-hoymiles-tost` | `rel_hoymiles_tost` | H | TOST SERVO MDNS W5500 MQTT | 5 | 1,879,264 | 86,816 |
| `opendtu-hoymiles-w-tost` | `rel_hoymiles_w_tost` | W | TOST SERVO MDNS W5500 MQTT | 5 | 1,852,688 | 113,392 |
| `opendtu-hiflow-bt-tost-no-mqtt` | `rel_hiflow_bt_tost_no_mqtt` | BLE | TOST SERVO MDNS W5500 | 5 | 1,885,248 | 80,832 |
| `opendtu-deye-tost` | `rel_deye_tost` | D | TOST SERVO MDNS W5500 MQTT | 5 | 1,867,376 | 98,704 |
| `opendtu-hoymiles-hoymiles-w-deye-tost-no-mqtt` | `rel_hoymiles_hoymiles_w_deye_tost_no_mqtt` | H + D + W | TOST SERVO MDNS W5500 | 5 | 1,770,624 | 195,456 |
| `opendtu-hoymiles-w-deye-hiflow-bt-tost-no-mqtt` | `rel_hoymiles_w_deye_hiflow_bt_tost_no_mqtt` | D + W + BLE | TOST SERVO MDNS (no W5500 - does not fit even at log 4) | 5 | 1,944,656 | 21,424 |
| `opendtu-no-nrf` | `rel_no_nrf` | D + W + BLE | MDNS W5500 (no TOST/SERVO) | 5 | 1,945,296 | 20,784 |
| `opendtu-no-nrf-bt` | `rel_no_nrf_bt` | D + W | MDNS W5500 MQTT (no BLE) | 5 | 1,871,168 | 94,912 |

### 8 MB flash boards (dual slot, web OTA, app slot 4,034,560 B)

`partitions_custom_8mb.csv`, qio, **no PSRAM** - runs on both flash-only and
PSRAM 8 MB modules.

| Artifact | Env | Backend(s) | Add-ons | Log | Est. size (B) | Margin (B) |
|---|---|---|---|---|---:|---:|
| `opendtu-all-8mb` | `rel_all_8mb` | H + D + W + BLE | TOST SERVO MDNS W5500 MQTT (all 9 features) | 5 | 2,249,168 | 1,785,392 |
| `opendtu-all-no-nrf-8mb` | `rel_all_no_nrf_8mb` | D + W + BLE | TOST SERVO MDNS W5500 MQTT (all except RF) | 5 | 2,179,856 | 1,854,704 |

### 16 MB flash boards (dual slot, web OTA, app slot 8,257,536 B)

qio + PSRAM (`partitions_custom_16mb.csv`).

| Artifact | Env | Backend(s) | Add-ons | Log | Est. size (B) | Margin (B) |
|---|---|---|---|---|---:|---:|
| `opendtu-all-16mb` | `rel_all_16mb` | H + D + W + BLE | all 9 features | 5 | 2,297,552 | 5,959,984 |
| `opendtu-all-no-nrf-16mb` | `rel_all_no_nrf_16mb` | D + W + BLE | all except RF | 5 | 2,226,160 | 6,031,376 |

### Not offered, and why

- **`generic` env (original OpenDTU board, hardcoded nRF pins):** covered by
  the `generic_esp32` builds - the user configures the 6 nRF pins once in the
  Web UI (pin mapping). Add later if original-board users of this fork appear.
- **Olimex POE/EVB, WT32-ETH01 (`OPENDTU_ETHERNET`):** built-in EMAC Ethernet
  is a different, untested code path; niche boards.
- **C3 / S3 / S2 envs:** different chip families, untested in this fork, no
  demand. The `generic_esp32s3_hiflow` env is a dev env (4 MB single-partition
  table = no web OTA), not a release target.
- **4 MB single-partition "everything" build:** would need to drop web OTA;
  the 8/16 MB all-features builds fill that role with OTA intact.

Artifacts per build: `opendtu-<name>.bin` (app, for OTA) and
`opendtu-<name>.factory.bin` (full flash image incl. bootloader/partitions,
flash at offset 0). Webapp must be built with `VITE_TOST`/`VITE_MQTT`/
`VITE_MDNS` matching the firmware flags for each variant.

## Caveats

- Flashed a too-large `firmware.bin` (image overruns the partition) = bootloader
  rejects the image = SW-reset boot loop, no app output. Verify `firmware.bin`
  <= 1,966,080 **before** flashing.
- Formula estimates overpredict (8-79 KB observed) - they are safe for
  "fits comfortably" but not for "fits exactly"; build to verify.
- Webapp state matters: building the webapp with extra cards (TOST/MQTT/mDNS)
  adds a few KB to `firmware.bin`; always rebuild it to match the firmware flags.
