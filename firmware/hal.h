#pragma once
#include <stdint.h>
void glu_write(uint8_t reg,uint8_t value);
void system_reset_pulse(void);
void set_apple_button(unsigned gpio,int pressed);
void adb_phy_init(void);
void adb_phy_reset(void);
uint8_t adb_phy_talk(uint8_t command,uint8_t *data,uint8_t capacity);
void adb_phy_listen(uint8_t command,const uint8_t *data,uint8_t length);
