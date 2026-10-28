# Reference documentation

No Apple/Mitsubishi ROM dumps or complete third-party manuals are distributed.
Page numbers below are printed page numbers and can differ between editions.

- [Apple IIgs Hardware Reference](https://downloads.reactivemicro.com/Apple%20II%20Items/Documentation/Manuals/IIgs/IIgs%20Hardware%20Reference.pdf): chapter6, ADB electrical interface, registers, Attention/Sync/Service Request, bit timing and keyboard/mouse packets.
- [Guide to the Macintosh Family Hardware, second edition](https://vintageapple.org/inside_o/pdf/Guide_to_Macintosh_Family_Hardware_2nd_Edition_1990.pdf): chapter8; page309 contrasts ADB transition and Keyboard Driver codes; pages312–319 cover transactions/commands/signals/timing.
- [Neil Parker, Inside the Apple IIGS ADB Controller](https://llx.com/Neil/a2/adb.html): External I/O Ports and ADB GLU Registers, STB/SEL lines and MCU-side interface. This is an independent investigation, not an Apple specification.
- [Apple IIgs Firmware Reference, Cortland draft scan](https://www.brutaldeluxe.fr/documentation/cortland/v2_01_AppleIIGSFirmwareReferenceManual.pdf): controller commands, interrupt/status definitions, system warm/cold resets.
- [Raspberry Pi Pico SDK2.3.1](https://github.com/raspberrypi/pico-sdk/tree/2.3.1): watchdog, PIO, DMA and multicore APIs. Pinned tested commit079c6f39023649b154152db30f1d781e884879bc.
- [RP2350 documentation](https://www.raspberrypi.com/documentation/microcontrollers/rp2350.html): check the datasheet for FT digital GPIO voltage/power conditions and silicon errata before reproducing this direct-interface prototype.
- [KiCad library license and design exception](https://www.kicad.org/libraries/license/).

The development investigation also checked the relevant branches in a local
ROM01 image. That image is not part of this repository. The key finding was
that FF85A0 tests the returned MCU51 byte forA5, and the failure branch at
FF85AD clears the16-bit CPU03F3/03F4 warm-start signature. Documentation,
source reading, USB traces and physical measurements were used together.
