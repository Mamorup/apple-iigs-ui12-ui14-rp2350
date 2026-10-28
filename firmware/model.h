#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint8_t key,command,mx,my,anykey,mods,response,irq_enable;
 bool command_full,key_strobe,data_full,mouse_full,y_phase,status_read;
} glu_t;
void glu_reset(glu_t *g);
uint8_t glu_host_peek(const glu_t *g,uint8_t physical_select);
void glu_host_end(glu_t *g,uint8_t physical_select,bool write,uint8_t data);
void glu_mcu_write(glu_t *g,uint8_t reg,uint8_t data);
uint8_t glu_status(const glu_t *g);
bool glu_irq(const glu_t *g);
typedef struct {
 uint8_t modes,config[3],ram[96],error,cmd,needed,received,args[10];
 uint8_t reply[64],head,tail; bool synced; uint16_t srq;
 uint8_t action,physical,length,payload[8],keycode;
 uint32_t commands,rom_reads;
} controller_t;
void ctrl_reset(controller_t *c);
void ctrl_host_reset(controller_t *c);
void ctrl_feed(controller_t *c,uint8_t data);
bool ctrl_pop(controller_t *c,uint8_t *data);
void ctrl_adb_reply(controller_t *c,const uint8_t *data,uint8_t length);
int model_selftest(void);
