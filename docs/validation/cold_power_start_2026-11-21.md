# Cold power start — 2026-11-21

User observed: JP1 power from IIgs sustains normal operation after USB removal,
but startup with JP1 only fails and keyboard is unavailable. Startup timing
is a hypothesis; rail startup quality has not been measured.

Change: assert existing open-drain RESET transistor Q2 via GPIO19 at entry to
main, before USB stdio initialization. Preserve GPIO19 through core1 output
initialization. After auto-arm at 750 ms, both bus_ready and input_init are
complete; hold another 4 ms, then release RESET once. This also replaces the
old extra watchdog startup pulse. Normal keyboard reset and powered-off-host
rearm logic are unchanged. GPIO19 never drives the host RESET line high.

Build: Wall/Wextra/Werror PASS.
UF2 SHA256: 464cbc0363a5f58797818aa5db9855a549e001587bd13a834dcaa3d310c47697
Installed with user-confirmed IIgs off; GPIO13=0 and PH0=0 verified first.
USB SELFTEST failures=0; armed=1 ready=1; startup_reset_held=0;
startup_done=1; startup_fix=1; reset_pulses=1; watchdog healthy.

Pending physical acceptance: disconnect USB while IIgs OFF, then power IIgs
from JP1 only. Check ROM boot and keyboard; repeat three cold starts. Then
check USB-first startup and keyboard reset. Electrical RESET waveform and
3V3 ramp have not been measured. No hardware success claimed yet.

Rollback: backups/before_cold_power_start_2026-11-21/ contains previous main.c,
CMakeLists.txt and UF2 SHA256
4f5d496ded5338ecf6ab8f8b65b8cd7959acab22d07a1a017817ce09f1881a80.

## User hardware result
User confirmed successful IIgs startup with JP1 closed, internal IIgs power
only and USB disconnected, without errors. Cold-start symptom resolved in
this reported check. Three repeated cold starts and a separate keyboard-reset
regression were not explicitly reported yet.
