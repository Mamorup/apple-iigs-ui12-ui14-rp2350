# Third-party components and references

The project preserves the existing MIT license for original project source and
documentation. This does not relicense third-party code, libraries or manuals.

- **Raspberry Pi Pico SDK2.3.1**, commit079c6f39023649b154152db30f1d781e884879bc:
  SDK source is fetched separately. SDK BSD-3-Clause notice used by the supplied
  binary is reproduced in licenses/pico-sdk-BSD-3-Clause.txt.
- **TinyUSB**, brought in by that SDK: its MIT notice is reproduced in
  licenses/tinyusb-MIT.txt. It is not a project-authored USB implementation.
- The prebuilt image also uses the Arm GNU embedded runtime/newlib. Toolchains
  are not distributed here; retain the applicable runtime/dependency notices
  when redistributing builds with additional components.
- **KiCad standard libraries** provide symbols/footprint references. See
  https://www.kicad.org/libraries/license/ for CC-BY-SA4.0 and its design
  exception. No separate symbol/footprint library collection is bundled.
- **Apple documentation and ROMs** remain their owners' works. Links and a
  description of protocol findings are provided; ROM dumps and full manuals
  are excluded.
- **Neil Parker's ADB research** and Apple technical manuals are credited in
  docs/REFERENCES.md. GSplus/KEGS was also consulted as a comparison during
  development; neither emulator is bundled as source or a dependency here.

The existing license holder text, ADBEMU contributors, is preserved unchanged.
Before public release, the maintainer can add their preferred name or handle
to the project credits; this snapshot does not invent a personal identity.
