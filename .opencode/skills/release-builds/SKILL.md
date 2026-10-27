---
name: release-builds
description: Build, flash and verify all OpenDTU release firmware variants. Use when the user asks for release builds, release firmwares, building the build matrix, flashing/verifying builds to the test device, or staging artifacts for a GitHub release.
---

# Release builds

Builds the full release firmware matrix (see `docs/FeatureSizes.md`, section
"Release build matrix") step by step: for every variant it builds, size-checks,
stages the artifacts in `release/`, and (4 MB builds only) flashes + verifies
on the test device. Work through the builds **one at a time and report one
status line after each** so failures surface early.

Every variant is a **predefined env** (`rel_*`) in the "RELEASE BUILDS"
section at the bottom of `platformio.ini`; common flags live in the
`[relcommon]` section there. `platformio.ini` is never modified at runtime.

The build matrix (one row per variant: `name | env | VITE_TOST VITE_MQTT
VITE_MDNS VITE_OTA | firmware.bin size limit`, tab-separated) is in
`scripts/release_matrix.tsv` - read it from there, do not copy it here. The
build script `scripts/release_builds.sh` exists for **unattended build-only runs** (CI / no device);
see `docs/ReleaseBuilds.md`. Do not run it in this interactive flow.

## Preflight (check before starting)

1. `webapp/node_modules` present; run `yarn --cwd webapp install
   --frozen-lockfile` if a webapp build fails on missing modules.
2. USB device present: `ls /dev/ttyUSB*`. If missing: **stop and ask the user
   to plug in the test device** - do not continue build-only unless they say so.
3. Test device reachable: `ping -c1 -W2 192.168.66.109` (default IP). If not
   reachable: **immediately ask the user for the current IP** - one short
   question, no IP scans, no guessing, no further probing.
4. Warn if `platformio.ini` has uncommitted changes, so the user knows the
   measured sizes refer to their working tree.

## Per-build loop (in MATRIX order)

For each row `name | env | VITE_TOST VITE_MQTT VITE_MDNS VITE_OTA | limit`:

1. **Webapp** - only if the row's VITE variant differs from the variant of
   the last built row:
     `VITE_TOST=<t> VITE_MQTT=<m> VITE_MDNS=<d> VITE_OTA=<o> yarn --cwd webapp build`
   (VITE flags must match the firmware flags - take them from the row, do not
   override manually).
2. **Firmware** - `platformio run -e <env>` (bash timeout ~10 min; first build
   of a fresh env takes longest).
3. **Size check** (4 MB rows only, `limit` > 0):
   `stat -c %s .pio/build/<env>/firmware.bin` must be <= the limit
   (1,966,080 B), else report SIZE-EXCEEDED and stop. 8/16 MB rows: no extra
   check - `create_factory_bin.py` already fails the build if the image
   exceeds the app slot.
   **Distrust fast builds (< 1 min):** PlatformIO can report SUCCESS
   without actually producing/updating artifacts. Before trusting, check
   `ls -la .pio/build/<env>/firmware.bin` (file must exist, mtime must be
   newer than the webapp build) and that the embedded webapp matches
   `webapp_dist` (must print EMBED-MATCHES-CURRENT):
   `python3 -c "d=open('.pio/build/<env>/app.js.gz.txt.o','rb').read();
   i=d.find(b'\x1f\x8b\x08'); w=open('webapp_dist/js/app.js.gz','rb').read();
   print('EMBED-MATCHES-CURRENT' if d[i:i+len(w)]==w else 'EMBED-DIFFERS')"`
   If the file is missing or stale: re-run `platformio run -e <env>` once and
   re-check; if it still fails, stop.
4. **Stage** - copy to
   `release/opendtu-<name>.bin` (app, for OTA) and
   `release/opendtu-<name>.factory.bin` (full flash image, offset 0).
5. **Flash + verify** (4 MB rows ONLY - there is no 8/16 MB test device,
   never flash those):
   - `platformio run -e <env> -t upload` (port from the user-local
     `platformio_override.ini`), log to `release/logs/upload_<name>.log`.
     **Retry up to 3 attempts with a 5 s pause** - esptool sync / baud-MHz /
     transient USB errors are common and usually clear on retry.
   - Wait ~30 s for boot, then verify: `ping -c3 -W2 <ip>`,
     `curl -s -m10 -o /dev/null -w "%{http_code}" http://<ip>/` must be 200,
     same for `http://<ip>/api/inverter`.
6. **Report one line**: `opendtu-<name> | <size> B | flash ok/fail | verify ok/fail`

Stop on the first failure that retries/diagnosis cannot resolve; do not
continue with the remaining builds.

## After the loop

1. Flash the standard build (first MATRIX row) last, so the device is left
   running a known-good image, and verify it.
2. Print a summary table (name, size, flash, verify).
3. If a `firmware.bin` size differs from the table in `docs/FeatureSizes.md`
   (section "Release build matrix"), update that measured value and the date.
4. Keep `release/` as the staging folder for the GitHub release - the user
   uploads the artifacts themselves; **do not create the release unasked**.

## Failure handling

| Symptom | Meaning / fix |
|---|---|
| `firmware binary too large` / `BUILD-FAILED` | The combo no longer fits the app slot (code grew). Known lever: `LOG_LOCAL_LEVEL`/`CORE_DEBUG_LEVEL` 5 -> 4 in that env (saves ~13.5 KB on the all-4 build). Common flags affect all 16 envs - they live in `[relcommon]`, feature flags per env. Adjust in the RELEASE BUILDS section of `platformio.ini`, rebuild that env, continue. |
| `SIZE-EXCEEDED` | Build succeeded but `firmware.bin` > limit on a 4 MB env. Same fix. |
| `upload attempt N failed` (x3) | Transient esptool sync / baud-MHz / USB issue. Check `ls /dev/ttyUSB*` - the port can re-enumerate `ttyUSB0` -> `ttyUSB1` after resets; if so, update the user-local `platformio_override.ini` to the new port and re-run. Otherwise re-plug the device. |
| `VERIFY FAILED: no ping answer` after upload | Device did not boot. Common cause: an oversized `firmware.bin` flashed earlier (bootloader SW-reset loop). Re-flash the known-good `release/opendtu-all-no-eth-mqtt-mdns.factory.bin`. |
| `web root returned 000/4xx/5xx` | App booted but web server misbehaves - read the serial log: `stty -F /dev/ttyUSB0 115200 raw -echo; timeout 30 cat /dev/ttyUSB0`. `platformio device monitor` does NOT work in the Bash tool (no TTY). |
| Webapp build failed | See its output. The VITE flags must match the firmware flags (TOST/MQTT/mDNS) - they are part of the matrix rows, don't override them manually. |
| Build fails with unknown `-D` / parse error in flags | Check the RELEASE BUILDS section of `platformio.ini` for a typo; `platformio project config` validates the file. |

## Notes

- 16 MB + PSRAM envs need `ld/memory_16mb_override.ld` (applied by
  `pio-scripts/ld_override_16mb.py` for every `*16mb*` env). Without it the
  link fails with `region 'iram0_0_seg' overflowed` - pre-existing issue,
  also affects `generic_esp32_16mb_psram`. Do not remove these two files.
- The script and the flow never commit anything and never touch
  `platformio.ini`.
- A full run takes ~40-80 min (16 firmware builds + up to 7 webapp builds +
  10 flash/verify cycles; a fresh env compiles from scratch).
