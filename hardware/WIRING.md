# Core2350B0 prototype wiring

Use the **GPIO numbers printed on the module**, not RP2350 package pin numbers.
UI12/UI14 numbers below mean the original pads on the IIgs motherboard after
removing those chips. Disconnect earlier CPLD/AVR emulators from these nets.
Keep the motherboard's original ADB pull-up and Q4/R108 driver circuit.

## Power and interface constraints

- Tested module: Waveshare Core2350B0, RP2350B SDK silicon revision3 (A3).
  A2 is not qualified. Review the RP2350 FT GPIO power/voltage conditions.
- Common GND: Core GND to motherboard GND, for example UI12 pad7.
- Power Core USB FIRST, then IIgs. Switch IIgs OFF before disconnecting Core.
- JP1 stays OPEN during USB power. Do not connect motherboard5V to Core3V3.
- Series resistors are not level translators. Only suitable powered FT digital
  GPIOs are used by this prototype; do not generalize this to other RP GPIOs.
- GPIO15/19 must use external open-drain/open-collector drivers, never direct
  push-pull connections to the motherboard IRQ/RESET nets.
- Solder and continuity-check with both boards powered off.

## Signal table

| Module | Signal | IIgs pad | Connection |
|---|---|---|---|
| GPIO0 | D0 | UI12 pad38 | 330 ohm series |
| GPIO1 | D1 | UI12 pad41 | 330 ohm series |
| GPIO2 | D2 | UI12 pad42 | 330 ohm series |
| GPIO3 | D3 | UI12 pad43 | 330 ohm series |
| GPIO4 | D4 | UI12 pad44 | 330 ohm series |
| GPIO5 | D5 | UI12 pad1 | 330 ohm series |
| GPIO6 | D6 | UI12 pad2 | 330 ohm series |
| GPIO7 | D7 | UI12 pad3 | 330 ohm series |
| GPIO8 | PH0 | UI12 pad37 | 220 ohm |
| GPIO9 | R/W | UI12 pad35 | 220 ohm |
| GPIO10 | KSEL0 | UI12 pad30 | 220 ohm |
| GPIO11 | KSEL1 | UI12 pad31 | 220 ohm |
| GPIO12 | KSEL2 | UI12 pad32 | 220 ohm |
| GPIO13 | SYSTEM_RESET sense | UI12 pad33 | 220 ohm; NOT UI14 pad16 |
| GPIO14 | VBL | UI12 pad34 / UI14 pad14 | 220 ohm; optional for initial bus test |
| GPIO15 | IRQ | UI12 pad36 | 100 ohm -> Q1 gate; Q1 source=GND; drain=pad36; gate 100k to GND |
| GPIO16 | Original MCU clock | UI14 pad17 | optional 220 ohm; not used by current firmware |
| GPIO17 | ADB IN | UI14 pad47 | 220 ohm |
| GPIO18 | ADB OUT | UI14 pad41 | 220 ohm; retains original R108/Q4 |
| GPIO19 | SYS_RESET drive | UI14 pad49 | 100 ohm -> Q2 gate; Q2 source=GND; drain=pad49; gate 100k to GND |
| GPIO20 | COMMAND | UI14 pad39 | 220 ohm; LOW released |
| GPIO21 | OPTION | UI14 pad40 | 220 ohm; LOW released |

## IRQ and RESET drivers

For each2N7002: source to GND, drain to the corresponding motherboard signal,
gate to GPIO through100ohm, and100kohm from gate to GND. Check the exact
part/package pinout. GPIO15 drives IRQ at UI12 pad36; GPIO19 drives system
RESET at UI14 pad49. Both are active by pulling the motherboard line LOW.

An NPN circuit is an alternative: emitter=GND, collector=motherboard signal,
GPIO through2.2kohm to base,100kohm base-to-emitter. Verify the exact transistor
pinout; do not assume TO-92 devices all have the same arrangement.

## Important distinctions

- GPIO13 senses SYSTEM RESET at UI12 pad33. It must NOT go to UI14 pad16;
  the latter is the original MCU's independent power-on RC reset.
- GPIO18 goes to UI14 pad41 and drives the original Q4 path. It must NOT
  connect directly to the physical ADB data line. GPIO17 senses UI14 pad47.
- Command=GPIO20/UI14 pad39 and Option=GPIO21/UI14 pad40: LOW=released,
  HIGH=pressed. Their state is intentionally preserved across host RESET.
- GPIO14/VBL and GPIO16/original MCU clock are optional and unused by the
  current firmware. GPIO22/23 UART is not implemented. No wiring is needed
  for the old MD0..7, SEL0..3 or STB inter-chip nets; they are internal here.
- The native KiCad9 schematic and PDF show the prototype/BOM, including
  optional/unimplemented provisions. No routed PCB is supplied.

GPIO39 is the module's existing onboard LED; no motherboard wire is needed.
