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
| PCB DRC / manufacturing | Not applicable: no routed PCB supplied |

The supplied UF2 is the2026-10-28 warm-reset snapshot with watchdog and key
LED retained. Not every earlier hardware test was repeated on this exact UF2.
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
