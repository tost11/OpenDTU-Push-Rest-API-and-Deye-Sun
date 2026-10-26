/*
 * IRAM code region override for 16 MB + PSRAM builds
 *
 * WHY:
 * The framework's memory.ld (tools/sdk/esp32/ld/memory.ld in
 * framework-arduinoespressif32) limits `iram0_0_seg` - the region that holds
 * all IRAM_ATTR code (WiFi, BLE/NimBLE, FreeRTOS, SPI/RTC drivers, PSRAM
 * init) - to 0x20000 (128 KB). With PSRAM enabled the total IRAM_ATTR code
 * of this project exceeds that by ~2 KB and the link fails with
 * "region `iram0_0_seg' overflowed".
 *
 * The 128 KB limit is a conservative choice of the IDF 4.x layout; the
 * framework's own comment says "Not sure if happy with this, this is MMU
 * area". The region above it (0x400A0000-0x400C0000) is plain IRAM SRAM:
 * ESP-IDF 3.x / arduino-esp32 1.x defined iram0_0_seg as 0x40000 (256 KB)
 * ending exactly at the 64 KB instruction cache (0x400C0000), which is the
 * layout every ESP32 board with WiFi+BLE+PSRAM has run in production.
 *
 * HOW IT WORKS:
 * The arduino build links with several -T scripts
 * (rom.redefined, memory.ld, sections.ld, ...). GNU ld merges the MEMORY
 * blocks of all -T files in order, a later definition of a region wins, so
 * this file must be passed AFTER the framework's scripts. The post build
 * script pio-scripts/ld_override_16mb.py appends "-T <this file>" to
 * LINKFLAGS for every *16mb* env (a flag in build_flags would land before
 * the framework's scripts and be ignored). sections.ld places .iram0.text
 * "into iram0_0_seg" by name, so it simply gets the larger region.
 *
 * AFFECTED ENVIRONMENTS (see platformio.ini):
 *   generic_esp32_16mb_psram (dev), rel_all_16mb, rel_all_no_nrf_16mb
 * 4 MB / 8 MB envs do NOT use this file - their IRAM_ATTR code fits the
 * default 128 KB region.
 */
MEMORY
{
  iram0_0_seg (RX) : org = 0x40080000, len = 0x40000
}
