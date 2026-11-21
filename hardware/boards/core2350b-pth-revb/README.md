# Core2350B PTH carrier Rev B

30 x 30 mm, four copper layers. The ready-made Waveshare Core2350B0 mounts
on top via 2.54 mm headers; 44 individual 0.40 mm square contacts mount below.
PLCC pitch 1.27 mm, row centres 17.40 mm, finished holes 0.65 mm, pad 1.05 mm.
Nominal outside contact span: 17.80 x 17.80 mm.

JP1 is on the top edge. All fitted SMD components are on top. Maintain at
least 4 mm below the Core module. GPIO mapping is unchanged. USB uses the
Core module's USB/FPC board; there is no USB connector to order for this carrier.
Bottom PLCC pin 1 has a numeral and triangular marker. R23/R26 are DNP.

CAD checks: zero ERC, DRC, unconnected or schematic parity errors. Independent
GPIO/power and drill checks passed. Minimum track 0.15 mm, clearance 0.20 mm.
All copper layers carry signals and GND pours; no continuous-plane claim.

This PCB has not been fabricated or electrically tested. Contact protrusion,
socket fit, clearance below soldered module pins and the printed housing
require physical checks. Successful JP1-only firmware startup was reported
on the user's existing assembly, not this new carrier.

See README.ru.md for installation, UI14 jumpers and power instructions.
Use firmware from releases/2026-11-21 in the parent GitHub project.
