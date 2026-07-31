# Build Information

The builds are now stored in [releases](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/releases) on GitHub.

All builds are the generic build for the ESP32 with 4MB flash.

For a description of each feature flag, see the
[Build Flags](../README.md#build-flags) section in the main README.

## Images

### All inverters
Support for all inverter types (Hoymiles NRF24, Hoymiles W-Series, Deye Sun).

Flags: `HOYMILES=1 HOYMILES_W=1 DEYE_SUN=1 TOST=0 SERVO=0`\
File: all_inverters_firmware_*.bin

### No NRF
All network inverters (Hoymiles W-Series, Deye Sun) without the original
Hoymiles NRF24 support. Intended for a DTU box that has no NRF chip, so it is
clear to the user that Hoymiles NRF24 devices cannot be used.

Flags: `HOYMILES=0 HOYMILES_W=1 DEYE_SUN=1 TOST=0 SERVO=0`\
File: no_nrf_firmware_*.bin

### All inverters + REST
All inverter types plus the REST push service.

Flags: `HOYMILES=1 HOYMILES_W=1 DEYE_SUN=1 TOST=1 SERVO=0`\
File: all_inverters_rest_firmware_*.bin

### No NRF + REST
All network inverters (no NRF24) plus the REST push service.

Flags: `HOYMILES=0 HOYMILES_W=1 DEYE_SUN=1 TOST=1 SERVO=0`\
File: no_nrf_rest_firmware_*.bin

### All combined (No NRF)
All network inverters plus the REST push service and servo output.

Flags: `HOYMILES=0 HOYMILES_W=1 DEYE_SUN=1 TOST=1 SERVO=1`\
File: all_combined_no_nrf_firmware_*.bin

## Everything (all flags)

Enabling every feature — including NRF24 support together with servo — does not
fit in 4MB, so this is not provided as a downloadable build.

It can still be built manually by using
[`partitions_custom_4mb one_partition.csv`](../partitions_custom_4mb%20one_partition.csv),
which uses a single large partition and drops over-the-air update support.

Flags: `HOYMILES=1 HOYMILES_W=1 DEYE_SUN=1 TOST=1 SERVO=1`
