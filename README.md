# Apple IIgs UI12/UI14 replacement on RP2350

[Русское описание](README.ru.md) · [Wiring](hardware/WIRING.md) · [Validation](docs/VALIDATION.md)

Maintainer: [Mamorup](https://github.com/Mamorup).

An experimental, open-source replacement for the Apple IIgs **UI12 ADB GLU**
and **UI14 keyboard/ADB microcontroller**, using one Waveshare Core2350B0 module.
It implements the motherboard interface and a behavioral ADB controller, not
an instruction-level Mitsubishi MCU emulator. Original Apple ROM images are
not required by this firmware and are not distributed here.

**Status: working hardware prototype, 2026-10-28.** One ROM01 IIgs has reached
Applesoft BASIC and accepted ADB keyboard input. This is not a fully qualified,
drop-in production replacement. The KiCad files are a wiring schematic;
there is no routed PCB or set of manufacturing files in this repository.

## Hardware used

- Waveshare **Core2350B0**; the connected RP2350B reports SDK silicon revision3
  (A3). The module product name does not identify its silicon stepping.
- Series resistors:330ohm for D0..D7, 220ohm for the other wired logic signals.
- Two external open-drain/open-collector drivers for IRQ and system RESET.
  The schematic uses2N7002; a verified NPN circuit can be used instead.
- The motherboard's original ADB Q4/R108/R107 circuit remains in place.
- A separate working RTC is required. The tested machine used an ATtiny85
  RTC replacement; this repository does not contain that separate project.

See [hardware/WIRING.md](hardware/WIRING.md) before connecting the board.
This prototype uses powered RP2350 FT digital GPIOs directly; it is not a
generic3.3V/5V interface design. A2 silicon is not qualified by this project.
Power Core from USB **before** powering the IIgs; switch IIgs off first.
Keep JP1 open during USB power. Never connect IIgs5V to Core3V3.

## Features and observed results

- Combined GLU and controller behavior, with PIO/DMA serving host reads and
  a second CPU core handling host writes.
- Valid capability replies for US character set/layout ID0.
- Physical ADB Command press/release, arrows, Return and
  Control+Command+Escape menu invocation were confirmed.
- Warm RESET preserves UI14 private RAM, including the51=A5 signature;
  Applesoft BASIC entry and subsequent BASIC operation were confirmed.
- GPIO39 LED lights while any decoded key is held and goes out on release.
- Hardware watchdog:2s timeout; core0 feeds only with fresh core1 progress.
  Deliberate hangs of both cores caused real watchdog restarts in bench tests.
- After a watchdog timeout boot, one4ms system RESET is issued through the
  external driver after re-ARM to allow the IIgs to synchronize again.

An earlier candidate reached **System Good**. The full diagnostic was not
rerun on every later image. See the per-image [validation record](docs/VALIDATION.md).

## Limitations

- Mouse, key repeat, international layouts, storage boot and full timing/IRQ
  coverage are not qualified. Only US layout/character set ID0 is advertised.
- IIe keyboard matrix scanning is not implemented.
- UI14 ROM reads return synthetic checksum-compatibility data:72/F7 at
  1400/1401 and zeros elsewhere in1000..1FFF. **System Good does not prove
  instruction-level or complete behavioral equivalence to the original MCU.**
- The read-lookup DMA currently has a finite0x0FFFFFFF request count. Continuous
  long-run operation and the behavior at exhaustion have not been qualified.
- Normal startup requires a boot device to load an OS. Without one,
  `Check startup device!` is expected. Control+Power/Reset can enter BASIC.

## Prebuilt firmware

The hardware-tested snapshot is [releases/2026-10-28/iigs_combined.uf2](releases/2026-10-28/iigs_combined.uf2).
Its SHA256 is:

```text
62d56b3345ebd8212ddd36b94d185cb3a6a15da60e8561c5a0ac06be8aba6cca
```

Enter the RP2350 UF2 bootloader and copy the UF2 to its USB storage volume.
When the existing firmware is accessible through USB CDC, send `DISARM`,
then `BOOT` to enter the bootloader. GPIOs start as inputs; after selftest
passes, ARM and ADB polling start automatically. If Core was restarted while
IIgs was already running, issue a system RESET so the host initializes it.
Do not connect a programmer or change wiring with the motherboard powered.

## Build from source

Requirements: Git, Python3, CMake>=3.20, Ninja and an Arm GNU Embedded
toolchain (`arm-none-eabi-gcc` in PATH or `PICO_TOOLCHAIN_PATH`). The observed
hardware image was built with SDK2.3.1 and GCC12.3.

Fetch the SDK separately; it is not vendored in this repository:

```sh
git clone --branch 2.3.1 --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git external/pico-sdk
cmake -S firmware -B firmware/build-rp2350 -G Ninja -DPICO_BOARD=waveshare_core2350b
cmake --build firmware/build-rp2350
python firmware/pack_uf2.py
```

SDK2.3.1 commit used: `079c6f39023649b154152db30f1d781e884879bc`.
Alternatively set `PICO_SDK_PATH` to an existing SDK checkout.
Windows users can run `firmware/build.ps1`; it uses environment variables
and PATH, with no machine-specific compiler path.
The newly built UF2 is `firmware/build-rp2350/iigs_combined.uf2`. Compiler
versions/build paths can change binary hashes; the supplied manifest names
the observed prebuilt snapshot, not every subsequent build.

## USB diagnostics

The port is discovered by the OS; it is not always the same COM number.
Optional console: `python tools/usb_console.py STATUS` (requires pyserial).

| Command | Meaning |
|---|---|
| `STATUS`, `PINS`, `INPUT`, `WDOG`, `LED` | Read state, key history and watchdog health |
| `SELFTEST` | Portable model regression check |
| `RESET` | Pulse the external system RESET driver for4ms |
| `ARM` / `DISARM` | Enable emulator / release GPIO0..23 |
| `POLL ON` / `POLL OFF` | Enable/disable physical ADB polling |
| `KEY xx` | Inject a raw ADB transition code; bit7 means release |
| `TRACE`, `CMDSTATS` | Controller command/reply diagnostics |
| `BOOT` | Enter UF2 mode; requires DISARM |

`BUSTEST` and `BUSWRITE` drive host control pins: **never use them with an
IIgs motherboard connected**. `WDOG TEST CORE0/CORE1` deliberately cause a
hardware restart and are accepted only after DISARM. They are not passive tests.

## Tests and contributions

```sh
python tests/run_tests.py
```

The native model/input and watchdog-health tests use HAL stubs; they do not
replace motherboard/oscilloscope tests. The included GitHub Actions workflow
runs these native tests; it does not qualify hardware behavior.
Please report motherboard/ROM version, module/silicon revision, firmware
SHA256, wiring changes and relevant USB diagnostics with an issue.

## Documentation and license

[docs/REFERENCES.md](docs/REFERENCES.md) links to the Apple ADB protocol,
GLU registers and keyboard-code documentation used during development.

Project-authored source/documentation use the existing [MIT license](LICENSE).
Third-party components retain their licenses; notices for the SDK and
TinyUSB used by the prebuilt firmware are included in [licenses/](licenses/).
See [THIRD_PARTY.md](THIRD_PARTY.md). Apple ROMs, proprietary manuals,
development captures and SDK/toolchain binaries are excluded.
