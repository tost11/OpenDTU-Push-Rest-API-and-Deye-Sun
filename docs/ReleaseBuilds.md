# Release builds

How `scripts/release_builds.sh` produces the release firmware matrix.

## What it does

Builds all 13 release firmware variants (9x 4 MB, 2x 8 MB, 2x 16 MB) and
stages the artifacts in `release/` for upload to a GitHub release.

## Where the build matrix lives

- Each variant is a **predefined env** (`rel_*`) in the "RELEASE BUILDS"
  section at the bottom of `platformio.ini`. It carries the board, flash
  options and the full compile flag set (feature toggles + log level).
  Common flags are shared via the `[relcommon]` section. The script never
  modifies `platformio.ini` - it just runs `pio run -e <env>`.
- The feature combination, log level and measured size of every variant are
  documented in `docs/FeatureSizes.md` (section "Release build matrix").

## What the script does per build

Each matrix row in the script is:
`name | env | VITE_TOST VITE_MQTT VITE_MDNS | firmware.bin size limit`

1. **Webapp** - if the row's VITE variant (`VITE_TOST`/`VITE_MQTT`/`VITE_MDNS`,
   must match the firmware flags) differs from the previous build, the webapp
   is rebuilt first (`npm --prefix webapp run build`); the result in
   `webapp_dist/` is embedded into the firmware. Only ~5 distinct variants
   exist, so the webapp is built a handful of times per run.
2. **Firmware** - `platformio run -e <env>`.
3. **Size check** - 4 MB builds fail hard if `firmware.bin` > 1,966,080 B.
   8/16 MB builds have no extra check: `create_factory_bin.py` already fails
   the build when the image exceeds the app slot of the partition table.
4. **Stage** - copies the artifacts to
   - `release/opendtu-<name>.bin` - app image, for OTA updates
   - `release/opendtu-<name>.factory.bin` - full flash image
     (bootloader + partitions + app), flash at offset 0

Per-build console output and webapp logs are written to `release/logs/`
(`build_<name>.log`, `webapp_*.log`).

## Result

A SUMMARY table (artifact, status, size) is printed at the end. The exit code
is non-zero if any build failed or exceeded its size limit - safe to run
unattended/in CI.

## Usage

```bash
scripts/release_builds.sh    # from the repo root, ~30-60 min for a cold run
```

A single variant can also be built directly:
`platformio run -e rel_<name>`.

Notes:

- The script never commits anything and never touches `platformio.ini`.
- 16 MB + PSRAM builds rely on `ld/memory_16mb_override.ld` (applied by
  `pio-scripts/ld_override_16mb.py`) - see that file for why.
- Flashing/verifying a build on the test device is done manually, not by the
  script (see `.opencode/skills/release-builds/SKILL.md`).
