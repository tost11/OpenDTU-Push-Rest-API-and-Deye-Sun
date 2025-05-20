#!/bin/bash

set -e

TARGET_FOLDER=$(realpath ~/Schreibtisch/opendtu)
BUILD_COMMAND='~/.platformio/penv/bin/platformio run'
BUILD_COMMAND_16='~/.platformio/penv/bin/platformio run -e generic_esp32_16mb_psram'

echo "Target folder is: $TARGET_FOLDER"

createBuild() {
  git checkout $1
  #clear existing old build stuff (some times not clear build done)
  rm -Rf .pio/build
  npm run build --prefix webapp

  eval $BUILD_COMMAND
  echo "Build: $2_esp32dev_firmware.bin copy to: $TARGET_FOLDER"
  cp -p .pio/build/generic_esp32/firmware.bin $TARGET_FOLDER/generic_esp32/$2_esp32dev_firmware.bin

  eval $BUILD_COMMAND_16
  echo "Build: $2_esp32dev_firmware.bin copy to: $TARGET_FOLDER"
  cp -p .pio/build/generic_esp32_16mb_psram/firmware.bin $TARGET_FOLDER/generic_esp32_16mb_psram/$2_esp32dev_firmware.bin
}

createBuild "feature/more-manufacturers" "more_manufacturers"
createBuild "feature/deye-sun" "deye"
createBuild "feature/hoymiles-HMS-W-2T" "HMS-XXXW-XT"
createBuild "feature/servo" "servo"
createBuild "feature/default-start-date" "starttime"
createBuild "feature/push-rest-api" "rest"
createBuild "combine/rest-more-manufacturers" "rest_more_manufacturers"
createBuild "combine/rest-no-nrf-hoymiles" "rest_no_nrf"
createBuild "combine/all" "all_in_one"