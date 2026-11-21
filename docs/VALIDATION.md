# Observed validation and limits

These results come from one repaired ROM01 motherboard and one Core2350B0
reporting SDK silicon revision3 (A3). They are not a compatibility claim for
all IIgs or RP2350 revisions.

| Check | Evidence and scope |
|---|---|
| Native model/input tests | PASS with HAL stubs; command/capability formats, key translation, strobe ACK, modifier release, private RAM retained on host reset |
| Native watchdog policy | PASS: stale detection, no initial progress, idle host, timer/counter wrap |
| Model SELFTEST on board | 0 failures on the supplied warm-reset image |
| Normal startup | User photograph of Check startup device with no boot medium; earlier capability-fix image |
| Full built-in diagnostic | User photograph of System Good; earlier capability-fix image, no repeats in45s read-only observation |
| Physical keyboard | Command press/release seen over USB; arrows/Return operated Control Panel; physical Ctrl-Command-Esc opened Desk Accessories |
| LED | User confirmed press=lit, release=off on LED candidate |
| Watchdog | Actual core0 and core1 hangs caused hardware reset/USB re-enumeration; Apple was OFF during fault tests |
| Powered-host reset | User confirmed keyboard reset worked; subsequent USB status had Sync=1 and watchdog active |
| Warm-reset correction | ROM read RAM51=A5 after host RESET; user confirmed BASIC prompt and BASIC operation |
| Schematic ERC | Rechecked the sharing copy with KiCad9 on2026-10-28:0 violations; original historical report retained |
| PCB Rev2 | Archived four-layer CAD: ERC/DRC, unconnected and parity checks zero; physical fit and bring-up unverified |

The historical 2026-10-28 snapshot is retained. The current UF2 is the
2026-11-21 cold-start image; see the current validation below. The old image
contains warm-reset, watchdog and key LED support. Not every earlier hardware test was repeated on this exact UF2.
Its SHA256 and tool versions are in releases/2026-10-28/manifest.json.

The sharing snapshot was rebuilt locally with SDK2.3.1/GCC12.3 after removing
machine-specific paths from its build scripts. Its newly produced UF2 was
byte-for-byte identical to the observed prebuilt UF2 above. Both direct CMake
commands and the portable PowerShell build wrapper were exercised. Native
tests were rerun from the sharing tree. GitHub-hosted Actions had not run at
the time of local snapshot preparation; its current status is visible on the
repository Actions tab.

## Important bug fixes

1. Capability replies0E/0F are count+IDs. Advertising8/10 IDs while returning
   only ID0 caused normal startup to time out with0911. Only ID0 is now advertised.
2. Raw ADB Control/arrow codes differ from Macintosh Keyboard Driver codes.
   Control is36; arrows are3B..3E; separate right modifiers use7B..7D.
3. Ctrl-Command-Esc must notify the host through C026 status bit5/DATA IRQ.
4. C000 character data is seven bits; C010 must clear the independent strobe.
5. System RESET must preserve UI14 private RAM. Erasing51=A5 caused ROM01
   to invalidate the CPU warm-reset vector and repeatedly search for a disk.

## What is NOT qualified

Mouse, repeat, international tables, IIe matrix, storage/OS boot, RTC retention,
all interrupt cases, long continuous uptime and all motherboard/silicon versions.
The DMA lookup has a finite request count. The synthetic UI14 ROM checksum
responses deliberately prevent System Good from being evidence of complete
Mitsubishi firmware emulation.

Raw logs in this directory are selected development observations, not automated
proof of every hardware property. Motherboard repairs were also necessary:
an open CPU D0 connection was repaired and UE9 was replaced. Their symptoms
should not be treated as evidence that every0911 error is a firmware issue.

## 2026-11-21 firmware and November observations

Current release: releases/2026-11-21/. Strict ARM build passed. On-device
SELFTEST=0, armed=1, ready=1, startup_fix=1, startup hold released and watchdog
healthy. The user confirmed startup with JP1 closed, internal IIgs supply
only, USB disconnected, without the previous fatal error. This is one
reported successful check; three cold starts and keyboard-reset regression
were not explicitly repeated on this exact image.

Earlier November firmware fixed host restarts while USB remains powered,
with three successful user-reported cold boots. Repeated keyboard Power/Reset
reports now yield one reset per press using a 250 ms quiet interval.
Control+Reset enters BASIC as expected; Control+Command+Reset cold-boots.
Some Floppy Emu disk images boot (B-1 Bomber reported working); Pac-Man
produces repeated 2 characters and Tetris also failed. General game/storage
compatibility is not established. Speaker hiss remains unresolved.

Detailed cold-start USB observations are in validation/cold_power_start*.
Firmware success does not qualify physical fit, pin protrusion, bottom
clearance or electrical operation of the separate routed PCB prototypes.

## Core2350B PTH carrier Rev B

The 30 x 30 mm four-layer carrier is now CAD-complete. ERC, DRC, unconnected
and schematic-parity counts are all zero. Independent GPIO/power and drill
checks passed: 44 finished 0.65 mm holes, 17.40 mm row-centre spacing.
Minimum trace 0.15 mm and clearance 0.20 mm. All fitted SMDs are on top.
The bottom pin-1 marker is included in the fabrication set.

No physical board, protruding pin fit or printed-housing clearance has been
tested. Keep at least 4 mm below the ready-made Core module. The available
3D model is an envelope, not manufacturer qualification CAD.

Publication-source ARM rebuild and native model/input/reset-filter/watchdog
regressions passed on 2026-11-21. The native runner explicitly undefines
NDEBUG so assertions remain active with compilers that define it by default.
The publication rebuild has its own hash in the current release manifest;
the unchanged hardware-tested UF2 remains the download image.
