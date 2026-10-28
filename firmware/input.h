#pragma once
#include <stdint.h>

void input_init(void);
void input_host_reset(void);
void input_keycode(uint8_t adb_code);
uint8_t input_any_key_down(void);
uint8_t input_take_notification(void);
void input_report(void);
void input_service(uint32_t now_ms, uint8_t glu_status, uint8_t modes,
                   uint8_t address_byte, uint8_t enabled);

