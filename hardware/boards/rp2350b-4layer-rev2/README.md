# RP2350B four-layer Rev2.2 — ALL via lands 0.45 mm

Supersedes Rev2 and Rev2.1. JLCPCB requires BOTH a 0.20 mm via drill and
a 0.45 mm via land to avoid the Kelvin-test condition reported by support.
Rev2.1 fixed the drill only; its 0.40 mm lands were insufficient.

Every one of the 89 vias is now 0.45 mm land / 0.20 mm drill, on all four
copper layers. Project rules enforce these minima and 0.125 mm annular rings.
No 0.40 mm via lands or 0.15 mm drill holes remain.

30 x 30 mm, four layers. Component positions, orientations, pad net mapping
and the PLCC44 connector holes are unchanged. The copper routing was revised.
Minimum track/clearance is 0.10/0.10 mm. BOM and CPL positions are unchanged;
J13 USB remains excluded from SMT assembly.

ERC=0, DRC=0, unconnected=0, schematic parity=0. All via sizes were checked
in native CAD, the drill output was checked, and the PLCC count is 44.
Bottom-component clearance remains at least 2 mm from the connector holes.
See audit.json for critical-net trace lengths after rerouting.

CAD validation does not constitute electrical testing or physical fitment.
Contact protrusion, housing fit and electrical bring-up remain untested.
USB impedance is unqualified. JLCPCB must review the replacement files
and confirm removal of the extra test/charge.

Use only the matching Rev2_2_JLC045 Gerber and KiCad ZIPs in this directory.
Previous manufacturing ZIPs are removed from the current GitHub branch and
remain in Git history. The separate Core2350B PTH carrier is unchanged.
