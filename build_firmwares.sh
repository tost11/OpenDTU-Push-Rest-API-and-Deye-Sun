#!/bin/bash
set -e

TARGET_FOLDER=$(realpath ~/Schreibtisch/opendtu)
PIO=~/.platformio/penv/bin/platformio
#ENVS="generic_esp32 generic_esp32s3 generic_esp32s3_usb"
ENVS="generic_esp32"

echo "Target folder is: $TARGET_FOLDER"
echo "Building for environments: $ENVS"
echo ""

# ─────────────────────────────────────────────────────────────────────────────
# Step 1: Build both frontend variants once
# ─────────────────────────────────────────────────────────────────────────────
echo "=== Building frontend (TOST enabled) ==="
yarn --cwd webapp run build
cp -r webapp_dist webapp_dist_tost

echo "=== Building frontend (TOST disabled) ==="
VITE_TOST=0 yarn --cwd webapp run build
cp -r webapp_dist webapp_dist_notost

# ─────────────────────────────────────────────────────────────────────────────
# Step 2: Build helper
# ─────────────────────────────────────────────────────────────────────────────
build_image() {
  local name=$1 hoymiles=$2 hoymiles_w=$3 deye=$4 tost=$5 servo=$6

  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "  Building: $name"
  echo "  HOYMILES=$hoymiles  HOYMILES_W=$hoymiles_w  DEYE_SUN=$deye  TOST=$tost  SERVO=$servo"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

  # Patch build flags in platformio.ini
  sed -i "s/-DHOYMILES=[^ ]*/-DHOYMILES=$hoymiles/" platformio.ini
  sed -i "s/-DHOYMILES_W=[^ ]*/-DHOYMILES_W=$hoymiles_w/" platformio.ini
  sed -i "s/-DDEYE_SUN=[^ ]*/-DDEYE_SUN=$deye/" platformio.ini
  sed -i "s/-DTOST=[^ ]*/-DTOST=$tost/" platformio.ini
  sed -i "s/-DSERVO=[^ ]*/-DSERVO=$servo/" platformio.ini

  # Swap in the correct frontend
  rm -rf webapp_dist
  if [ "$tost" = "1" ]; then
    cp -r webapp_dist_tost webapp_dist
  else
    cp -r webapp_dist_notost webapp_dist
  fi

  # Clean previous build artifacts
  rm -rf .pio/build

  # Build all target environments
  for env in $ENVS; do
    echo "  → Building environment: $env"
    $PIO run -e "$env"

    mkdir -p "$TARGET_FOLDER/$env"
    cp ".pio/build/$env/firmware.bin" \
       "$TARGET_FOLDER/$env/${name}_firmware.bin"
    echo "  ✓ Copied: $TARGET_FOLDER/$env/${name}_firmware.bin"
  done
}

# ─────────────────────────────────────────────────────────────────────────────
# Step 3: Build all 6 images
# ─────────────────────────────────────────────────────────────────────────────
#                name                      H  HW  D  T  S
build_image "all_inverters"                1  1   1  0  0
build_image "no_nrf"                       0  1   1  0  0
build_image "all_inverters_rest"           1  1   1  1  0
build_image "no_nrf_rest"                  0  1   1  1  0
#build_image "all_combined"                 1  1   1  1  1
build_image "all_combined_no_nrf"          0  1   1  1  1

# ─────────────────────────────────────────────────────────────────────────────
# Step 4: Cleanup
# ─────────────────────────────────────────────────────────────────────────────
git checkout platformio.ini
rm -rf webapp_dist && cp -r webapp_dist_tost webapp_dist
rm -rf webapp_dist_tost webapp_dist_notost

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  All 6 images built successfully!"
echo "  Output: $TARGET_FOLDER"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
