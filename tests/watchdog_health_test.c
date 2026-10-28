#include <assert.h>
#include <stdio.h>
#include "../firmware/watchdog_health.h"

int main(void) {
    watchdog_health_t h = {0, 0, false};
    assert(!watchdog_health_update(&h, 100, 0)); /* no core1 progress yet */
    assert(watchdog_health_update(&h, 200, 1));
    assert(watchdog_health_update(&h, 699, 1));
    assert(!watchdog_health_update(&h, 700, 1)); /* stale, even if core0 runs */
    assert(!watchdog_health_update(&h, 2000, 1));
    assert(watchdog_health_update(&h, 2100, 2)); /* progress resumes */
    /* No host clocks/bus cycles are needed to remain healthy. */
    for (uint32_t t = 2200; t < 10000; t += 100)
        assert(watchdog_health_update(&h, t, t));
    h = (watchdog_health_t){0, 0, false};
    assert(watchdog_health_update(&h, UINT32_MAX-100, UINT32_MAX));
    assert(watchdog_health_update(&h, 100, UINT32_MAX)); /* 201ms across wrap */
    assert(!watchdog_health_update(&h, 399, UINT32_MAX)); /* 500ms */
    assert(watchdog_health_update(&h, 400, 0)); /* heartbeat counter wraps */
    puts("PASS: core1 progress, stale detection, idle operation and timer/counter wrap");
    return 0;
}
