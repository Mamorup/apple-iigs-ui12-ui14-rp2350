#pragma once
#include <stdbool.h>
#include <stdint.h>

#define EMULATOR_WATCHDOG_TIMEOUT_MS 2000u
#define EMULATOR_CORE1_STALE_MS 500u
#define EMULATOR_WATCHDOG_SERVICE_MS 100u

typedef struct {
    uint32_t heartbeat, last_progress_ms;
    bool seen;
} watchdog_health_t;

/* CPU progress, not host-bus activity: healthy while Apple is off, in RESET,
 * or the emulator is DISARMed. Unsigned subtraction handles timer wrap. */
static inline bool watchdog_health_update(watchdog_health_t *health,
                                         uint32_t now_ms, uint32_t heartbeat) {
    if (heartbeat != health->heartbeat) {
        health->heartbeat = heartbeat;
        health->last_progress_ms = now_ms;
        health->seen = true;
    }
    return health->seen &&
        (uint32_t)(now_ms - health->last_progress_ms) < EMULATOR_CORE1_STALE_MS;
}
