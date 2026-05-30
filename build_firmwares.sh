#!/bin/bash

set -e

TARGET_FOLDER=$(realpath ~/Schreibtisch/opendtu)
BUILD_COMMAND='~/.platformio/penv/bin/platformio run'
#BUILD_COMMAND_16='~/.platformio/penv/bin/platformio run -e generic_esp32_16mb_psram'

echo "Target folder is: $TARGET_FOLDER"


createBuild() {
  #clear existing old build stuff (some times not clear build done)
  rm -Rf .pio/build
  yarn --cwd webapp run build

  eval $BUILD_COMMAND
  echo "Build Generic: $1_esp32dev_firmware.bin copy to: $TARGET_FOLDER"
  mkdir -p $TARGET_FOLDER/generic_esp32
  cp .pio/build/generic_esp32/firmware.bin $TARGET_FOLDER/generic_esp32/$2_esp32dev_firmware.bin

#  eval $BUILD_COMMAND_16
#  echo "Build 16MB psram: $2_esp32dev_firmware.bin copy to: $TARGET_FOLDER"
#  mkdir -p $TARGET_FOLDER/generic_esp32_16mb_psram
#  cp .pio/build/generic_esp32_16mb_psram/firmware.bin $TARGET_FOLDER/generic_esp32_16mb_psram/$2_esp32dev_firmware.bin
}

buildAndCheckout(){
    git checkout $1
    createBuild $1
}

buildAndCheckout "feature/more-manufacturers" "more_manufacturers"
buildAndCheckout "feature/deye-sun" "deye"
buildAndCheckout "feature/hoymiles-HMS-W-2T" "HMS-XXXW-XT"
buildAndCheckout "feature/servo" "servo"
buildAndCheckout "feature/default-start-date" "starttime"
buildAndCheckout "feature/push-rest-api" "rest"
buildAndCheckout "combine/rest-more-manufacturers" "rest_more_manufacturers"
buildAndCheckout "combine/rest-no-nrf-hoymiles" "rest_no_nrf"

git checkout "combine/all"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=1/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=0/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=0/' platformio.ini
createBuild "all_in_one_hoymiles"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=0/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=1/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=0/' platformio.ini
createBuild "all_in_one_deye"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=0/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=0/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=1/' platformio.ini
createBuild "all_in_one_hoymilesW"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=1/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=1/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=0/' platformio.ini
createBuild "all_in_one_hoymiles_deye"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=0/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=1/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=1/' platformio.ini
createBuild "all_in_one_deye_hoymilesW"

sed -i 's/-DHOYMILES=[^ ]*/-DHOYMILES=1/' platformio.ini
sed -i 's/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=0/' platformio.ini
sed -i 's/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=1/' platformio.ini
createBuild "all_in_one_hoymiles_hoymiles_w"

git reset --hard HEAD
echo "Build all images successfully!"