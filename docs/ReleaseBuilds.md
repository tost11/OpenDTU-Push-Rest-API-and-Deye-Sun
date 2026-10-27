# Release builds

How `scripts/release_builds.sh` produces the release firmware matrix.

## What it does

Builds all 16 release firmware variants (10x 4 MB with OTA, 2x 4 MB without
OTA, 2x 8 MB, 2x 16 MB) and stages the artifacts in `release/` for upload to
a GitHub release.

## Where the build matrix lives

- The rows are in `scripts/release_matrix.tsv`:
  `name  env  vite_tost  vite_mqtt  vite_mdns  vite_ota  firmware.bin size limit`
  (tab-separated, one variant per line). This file is the single source of
  truth - the script and the GitHub Actions workflows both read it.
- Each variant is a **predefined env** (`rel_*`) in the "RELEASE BUILDS"
  section at the bottom of `platformio.ini`. It carries the board, flash
  options and the full compile flag set (feature toggles + log level).
  Common flags are shared via the `[relcommon]` section. The script never
  modifies `platformio.ini` - it just runs `pio run -e <env>`.
- The feature combination, log level and measured size of every variant are
  documented in `docs/FeatureSizes.md` (section "Release build matrix").

## What the script does per build

Each matrix row is:
`name | env | VITE_TOST VITE_MQTT VITE_MDNS VITE_OTA | firmware.bin size limit`

1. **Webapp** - if the row's VITE variant (`VITE_TOST`/`VITE_MQTT`/
   `VITE_MDNS`/`VITE_OTA`, must match the firmware flags) differs from the
   previous build, the webapp is rebuilt first (`npm --prefix webapp run
   build`); the result in `webapp_dist/` is embedded into the firmware. Only
   a handful of distinct variants exist, so the webapp is built a handful of
   times per run. With `RELEASE_WEBAPP_DIR` set, a prebuilt dist from that
   directory is used instead (GitHub Actions builds the variants once in a
   separate job).
2. **Firmware** - `platformio run -e <env>`.
3. **Size check** - builds fail hard if `firmware.bin` exceeds the row's
   limit (4 MB dual-slot: 1,966,080 B; 4 MB single-partition no-OTA:
   3,932,160 B). 8/16 MB builds have no extra check:
   `create_factory_bin.py` already fails the build when the image exceeds the
   app slot of the partition table.
4. **Stage** - copies the artifacts to
    - `release/<prefix><name>.bin` - app image, for OTA updates
    - `release/<prefix><name>.factory.bin` - full flash image
      (bootloader + partitions + app), flash at offset 0

    `no-ota` builds (name contains `no-ota`) only get the `factory.bin` -
    the plain app image is useless without an OTA slot.

   `<prefix>` is `opendtu-` by default, or `RELEASE_NAME_PREFIX` (the CI
   workflow sets it to `opendtu-<tag>-` on tagged runs).

Per-build console output and webapp logs are written to `release/logs/`
(`build_<name>.log`, `webapp_*.log`).

## Result

A SUMMARY table (artifact, status, size) is printed at the end. The exit code
is non-zero if any build failed or exceeded its size limit - safe to run
unattended/in CI.

## Usage

```bash
scripts/release_builds.sh            # all 16 variants, ~40-80 min cold
scripts/release_builds.sh <name>     # only one matrix row (name as in the TSV)
```

Environment:

- `PIO` - path to the platformio binary (default
  `~/.platformio/penv/bin/platformio`, falling back to `platformio` on PATH)
- `RELEASE_WEBAPP_DIR` - dir with a prebuilt webapp dist, skips the npm
  webapp build
- `RELEASE_NAME_PREFIX` - artifact name prefix (default `opendtu-`)

A single variant can also be built directly:
`platformio run -e rel_<name>`.

## GitHub Actions

Three workflows in `.github/workflows/`:

- `release-builds.yml` (reusable) - `webapp` job builds the 7 webapp
  variants once; `canary` job builds the first matrix row
  (`all-no-eth-mqtt-mdns`); `firmware` job builds the remaining 14 rows in
  parallel (one job per row, `fail-fast: false`). Each firmware job uploads
  its bins + build log as a `fw-<name>` artifact. If the canary fails, the
  14 are skipped - a broken toolchain wastes ~15 min instead of ~140.
  Like the original OpenDTU project, PlatformIO is pip-installed on a plain
  Ubuntu runner (`pip install platformio setuptools intelhex`), and
  `~/.platformio` (ESP32 toolchain) and pip are cached with `actions/cache`
  keyed on `platformio.ini`.
- `release.yml` - on tag push `v*`: runs the builds, then `create-release`
  collects all `fw-*` artifacts and creates a **draft** GitHub release
  (versioned artifact names `opendtu-<tag>-<name>.bin` + one `.sha256`
  file). Zero successful builds -> no release. Missing builds are listed in
  the release body; after fixing them, re-run the failed jobs and
  `create-release` to refresh the draft.
- `release-test.yml` - on push to branch `release`: runs the same builds
  without creating a release (test artifacts, plain names).

Retry model: every firmware row is an independent job, so "Re-run failed
jobs" rebuilds only the failed rows; the webapp and canary results are
reused.

Notes:

- The script never commits anything and never touches `platformio.ini`.
- 16 MB + PSRAM builds rely on `ld/memory_16mb_override.ld` (applied by
  `pio-scripts/ld_override_16mb.py`) - see that file for why.
- Flashing/verifying a build on the test device is done manually, not by the
  script (see `.opencode/skills/release-builds/SKILL.md`).
