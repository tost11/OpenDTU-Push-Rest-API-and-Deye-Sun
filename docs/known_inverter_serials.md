# Known inverter serials

Serial number prefixes that the firmware maps to a device model.

> Not all inverters are mapped to the correct model yet. Devices with an unknown
> serial are shown as unknown (or fall back to a default model); the model can be
> overridden manually in the inverter settings. If your device is unknown or mapped
> incorrectly, it would be nice to add its serial and model on
> [issue #6](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/issues/6).

## Deye Sun

First characters of the serial number.

| Serial Prefix | Device |
|:-------------:|:------:|
| 415, 414, 221 | SUN600G3-EU-230 |
| 413, 411 | SUN300G3-EU-230 |
| 384, 385, 386 | SUN-M60/80/100G4-EU-Q0 |

Source: `lib/DeyeSun/src/inverters/DeyeInverter.cpp` (`serialToModel`).

## Hoymiles W-Series (Wi-Fi)

First 4 hex digits of the serial number.

| Serial Prefix | Model |
|:-------------:|:-----:|
| 0x1122, 0x1123, 0x1124, 0x1125, 0x1403 | HMS-\*-W-1T |
| 0x1014, 0x1141, 0x1142, 0x1144, 0x1410, 0x1412, 0x2821 | HMS-\*-W-2T |
| 0x1151, 0x1162, 0x1164, 0x1222 | HMS-\*-W-4T |

Source: `lib/Hoymiles-W/src/inverters/HMS_W_1T.cpp`, `HMS_W_2T.cpp`, `HMS_W_4T.cpp`
(`isValidSerial`).

## HiFlow BLE (HMS-\*-WB / HF-\*-WB)

First 4 hex digits of the serial number. The serial determines the channel
count, but the power output is not known from the serial alone — only the HF
series is fixed at 800 W.

| Serial Prefix | Model |
|:-------------:|:-----:|
| 0x1650 | HF-800-WB |
| 0x1610, 0x4161 | HMS-\*-2WB |
| 0x1164 | HMS-\*-4WB |
| - | HMS-\*-1WB |

Source: `lib/HiFlowBLE/src/inverters/HF_800_WB.cpp`, `HF_2CH.cpp`, `HF_4CH.cpp`,
`HF_1CH.cpp` (`isValidSerial`).

## Hoymiles nRF24/CMT2300

See the class overview table (models + serial ranges) in
`lib/Hoymiles/src/inverters/README.md`.
