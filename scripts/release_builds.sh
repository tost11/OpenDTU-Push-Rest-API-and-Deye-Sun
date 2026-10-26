#!/bin/bash
# OpenDTU release build matrix
#
# Builds the release firmware variants listed in scripts/release_matrix.tsv.
# Each variant is a predefined env (rel_*) in platformio.ini - platformio.ini
# is never modified at runtime.
# Feature/log-level matrix and sizes: docs/FeatureSizes.md.
#
# Artifacts are staged in release/ as <prefix><name>.bin (app, for OTA) and
# <prefix><name>.factory.bin (full flash image).
#
# Usage: scripts/release_builds.sh [name]
#   [name]   build only the matrix row with this name (used by GitHub Actions,
#            where each matrix row runs in its own job)
#
# Environment:
#   PIO                  path to the platformio binary
#                        (default: ~/.platformio/penv/bin/platformio,
#                        falling back to 'platformio' on PATH)
#   RELEASE_WEBAPP_DIR   dir with a prebuilt webapp dist (webapp CI job) -
#                        skip the npm webapp build and use it as webapp_dist
#   RELEASE_NAME_PREFIX  artifact name prefix (default: opendtu-)
set -u

cd "$(dirname "$0")/.." || exit 1
OUT=release
MATRIX_FILE=scripts/release_matrix.tsv
NAME_PREFIX="${RELEASE_NAME_PREFIX:-opendtu-}"
ONLY_NAME="${1:-}"

PIO="${PIO:-$HOME/.platformio/penv/bin/platformio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$(command -v platformio || true)"
[ -n "$PIO" ] || { echo "ERROR: platformio not found"; exit 1; }
[ -f "$MATRIX_FILE" ] || { echo "ERROR: $MATRIX_FILE not found"; exit 1; }
mkdir -p "$OUT/logs"

LAST_WA=""
wa() { # <VITE_TOST> <VITE_MQTT> <VITE_MDNS>
    local key="$1/$2/$3" safe
    safe=$(echo "$key" | tr '/' '_')
    [ "$key" = "$LAST_WA" ] && return 0
    if [ -n "${RELEASE_WEBAPP_DIR:-}" ]; then
        if [ ! -f "$RELEASE_WEBAPP_DIR/js/app.js.gz" ]; then
            echo "ERROR: prebuilt webapp dir $RELEASE_WEBAPP_DIR incomplete"
            return 1
        fi
        cp -r "$RELEASE_WEBAPP_DIR" webapp_dist.new
        rm -rf webapp_dist
        mv webapp_dist.new webapp_dist
        echo "[webapp] using prebuilt $RELEASE_WEBAPP_DIR ($key)"
    else
        command -v npm >/dev/null 2>&1 || { echo "ERROR: npm not found (webapp builds)"; return 1; }
        if ! VITE_TOST=$1 VITE_MQTT=$2 VITE_MDNS=$3 npm --prefix webapp run build \
                >"$OUT/logs/webapp_$safe.log" 2>&1; then
            echo "ERROR: webapp build failed ($key) - see $OUT/logs/webapp_$safe.log"
            return 1
        fi
        echo "[webapp] VITE_TOST=$1 VITE_MQTT=$2 VITE_MDNS=$3"
    fi
    LAST_WA=$key
    return 0
}

# header: name<TAB>env<TAB>vite_tost<TAB>vite_mqtt<TAB>vite_mdns<TAB>limit
# limit: firmware.bin size limit (0 = no check, enforced by
#        create_factory_bin.py against the partition table instead)
ROWS="$(tail -n +2 "$MATRIX_FILE")"
[ -n "$ONLY_NAME" ] && ROWS="$(grep -P "^\Q$ONLY_NAME\E\t" <<<"$ROWS" || true)"
[ -n "$ROWS" ] || { echo "ERROR: no matrix row named '$ONLY_NAME' in $MATRIX_FILE"; exit 1; }

RESULTS=""
FAILURES=0

echo "### OpenDTU release builds - $(date)"
while IFS=$'\t' read -r name envn wt wm wd limit; do
    [ -z "$name" ] && continue
    echo "=================================================================="
    echo "BUILD ${NAME_PREFIX}${name} (env=$envn)"
    wa "$wt" "$wm" "$wd" || exit 1

    if "$PIO" run -e "$envn" >"$OUT/logs/build_$name.log" 2>&1; then
        bin=$(stat -c %s ".pio/build/$envn/firmware.bin")
        if [ "$limit" -gt 0 ] && [ "$bin" -gt "$limit" ]; then
            FAILURES=$((FAILURES + 1))
            RESULTS="$RESULTS
${NAME_PREFIX}${name}|SIZE-EXCEEDED|$bin > $limit"
            echo "[$name] firmware.bin=$bin EXCEEDS limit $limit"
            continue
        fi
        cp ".pio/build/$envn/firmware.bin"         "$OUT/${NAME_PREFIX}${name}.bin"
        cp ".pio/build/$envn/firmware.factory.bin" "$OUT/${NAME_PREFIX}${name}.factory.bin"
        RESULTS="$RESULTS
${NAME_PREFIX}${name}|OK|$bin"
        echo "[$name] firmware.bin=$bin -> release/${NAME_PREFIX}${name}.{bin,factory.bin}"
    else
        FAILURES=$((FAILURES + 1))
        RESULTS="$RESULTS
${NAME_PREFIX}${name}|BUILD-FAILED|-  (see $OUT/logs/build_$name.log)"
        echo "[$name] BUILD FAILED - see $OUT/logs/build_$name.log"
    fi
done <<<"$ROWS"

# ---------------------------------------------------------------- summary
echo "=================================================================="
echo "### SUMMARY"
printf "%-55s %s\n" "artifact" "status"
while IFS='|' read -r n st sz; do
    [ -z "$n" ] && continue
    printf "%-55s %s\n" "$n" "$st ($sz)"
done <<<"$RESULTS"
echo "artifacts in: $(pwd)/$OUT"
ls -la "$OUT"/*.bin 2>/dev/null
if [ "$FAILURES" -gt 0 ]; then
    echo "RESULT: $FAILURES FAILURE(S) - see $OUT/logs/"
    exit 1
fi
echo "RESULT: ALL OK"
