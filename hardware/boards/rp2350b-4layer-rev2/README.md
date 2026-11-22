# RP2350B four-layer Rev2.1 — standard drill update

Replacement for the discrete-RP2350B Rev2 board flagged by JLCPCB for
mandatory four-wire Kelvin testing because of 0.15 mm drilled vias.
All vias now use 0.20 mm drills: 113 pads at 0.40 mm and 3 at 0.45 mm.
There are no 0.15 mm drill tools in the exported fabrication set.

30 x 30 mm, four copper layers, 44 PLCC pin holes at 0.65 mm. Outline,
component positions/orientations, footprint attributes and pad nets are
unchanged. BOM/CPL are unchanged; J13 remains excluded from SMT assembly.
Some traces were rerouted for clearance. Minimum track/clearance 0.10/0.10 mm.
USB and crystal trace lengths are unchanged.

ERC=0, DRC=0, unconnected=0, schematic parity=0. Drill count and bottom
component clearance were checked. See audit.json and reports.

This is CAD validation only. Physical pin fit, protrusion, housing clearance
and electrical bring-up remain untested. JLCPCB must review the replacement
and confirm the revised quote; no manufacturer acceptance is implied.

Use the Rev2_1_StandardVias_Gerbers.zip for fabrication and the matching
KiCad ZIP for editable sources. The old Rev2 manufacturing archives were
removed from the current branch; historical commits still contain them.
The directory name is retained to preserve existing links.

This board uses a discrete RP2350B. The Core2350B PTH carrier in the sibling
directory is a separate design and is unchanged.
