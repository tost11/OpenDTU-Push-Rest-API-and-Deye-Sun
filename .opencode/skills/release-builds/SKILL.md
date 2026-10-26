---
name: release-builds
description: Build all OpenDTU release firmware variants. Use when the user asks for release builds, release firmwares, building the build matrix, or staging artifacts for a GitHub release.
---

# Release builds

Builds the full release firmware matrix (see `docs/FeatureSizes.md`, section
"Release build matrix") and stages the artifacts in `release/`.

Every variant is a **predefined env** (`rel_*`) at the bottom of
`platformio.ini` (marked "RELEASE BUILDS"). The script never modifies
`platformio.ini` - it just runs `pio run -e <env>` per row. To build a single
variant manually: `platformio run -e rel_<name>`.

```bash
scripts/release_builds.sh    # build all 13 variants + stage artifacts (~40-60 min)
```

Artifacts per build: `release/opendtu-<name>.bin` (app, for OTA) and
`release/opendtu-<name>.factory.bin` (full flash image).
4 MB builds additionally get a hard `firmware.bin` size check against
1,966,080 B (8/16 MB builds are enforced by `create_factory_bin.py` against
the partition table during the build).

## Preflight (check before starting)

1. `webapp/node_modules` present; run `npm --prefix webapp ci` if a webapp
   build fails on missing modules.
2. A dirty `platformio.ini` is fine (it is not modified), but warn about
   uncommitted changes so the user knows the measured sizes refer to their
   working tree.

Run the script from the repo root in a long-running terminal. Expect
~40-60 min (13 firmware builds + up to 5 webapp builds; a first build of a
fresh env compiles from scratch and takes longer).

## Expected output

Per build: `BUILD opendtu-<name>`, then
`firmware.bin=<size> -> release/opendtu-<name>...`
At the end a SUMMARY table; exit code 0 only if everything is OK.

## Failure handling

| Symptom | Meaning / fix |
|---|---|
| `firmware binary too large` / `BUILD-FAILED` | The combo no longer fits the app slot (code grew). Check the measured size vs limit in `docs/FeatureSizes.md`; the known lever is `LOG_LOCAL_LEVEL`/`CORE_DEBUG_LEVEL` 5 -> 4 (saves ~13.5 KB on the all-4 build) - adjust that env's flags in the RELEASE BUILDS section of `platformio.ini` and re-run. |
| `SIZE-EXCEEDED` | Build succeeded but `firmware.bin` > 1,966,080 B on a 4 MB env. Same fix: lower the log level of that env. |
| Webapp build failed | See `release/logs/webapp_*.log`. The VITE flags must match the firmware flags (TOST/MQTT/mDNS) - they are the 3rd column of the matrix rows in the script, don't override them manually. |
| Build fails with unknown `-D` / parse error in flags | Check the RELEASE BUILDS section of `platformio.ini` for a typo; `platformio project config` validates the file. |

## After a successful full run

1. Replace the estimated sizes in `docs/FeatureSizes.md` (section "Release
   build matrix") with the measured `firmware.bin` values from the SUMMARY
   and mark the section as built with the date.
2. Keep `release/` as the staging folder for the GitHub release - the user
   uploads the `.factory.bin` (and optionally `.bin`) files themselves; do not
   create the release unasked.

## Notes

- 16 MB + PSRAM envs need `ld/memory_16mb_override.ld` (applied by
  `pio-scripts/ld_override_16mb.py` for every `*16mb*` env). Without it the
  link fails with `region 'iram0_0_seg' overflowed` - pre-existing issue,
  also affects `generic_esp32_16mb_psram`. Do not remove these two files.
- The script never commits anything and never touches `platformio.ini`.
- The old `build_firmwares.sh` in the repo root is superseded by this flow.
- Flashing/verifying individual builds to the test device is done manually:
  `platformio run -e <env> -t upload` (port from `platformio_override.ini`),
  then ping + `curl http://<ip>/` + `curl http://<ip>/api/inverter`.
