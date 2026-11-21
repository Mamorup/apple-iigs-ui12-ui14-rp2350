# RP2350B four-layer Rev2 prototype

30 x 30 mm board with a discrete RP2350B (no Core module). Use matching
Rev2 Gerber, KiCad archive, grouped JLCPCB BOM and CPL from this directory.
J13 USB connector footprint is retained but excluded from assembly.

44 PTH contacts: finished hole 0.65 mm, pad 1.05 mm, pitch 1.27 mm, row
centres 17.40 mm. With 0.40 mm square pins, nominal outside span is 17.80 mm.
JP1 is on top near the edge; Flash U3 is centred underneath.
Bottom component courtyard-to-hole-edge clearance is at least 2.070 mm.

The archived CAD passed ERC/DRC with zero unconnected items and schematic
parity errors. These are CAD results, not qualification of an assembled board.
Physical pin fit, protrusion and clearance to the printed housing remain
unverified. USB impedance and full electrical bring-up are unqualified.
The native project and local libraries are in the KiCad ZIP.

The Core2350B carrier is a different board; do not interchange its files.
