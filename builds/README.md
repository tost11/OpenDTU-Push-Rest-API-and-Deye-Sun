# Build Information

All the build in this folder are the generic build for the Esp with 4MB Ram. More builds for other Platforms can be found in the subfolders.

## Features

### Deye Sun
This build includes additional support for the Deye Sun micro inverters (SUN300G3-EU-230, SUN600G3-EU-230 and so on).

Branch: [feature/deye-sun](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/deye-sun)\
Build: [deye_esp32dev_firmware.bin](deye_esp32dev_firmware.bin)

### Hoymiels W-Series
This build includes additional support for the Hoymiles Wireless micro inverters (HMS-800W-2T and so on)

Branch: [feature/hoymiles-HMS-W-2T](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/hoymiles-HMS-W-2T)\
Build: [HMS-XXXW-XT_esp32dev_firmware.bin](HMS-XXXW-XT_esp32dev_firmware.bin)

### More manufacturers
This build includes additional support for the Hoymiles Wireless micro inverters (HMS-800W-2T and so on) and the Deye Sun micro inverters (SUN300G3-EU-230, SUN600G3-EU-230 and so on).

Branch: [feature/more-manufacturers](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/more-manufacturers)\
Build: [more_manufacturers_esp32dev_firmware.bin](more_manufacturers_esp32dev_firmware.bin)

### Rest push service
This build includes the feature of pushing data to a configurable rest endpoint.

Branch: [feature/push-rest-api](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/push-rest-api)\
Build: [rest_esp32dev_firmware.bin](rest_esp32dev_firmware.bin)

### Default start time
This build includes the feature of setting default start time to use openDTU on places without Internet and NTP connections.
It has a configurable start time so the time will not be correct but fetching data will work after restart.

Branch: [feature/default-start-date](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/default-start-date)\
Build: [starttime_esp32dev_firmware.bin](starttime_esp32dev_firmware.bin)

### Servo
This build includes the feature of adding a servo engine to show current energy production. It is configurable by pin mapping and hardware settings.

Branch: [feature/servo](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/feature/servo)\
Build: [servo_esp32dev_firmware.bin](servo_esp32dev_firmware.bin)

## Combinations

### Rest and more manufacturers
Combination of features: feature/more-manufacturers and feature/push-rest-api

Branch: [combine/rest-more-manufacturers](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/combine/rest-more-manufacturers)\
Build: [rest_more_manufacturers_esp32dev_firmware.bin](rest_more_manufacturers_esp32dev_firmware.bin)

### No NRF
Combination of features: feature/more-manufacturers and feature/push-rest-api without the original Hoymiles inverters enabled.
Perfect for an esp without the NRF wireless modul. So it is directly clear to the user not Hoymiles NRF devices are possible to use.

Branch: [combine/rest-no-nrf-hoymiles](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/combine/rest-no-nrf-hoymiles)\
Build: [rest_no_nrf_esp32dev_firmware.bin](rest_no_nrf_esp32dev_firmware.bin)

### Everything
This build combines all the implemented features.

Branch: [combine/all](https://github.com/tost11/OpenDTU-Push-Rest-API-and-Deye-Sun/tree/combine/all)\
Build: [all_in_one_esp32dev_firmware.bin](all_in_one_esp32dev_firmware.bin)