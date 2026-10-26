# Extends the IRAM code region for 16 MB + PSRAM builds.
# What and why: see ld/memory_16mb_override.ld.
#
# This must be a `post:` script: the arduino framework appends its own
# linker scripts (-T memory.ld, ...) to LINKFLAGS during build setup and a
# LATER -T definition wins. Flags given in build_flags are placed BEFORE
# the framework's -T scripts and therefore have no effect. Appending here
# puts the override last.

Import("env")

env = DefaultEnvironment()

if "16mb" in env.get("PIOENV", ""):
    env.Append(
        LINKFLAGS=["-T", env.subst("$PROJECT_DIR") + "/ld/memory_16mb_override.ld"]
    )
