#!/bin/bash
# OpenDTU release build matrix
#
# Builds all release firmware variants. Each variant is a predefined env
# (rel_*) in platformio.ini - platformio.ini is never modified at runtime.
# Feature/log-level matrix and sizes: docs/FeatureSizes.md.
#
# Artifacts are staged in release/ as opendtu-<name>.bin (app, for OTA) and
# opendtu-<name>.factory.bin (full flash image).
#
# Usage: scripts/release_builds.sh
set -u

cd "$(dirname "$0")/.." || exit 1
PIO="$HOME/.platformio/penv/bin/platformio"
OUT=release

command -v "$PIO" >/dev/null 2>&1 || { echo "ERROR: platformio not found at $PIO"; exit 1; }
command -v npm >/dev/null 2>&1 || { echo "ERROR: npm not found (webapp builds)"; exit 1; }
mkdir -p "$OUT/logs"

LAST_WA=""
wa() { # <VITE_TOST> <VITE_MQTT> <VITE_MDNS>
    local key="$1/$2/$3" safe
    safe=$(echo "$key" | tr '/' '_')
    [ "$key" = "$LAST_WA" ] && return 0
    if ! VITE_TOST=$1 VITE_MQTT=$2 VITE_MDNS=$3 npm --prefix webapp run build \
            >"$OUT/logs/webapp_$safe.log" 2>&1; then
        echo "ERROR: webapp build failed ($key) - see $OUT/logs/webapp_$safe.log"
        return 1
    fi
    LAST_WA=$key
    echo "[webapp] VITE_TOST=$1 VITE_MQTT=$2 VITE_MDNS=$3"
    return 0
}

RESULTS=""
FAILURES=0

# name|env|VITE_TOST VITE_MQTT VITE_MDNS|firmware.bin limit (0 = no check,
# enforced by create_factory_bin.py against the partition table instead)
MATRIX=(
"all-no-eth-mqtt-mdns|rel_all_no_eth_mqtt_mdns|0 0 0|1966080"
"hoymiles-tost|rel_hoymiles_tost|1 1 1|1966080"
"hoymiles-w-tost|rel_hoymiles_w_tost|1 1 1|1966080"
"hiflow-bt-tost-no-mqtt|rel_hiflow_bt_tost_no_mqtt|1 0 1|1966080"
"deye-tost|rel_deye_tost|1 1 1|1966080"
"hoymiles-hoymiles-w-deye-tost-no-mqtt|rel_hoymiles_hoymiles_w_deye_tost_no_mqtt|1 0 1|1966080"
"hoymiles-w-deye-hiflow-bt-tost-no-mqtt|rel_hoymiles_w_deye_hiflow_bt_tost_no_mqtt|1 0 1|1966080"
"no-nrf|rel_no_nrf|0 0 1|1966080"
"no-nrf-bt|rel_no_nrf_bt|0 1 1|1966080"
"all-8mb|rel_all_8mb|1 1 1|0"
"all-no-nrf-8mb|rel_all_no_nrf_8mb|1 1 1|0"
"all-16mb|rel_all_16mb|1 1 1|0"
"all-no-nrf-16mb|rel_all_no_nrf_16mb|1 1 1|0"
)

echo "### OpenDTU release builds - $(date)"
for row in "${MATRIX[@]}"; do
    IFS='|' read -r name envn wv limit <<<"$row"
    read -r wt wm wd <<<"$wv"
    echo "=================================================================="
    echo "BUILD opendtu-$name (env=$envn)"
    wa "$wt" "$wm" "$wd" || exit 1

    if "$PIO" run -e "$envn" >"$OUT/logs/build_$name.log" 2>&1; then
        bin=$(stat -c %s ".pio/build/$envn/firmware.bin")
        if [ "$limit" -gt 0 ] && [ "$bin" -gt "$limit" ]; then
            FAILURES=$((FAILURES + 1))
            RESULTS="$RESULTS
opendtu-$name|SIZE-EXCEEDED|$bin > $limit"
            echo "[$name] firmware.bin=$bin EXCEEDS limit $limit"
            continue
        fi
        cp ".pio/build/$envn/firmware.bin"         "$OUT/opendtu-$name.bin"
        cp ".pio/build/$envn/firmware.factory.bin" "$OUT/opendtu-$name.factory.bin"
        RESULTS="$RESULTS
opendtu-$name|OK|$bin"
        echo "[$name] firmware.bin=$bin -> release/opendtu-$name.{bin,factory.bin}"
    else
        FAILURES=$((FAILURES + 1))
        RESULTS="$RESULTS
opendtu-$name|BUILD-FAILED|-  (see $OUT/logs/build_$name.log)"
        echo "[$name] BUILD FAILED - see $OUT/logs/build_$name.log"
    fi
done

# ---------------------------------------------------------------- summary
echo "=================================================================="
echo "### SUMMARY"
printf "%-45s %s\n" "artifact" "status"
while IFS='|' read -r n st sz; do
    [ -z "$n" ] && continue
    printf "%-45s %s\n" "$n" "$st ($sz)"
done <<<"$RESULTS"
echo "artifacts in: $(pwd)/$OUT"
ls -la "$OUT"/*.bin 2>/dev/null
if [ "$FAILURES" -gt 0 ]; then
    echo "RESULT: $FAILURES FAILURE(S) - see $OUT/logs/"
    exit 1
fi
echo "RESULT: ALL OK"
