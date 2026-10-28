#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pico/platform.h"
#include "hardware/sync.h"
#include "hardware/structs/sio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "watchdog_health.h"
#include "model.h"
#include "model.c"
#include "hal.h"
#include "input.h"
// GPIO0..7 D0..7; 8 PH0; 9 RW; 10..12 physical KSEL; 13 SYSTEM RESET.
// 15 IRQ MOSFET gate, 17 ADB input,18 Q4 drive,19 reset gate,20 Command,21 Option.
#define PH (1u<<8)
#define RW (1u<<9)
#define RESET (1u<<13)
#define QMASK 255u
#define QSIZE 256u
typedef struct {uint32_t data[QSIZE];atomic_uint head,tail;} ring_t;
static ring_t host_to_ctrl,ctrl_to_glu;
static atomic_bool armed=false,bus_ready=false,host_reset=true,adb_poll=true;
static atomic_uint reset_epoch,public_status,public_system_status,read_mismatch_mask,read_mismatch_count,last_read_expected,last_read_seen,last_read_oe,last_read_out,response_applied,cycles,read_cycles,write_cycles,overflows;
static controller_t controller;
static atomic_uint public_key_state;
static unsigned command_hist[256],reset_pulses;
static atomic_uint core1_heartbeat;
static atomic_bool watchdog_test_core1_hang;
static watchdog_health_t watchdog_health;
static bool watchdog_started, watchdog_timeout_boot;
static uint32_t watchdog_last_service, watchdog_feeds;
static void watchdog_service(uint32_t now_ms){
 if(!watchdog_started||(uint32_t)(now_ms-watchdog_last_service)<EMULATOR_WATCHDOG_SERVICE_MS)return;
 watchdog_last_service=now_ms;
 if(watchdog_health_update(&watchdog_health,now_ms,atomic_load_explicit(&core1_heartbeat,memory_order_relaxed))){watchdog_update();watchdog_feeds++;}
}
/* Decimated in the bus loop: no timer reads, locks or interrupts in the
 * PH0 sampling path. Also executed in DISARM and host RESET/PH0 wait paths. */
static inline void core1_health_poll(uint32_t *budget,uint32_t *heartbeat){
 if(--*budget)return;
 *budget=4096;
 if(atomic_load_explicit(&watchdog_test_core1_hang,memory_order_relaxed))for(;;)tight_loop_contents();
 atomic_store_explicit(&core1_heartbeat,++*heartbeat,memory_order_relaxed);
}
static uint32_t trace[1024];static unsigned trace_head;
static inline bool push(ring_t *q,uint32_t value){unsigned h=atomic_load_explicit(&q->head,memory_order_relaxed);unsigned n=(h+1)&QMASK;if(n==atomic_load_explicit(&q->tail,memory_order_acquire))return false;q->data[h]=value;atomic_store_explicit(&q->head,n,memory_order_release);return true;}
static inline bool pop(ring_t *q,uint32_t *value){unsigned t=atomic_load_explicit(&q->tail,memory_order_relaxed);if(t==atomic_load_explicit(&q->head,memory_order_acquire))return false;*value=q->data[t];atomic_store_explicit(&q->tail,(t+1)&QMASK,memory_order_release);return true;}
void glu_write(uint8_t reg,uint8_t value){if(!atomic_load(&bus_ready))return;if(!push(&ctrl_to_glu,((atomic_load_explicit(&reset_epoch,memory_order_acquire)&0xffff)<<16)|((uint32_t)reg<<8)|value))atomic_fetch_add(&overflows,1);}
void set_apple_button(unsigned gpio,int pressed){if(atomic_load(&bus_ready))gpio_put(gpio,pressed!=0);}
void system_reset_pulse(void){if(!atomic_load(&bus_ready))return;reset_pulses++;gpio_put(19,1);busy_wait_us_32(4000);gpio_put(19,0);}
static void passive(void){for(unsigned i=0;i<24;i++){gpio_init(i);gpio_set_dir(i,GPIO_IN);gpio_disable_pulls(i);}}
static void active_outputs(void){for(unsigned i=0;i<24;i++){gpio_init(i);gpio_disable_pulls(i);}for(unsigned i=0;i<8;i++)gpio_set_dir(i,GPIO_IN);
 for(unsigned i=8;i<=14;i++)gpio_set_dir(i,GPIO_IN);
 for(unsigned i=15;i<=21;i++){gpio_put(i,0);gpio_set_dir(i,(i==16||i==17)?GPIO_IN:GPIO_OUT);}
}
static volatile uint32_t read_headers[16],read_replies,last_reply;
_Alignas(64) static volatile uint32_t response_table[16];
static int lookup_dma=-1,address_dma=-1;
static uint request_program_offset;
static bool request_program_loaded;
static glu_t * volatile irq_glu;
static uint read_program_offset;
static bool read_program_loaded;
static void __not_in_flash_func(read_service)(void){
 pio_interrupt_clear(pio2,0);
 while(!pio_sm_is_rx_fifo_empty(pio2,0)){
  uint32_t control=pio_sm_get(pio2,0)&15;
  uint32_t pins=sio_hw->gpio_in;
  if((control&1)&&((control>>1)<6)&&(pins&RESET)&&(((pins>>9)&15)==control)){
   uint32_t word=response_table[control];
   if(!pio_sm_is_tx_fifo_full(pio2,0)){pio_sm_put(pio2,0,word);read_replies++;last_reply=word;}
  }
 }
}
static inline void refresh_read_table(const glu_t *g){
 atomic_store_explicit(&public_key_state,(uint32_t)glu_host_peek(g,0)|((uint32_t)g->mods<<8)|((uint32_t)g->irq_enable<<16),memory_order_relaxed);
 for(unsigned tag=0;tag<16;tag++)if(!(tag&1)||(tag>>1)>=6)response_table[tag]=tag;
 for(unsigned sel=0;sel<6;sel++){unsigned tag=(sel<<1)|1;response_table[tag]=tag|((uint32_t)glu_host_peek(g,sel)<<4)|0xff000;}
}
static void lookup_start(void){
 if(!request_program_loaded){
  // Build a full 32-bit SRAM address from the 64-byte-aligned table base
  // and RW/KSEL index. No processor IRQ is involved in the response path.
  uintptr_t base=((uintptr_t)response_table)>>6;
  uint16_t ins[]={
   pio_encode_mov(pio_isr,pio_null),pio_encode_in(pio_pins,4),pio_encode_mov(pio_x,pio_isr),pio_encode_jmp_x_ne_y(5),pio_encode_jmp(0),
   pio_encode_mov(pio_y,pio_x),pio_encode_mov(pio_isr,pio_null),
   pio_encode_set(pio_x,(base>>20)&31),pio_encode_in(pio_x,5),
   pio_encode_set(pio_x,(base>>15)&31),pio_encode_in(pio_x,5),
   pio_encode_set(pio_x,(base>>10)&31),pio_encode_in(pio_x,5),
   pio_encode_set(pio_x,(base>>5)&31),pio_encode_in(pio_x,5),
   pio_encode_set(pio_x,base&31),pio_encode_in(pio_x,5),
   pio_encode_in(pio_pins,4),pio_encode_in(pio_null,2),pio_encode_push(false,false),pio_encode_jmp(0)
  };
  struct pio_program prog={.instructions=ins,.length=21,.origin=-1};request_program_offset=pio_add_program(pio1,&prog);pio_sm_claim(pio1,0);
  lookup_dma=dma_claim_unused_channel(true);address_dma=dma_claim_unused_channel(true);request_program_loaded=true;
 }
 pio_sm_set_enabled(pio1,0,false);pio_sm_clear_fifos(pio1,0);
 pio_sm_config cfg=pio_get_default_sm_config();sm_config_set_in_pins(&cfg,9);sm_config_set_in_shift(&cfg,false,false,32);sm_config_set_wrap(&cfg,request_program_offset,request_program_offset+20);pio_sm_init(pio1,0,request_program_offset,&cfg);pio_sm_exec(pio1,0,pio_encode_set(pio_y,31));
 dma_channel_config dc=dma_channel_get_default_config(lookup_dma);channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);channel_config_set_read_increment(&dc,false);channel_config_set_write_increment(&dc,false);channel_config_set_dreq(&dc,pio_get_dreq(pio2,0,true));
 dma_channel_configure(lookup_dma,&dc,&pio2->txf[0],(const void*)response_table,1,false);
 dc=dma_channel_get_default_config(address_dma);channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);channel_config_set_read_increment(&dc,false);channel_config_set_write_increment(&dc,false);channel_config_set_dreq(&dc,pio_get_dreq(pio1,0,false));
 dma_channel_configure(address_dma,&dc,&dma_hw->ch[lookup_dma].al3_read_addr_trig,&pio1->rxf[0],0x0fffffffu,true);
 pio_sm_set_enabled(pio1,0,true);
}
static void lookup_stop(void){if(request_program_loaded){pio_sm_set_enabled(pio1,0,false);dma_channel_abort(address_dma);dma_channel_abort(lookup_dma);}}
static void read_pio_start(glu_t *state){
 irq_glu=state;refresh_read_table(state);
 if(!read_program_loaded){
  // X = cached tagged response; Y = latest RW/KSEL code. Replies carry a
  // 4-bit control tag so a late response can NEVER enable output for another
  // bus selection. Input monitoring and output release run independently.
  uint16_t ins[]={
   pio_encode_jmp_pin(6),        // 0: RESET high -> monitor controls
   pio_encode_set(pio_x,15),    // 1: otherwise invalidate response
   pio_encode_mov(pio_osr,pio_null),
   pio_encode_out(pio_pindirs,8),
   pio_encode_set(pio_y,31),
   pio_encode_jmp(0),
   pio_encode_mov(pio_isr,pio_null), // 6
   pio_encode_in(pio_pins,4),
   pio_encode_mov(pio_osr,pio_x),
   pio_encode_mov(pio_x,pio_isr),
   pio_encode_jmp_x_ne_y(20),
   pio_encode_mov(pio_x,pio_osr), // 11: unchanged control
   pio_encode_pull(false,false), // cached X if TX FIFO empty
   pio_encode_mov(pio_isr,pio_osr),
   pio_encode_out(pio_x,4),
   pio_encode_jmp_x_ne_y(27),    // discard stale tagged response
   pio_encode_out(pio_pins,8),
   pio_encode_out(pio_pindirs,8),
   pio_encode_mov(pio_x,pio_isr),
   pio_encode_jmp(0),
   pio_encode_mov(pio_y,pio_x),  // 20: new control -> immediate release
   pio_encode_set(pio_x,15),
   pio_encode_mov(pio_osr,pio_null),
   pio_encode_out(pio_pindirs,8),
   pio_encode_nop(),
   pio_encode_nop(),
   pio_encode_jmp(0),
   pio_encode_mov(pio_osr,pio_null), // 27: stale tag -> release and invalidate
   pio_encode_out(pio_pindirs,8),
   pio_encode_set(pio_x,15),
   pio_encode_jmp(0)
  };
  struct pio_program prog={.instructions=ins,.length=31,.origin=-1};
  read_program_offset=pio_add_program(pio2,&prog);read_program_loaded=true;
 }
 pio_sm_set_enabled(pio2,0,false);pio_sm_clear_fifos(pio2,0);
 pio_sm_config cfg=pio_get_default_sm_config();sm_config_set_in_pins(&cfg,9);
 sm_config_set_out_pins(&cfg,0,8);sm_config_set_jmp_pin(&cfg,13);
 sm_config_set_in_shift(&cfg,false,false,32);sm_config_set_out_shift(&cfg,true,false,32);
 sm_config_set_wrap(&cfg,read_program_offset,read_program_offset+30);
 pio_sm_init(pio2,0,read_program_offset,&cfg);
 pio_sm_exec(pio2,0,pio_encode_set(pio_x,15));pio_sm_exec(pio2,0,pio_encode_set(pio_y,31));
 pio_sm_set_pindirs_with_mask(pio2,0,0,0xff);
 for(unsigned pin=0;pin<8;pin++)pio_gpio_init(pio2,pin);
 
 pio_set_irq0_source_enabled(pio2,pis_interrupt0,false);pio_interrupt_clear(pio2,0);
 irq_set_enabled(PIO2_IRQ_0,false);pio_sm_set_enabled(pio2,0,true);lookup_start();
}
static void __not_in_flash_func(bus_core)(void){
 uint32_t irq=save_and_disable_interrupts();(void)irq;
 uint32_t heartbeat_budget=4096,heartbeat=0;
 for(;;){while(!atomic_load_explicit(&armed,memory_order_acquire)){core1_health_poll(&heartbeat_budget,&heartbeat);tight_loop_contents();}
  glu_t g;glu_reset(&g);g.mouse_full=true;active_outputs();read_pio_start(&g);restore_interrupts(0);
  atomic_store_explicit(&bus_ready,true,memory_order_release);
  unsigned nr=0,nw=0;
  bool pending_publish=true;
  bool high=false,was_reset=false;uint8_t selected=7;bool writing=false;uint32_t prior=0;uint8_t driven=0,prior_oe=0,prior_out=0;
  while(atomic_load_explicit(&armed,memory_order_relaxed)){
   core1_health_poll(&heartbeat_budget,&heartbeat);
   uint32_t pins=sio_hw->gpio_in;
   if(!(pins&RESET)){
    atomic_store_explicit(&host_reset,true,memory_order_release);
    sio_hw->gpio_oe_clr=0xff;sio_hw->gpio_clr=1u<<15;
    if(!was_reset){glu_reset(&g);g.mouse_full=true;refresh_read_table(&g);atomic_fetch_add_explicit(&reset_epoch,1,memory_order_release);was_reset=true;}
    high=false;prior=pins;continue;
   }
   was_reset=false;atomic_store_explicit(&host_reset,false,memory_order_release);
   if(pins&PH){
    if(!high){selected=(pins>>10)&7;writing=!(pins&RW);high=true;
     if(selected<6&&!writing)driven=glu_host_peek(&g,selected);
    }
    prior_oe=pio2->dbg_padoe;prior_out=pio2->dbg_padout;prior=pins;continue;
   }
   // Release data immediately at PH0 falling edge; use last PH0-high sample
   // for writes so post-edge motherboard decode/data transitions cannot corrupt it.
   bool dirty=false;
   if(high){
    if(selected<6){uint8_t d=(uint8_t)prior;
     if(!writing && d!=driven){atomic_fetch_or_explicit(&read_mismatch_mask,d^driven,memory_order_relaxed);atomic_fetch_add_explicit(&read_mismatch_count,1,memory_order_relaxed);atomic_store_explicit(&last_read_expected,driven,memory_order_relaxed);atomic_store_explicit(&last_read_seen,d,memory_order_relaxed);atomic_store_explicit(&last_read_oe,prior_oe,memory_order_relaxed);atomic_store_explicit(&last_read_out,prior_out,memory_order_relaxed);}
     if(writing||selected==1||selected==2||selected==3||selected==4){glu_host_end(&g,selected,writing,d);dirty=true;}
     
     if(writing){nw++;
      if(selected==3&&!push(&host_to_ctrl,((atomic_load_explicit(&reset_epoch,memory_order_relaxed)&0xffff)<<16)|d))atomic_fetch_add_explicit(&overflows,1,memory_order_relaxed);
     }else nr++;
    }
    high=false;
   }
   uint32_t op;
   if(pop(&ctrl_to_glu,&op)){
    if((op>>16)!=(atomic_load_explicit(&reset_epoch,memory_order_relaxed)&0xffff))continue;
    dirty=true;
    uint8_t reg=(op>>8)&255;
    if(reg==7&&g.data_full){ /* preserve pending response; core0 only posts one at a time */
     atomic_fetch_add_explicit(&overflows,1,memory_order_relaxed);
    }else { glu_mcu_write(&g,reg,(uint8_t)op); if(reg==7){atomic_store_explicit(&public_status,glu_status(&g),memory_order_release);atomic_fetch_add_explicit(&response_applied,1,memory_order_release);} }
   }
   if(dirty)refresh_read_table(&g);
   pending_publish |= dirty;
   if(pending_publish && ((pins>>10)&7)==7){pending_publish=false;atomic_store_explicit(&public_status,glu_status(&g),memory_order_release);
    atomic_store_explicit(&public_system_status,glu_host_peek(&g,2),memory_order_release);
    if(glu_irq(&g))sio_hw->gpio_set=1u<<15;else sio_hw->gpio_clr=1u<<15;}
   if(((pins>>10)&7)==7){atomic_store_explicit(&read_cycles,nr,memory_order_relaxed);atomic_store_explicit(&write_cycles,nw,memory_order_relaxed);atomic_store_explicit(&cycles,nr+nw,memory_order_relaxed);}
   prior=pins;
  }
  save_and_disable_interrupts();lookup_stop();pio_sm_set_enabled(pio2,0,false);irq_set_enabled(PIO2_IRQ_0,false);pio_set_irq0_source_enabled(pio2,pis_interrupt0,false);pio_sm_set_pindirs_with_mask(pio2,0,0,0xff);atomic_store_explicit(&bus_ready,false,memory_order_release);passive();
 }
}
static void actions(void){uint8_t packet[8],n;uint8_t action=controller.action;controller.action=0;
 switch(action){case 1:adb_phy_reset();break;case 2:system_reset_pulse();break;case 3:input_init();break;
 case 4:input_keycode(controller.keycode);break;
 case 5:n=adb_phy_talk(controller.physical,packet,8);ctrl_adb_reply(&controller,packet,n);break;
 case 6:adb_phy_listen(controller.physical,controller.payload,controller.length);break;default:break;}
}
static void print_status(void){printf("STATUS armed=%u ready=%u cycles=%lu reads=%lu writes=%lu overflow=%lu reset=%lu glu=%02lx sync=%u cmd=%02x need=%u got=%u rom_reads=%lu poll=%u chip_revision=%u\n",
 atomic_load(&armed),atomic_load(&bus_ready),(unsigned long)atomic_load(&cycles),(unsigned long)atomic_load(&read_cycles),(unsigned long)atomic_load(&write_cycles),(unsigned long)atomic_load(&overflows),(unsigned long)atomic_load(&reset_epoch),(unsigned long)atomic_load(&public_status),controller.synced,controller.cmd,controller.needed,controller.received,(unsigned long)controller.rom_reads,atomic_load(&adb_poll),rp2350_chip_version());}
// Standalone only: drives PH0/control pins internally with no IIgs attached.
static void bus_selftest(bool write_test){
 if(atomic_load(&armed)){puts("BUSTEST requires DISARM; USB-only board");return;}
 static uint32_t samples[4096];
 atomic_store(&host_to_ctrl.head,0);atomic_store(&host_to_ctrl.tail,0);atomic_store(&ctrl_to_glu.head,0);atomic_store(&ctrl_to_glu.tail,0);
 atomic_store(&armed,true);while(!atomic_load(&bus_ready))tight_loop_contents();
 for(unsigned pin=9;pin<=13;pin++){gpio_put(pin,pin==13||(pin==9&&!write_test)||(pin==11&&write_test));gpio_set_dir(pin,GPIO_OUT);}
 gpio_put(8,0);gpio_set_dir(8,GPIO_OUT);
 if(write_test){gpio_put_masked(0xff,0x54);gpio_set_dir_out_masked(0xff);}else glu_write(0,0x25);busy_wait_us_32(200); // Host sees A5 because KEY_STROBE is set.
 PIO pio=pio0;uint gen=pio_claim_unused_sm(pio,true),rx=pio_claim_unused_sm(pio,true);
 // At 150 MHz: 75 cycles HIGH,75 LOW => 1 MHz.
 uint16_t gen_ins[]={pio_encode_set(pio_pins,1)|pio_encode_delay(31),pio_encode_nop()|pio_encode_delay(31),pio_encode_nop()|pio_encode_delay(10),pio_encode_set(pio_pins,0)|pio_encode_delay(31),pio_encode_nop()|pio_encode_delay(31),pio_encode_nop()|pio_encode_delay(9),pio_encode_jmp(0)};
 struct pio_program gen_prog={.instructions=gen_ins,.length=7,.origin=-1};uint go=pio_add_program(pio,&gen_prog);
 // Relocate local JMP target because raw program origin is dynamic.
 pio->instr_mem[go+6]=pio_encode_jmp(go);
 pio_sm_config gc=pio_get_default_sm_config();sm_config_set_set_pins(&gc,8,1);sm_config_set_wrap(&gc,go,go+6);
 sm_config_set_clkdiv(&gc,(float)clock_get_hz(clk_sys)/150000000.f);
 pio_gpio_init(pio,8);pio_sm_set_consecutive_pindirs(pio,gen,8,1,true);pio_sm_init(pio,gen,go,&gc);pio_sm_set_pins_with_mask(pio,gen,0,1u<<8);busy_wait_us_32(50);
 uint16_t rx_ins[]={pio_encode_wait_gpio(false,8),pio_encode_wait_gpio(true,8),pio_encode_nop()|pio_encode_delay(29),pio_encode_nop()|pio_encode_delay(29),pio_encode_in(pio_pins,8),pio_encode_wait_gpio(false,8),pio_encode_jmp(0)};
 struct pio_program rx_prog={.instructions=rx_ins,.length=7,.origin=-1};uint ro=pio_add_program(pio,&rx_prog);pio->instr_mem[ro+6]=pio_encode_jmp(ro+1);
 pio_sm_config rc=pio_get_default_sm_config();sm_config_set_in_pins(&rc,0);sm_config_set_in_shift(&rc,true,true,8);sm_config_set_fifo_join(&rc,PIO_FIFO_JOIN_RX);sm_config_set_wrap(&rc,ro+1,ro+6);
 sm_config_set_clkdiv(&rc,(float)clock_get_hz(clk_sys)/150000000.f);pio_sm_init(pio,rx,ro,&rc);
 int dma=dma_claim_unused_channel(true);dma_channel_config dc=dma_channel_get_default_config(dma);channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);channel_config_set_read_increment(&dc,false);channel_config_set_write_increment(&dc,true);channel_config_set_dreq(&dc,pio_get_dreq(pio,rx,false));
 dma_channel_configure(dma,&dc,samples,&pio->rxf[rx],4096,true);
 uint32_t before=0;
 pio_sm_set_enabled(pio,gen,true);
 // Exclude activation boundary: allow bus state and pad input synchronizers
 // to settle for 10 us before collecting the 4096 steady-state samples.
 busy_wait_us_32(10);pio_sm_set_enabled(pio,rx,true);
 uint32_t start=time_us_32();while(dma_channel_is_busy(dma)&&(uint32_t)(time_us_32()-start)<100000) tight_loop_contents();
 bool timeout=dma_channel_is_busy(dma);pio_set_sm_mask_enabled(pio,(1u<<gen)|(1u<<rx),false);if(timeout)dma_channel_abort(dma);
 gpio_set_function(8,GPIO_FUNC_SIO);gpio_put(8,0);for(unsigned pin=10;pin<=12;pin++)gpio_put(pin,1);busy_wait_us_32(20);
 unsigned bad=0,first_index=0;uint8_t first=0;for(unsigned i=0;i<4096;i++)if((uint8_t)(samples[i]>>24)!=(write_test?0x54:0xa5)){if(!bad){first=samples[i]>>24;first_index=i;}bad++;}
 uint32_t observed=atomic_load(write_test?&write_cycles:&read_cycles)-before;
 uint8_t reg27=atomic_load(&public_system_status);
 dma_channel_unclaim(dma);pio_sm_unclaim(pio,gen);pio_sm_unclaim(pio,rx);pio_remove_program(pio,&gen_prog,go);pio_remove_program(pio,&rx_prog,ro);
 atomic_store(&armed,false);while(atomic_load(&bus_ready))tight_loop_contents();
 printf("BUSTEST %s 1MHz samples=4096 bad=%u first_bad=%02x index=%u timeout=%u core1_cycles=%lu C027=%02x\n",write_test?"WRITE":"READ",bad,first,first_index,timeout,(unsigned long)observed,reg27);
}
static void measure_ph0(void){
 PIO pio=pio1;uint sm=pio_claim_unused_sm(pio,true);
 uint16_t ins[]={pio_encode_wait_gpio(true,8),pio_encode_wait_gpio(false,8),pio_encode_jmp_x_dec(0)};
 struct pio_program prog={.instructions=ins,.length=3,.origin=-1};uint offset=pio_add_program(pio,&prog);pio->instr_mem[offset+2]=pio_encode_jmp_x_dec(offset);
 pio_sm_config cfg=pio_get_default_sm_config();sm_config_set_wrap(&cfg,offset,offset+2);pio_sm_init(pio,sm,offset,&cfg);
 pio_sm_put_blocking(pio,sm,0xffffffffu);pio_sm_exec(pio,sm,pio_encode_pull(false,true));pio_sm_exec(pio,sm,pio_encode_mov(pio_x,pio_osr));
 uint32_t t=time_us_32();pio_sm_set_enabled(pio,sm,true);busy_wait_us_32(10000);pio_sm_set_enabled(pio,sm,false);uint32_t dt=time_us_32()-t;
 pio_sm_exec(pio,sm,pio_encode_mov(pio_isr,pio_x));pio_sm_exec(pio,sm,pio_encode_push(false,true));uint32_t count=0xffffffffu-pio_sm_get_blocking(pio,sm);
 pio_sm_unclaim(pio,sm);pio_remove_program(pio,&prog,offset);
 printf("PH0 counter=%lu elapsed_us=%lu Hz=%lu\n",(unsigned long)count,(unsigned long)dt,(unsigned long)(((uint64_t)count*1000000)/dt));
}
static void pio_check(void){
 atomic_store(&armed,false);while(atomic_load(&bus_ready))tight_loop_contents();
 atomic_store(&armed,true);while(!atomic_load(&bus_ready))tight_loop_contents();
 // Force pad OE LOW before synthesizing INTERNAL input levels.
 for(unsigned pin=0;pin<8;pin++)gpio_set_oeover(pin,GPIO_OVERRIDE_LOW);
 for(unsigned pin=9;pin<=13;pin++)gpio_set_inover(pin,(pin==9||pin==11||pin==13)?GPIO_OVERRIDE_HIGH:GPIO_OVERRIDE_LOW);
 busy_wait_us_32(100);
 uint32_t out=pio2->dbg_padout,oe=pio2->dbg_padoe,pins=sio_hw->gpio_in,pc=pio2->sm[0].addr;
 atomic_store(&armed,false);while(atomic_load(&bus_ready))tight_loop_contents();
 for(unsigned pin=9;pin<=13;pin++)gpio_set_inover(pin,GPIO_OVERRIDE_NORMAL);
 for(unsigned pin=0;pin<8;pin++)gpio_set_oeover(pin,GPIO_OVERRIDE_NORMAL);
 printf("PIOCHECK internal-control=5 output=%02lx oe=%02lx pc=%lu in=%08lx replies=%lu last=%08lx\n",(unsigned long)(out&255),(unsigned long)(oe&255),(unsigned long)pc,(unsigned long)pins,(unsigned long)read_replies,(unsigned long)last_reply);
 puts("PIOCHECK finished DISARMED; data output was hardware-inhibited");
}
// Read-only RESET pulse recorder. PIO0 samples GPIO13 without changing its mux or OE.
static uint32_t reset_width_samples[128];
static int reset_watch_dma=-1,reset_watch_sm=-1;
static uint reset_watch_offset;
static void reset_watch_start(void){
 if(reset_watch_dma<0){
  uint16_t ins[]={pio_encode_mov(pio_x,pio_osr),pio_encode_wait_gpio(false,13),pio_encode_jmp_pin(5),pio_encode_jmp_x_dec(2),pio_encode_jmp(5),pio_encode_mov(pio_isr,pio_x),pio_encode_push(false,false),pio_encode_wait_gpio(true,13),pio_encode_jmp(0)};
  struct pio_program prog={.instructions=ins,.length=9,.origin=-1};reset_watch_offset=pio_add_program(pio0,&prog);reset_watch_sm=pio_claim_unused_sm(pio0,true);reset_watch_dma=dma_claim_unused_channel(true);
 }
 pio_sm_set_enabled(pio0,reset_watch_sm,false);dma_channel_abort(reset_watch_dma);pio_sm_clear_fifos(pio0,reset_watch_sm);
 pio_sm_config cfg=pio_get_default_sm_config();sm_config_set_jmp_pin(&cfg,13);sm_config_set_wrap(&cfg,reset_watch_offset,reset_watch_offset+8);pio_sm_init(pio0,reset_watch_sm,reset_watch_offset,&cfg);
 pio_sm_put_blocking(pio0,reset_watch_sm,0xffffffffu);pio_sm_exec(pio0,reset_watch_sm,pio_encode_pull(false,true));
 dma_channel_config dc=dma_channel_get_default_config(reset_watch_dma);channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);channel_config_set_read_increment(&dc,false);channel_config_set_write_increment(&dc,true);channel_config_set_dreq(&dc,pio_get_dreq(pio0,reset_watch_sm,false));
 dma_channel_configure(reset_watch_dma,&dc,reset_width_samples,&pio0->rxf[reset_watch_sm],128,true);pio_sm_set_enabled(pio0,reset_watch_sm,true);puts("RESETWATCH started: passive GPIO13 pulse capture");
}
static void reset_watch_report(void){
 if(reset_watch_dma<0){puts("RESETWATCH not started");return;}
 pio_sm_set_enabled(pio0,reset_watch_sm,false);dma_channel_abort(reset_watch_dma);unsigned n=(dma_hw->ch[reset_watch_dma].write_addr-(uintptr_t)reset_width_samples)/sizeof(uint32_t);if(n>128)n=128;
 printf("RESETWATCH pulses=%u (capacity 128), measured low widths ns:",n);
 for(unsigned i=0;i<n;i++)printf(" %llu",(unsigned long long)(((uint64_t)(0xffffffffu-reset_width_samples[i])*2000000000ull)/clock_get_hz(clk_sys)));
 puts("");
}
static void command(char *line){
 if(!strcmp(line,"RESETWATCH"))reset_watch_start();
 else if(!strcmp(line,"WDOG"))printf("WDOG enabled=%u timeout_ms=%u remaining_ms=%lu timeout_boot=%u core1_seen=%u core1_heartbeat=%lu core1_age_ms=%lu feeds=%lu recovery_reset=%u uptime_ms=%lu\n",watchdog_started,EMULATOR_WATCHDOG_TIMEOUT_MS,(unsigned long)watchdog_get_time_remaining_ms(),watchdog_timeout_boot,watchdog_health.seen,(unsigned long)atomic_load_explicit(&core1_heartbeat,memory_order_relaxed),(unsigned long)(to_ms_since_boot(get_absolute_time())-watchdog_health.last_progress_ms),(unsigned long)watchdog_feeds,watchdog_timeout_boot&&reset_pulses!=0,(unsigned long)to_ms_since_boot(get_absolute_time()));
 else if(!strcmp(line,"WDOG TEST CORE0")||!strcmp(line,"WDOG TEST CORE1")){
  if(atomic_load(&armed)||atomic_load(&bus_ready)){puts("DISARM required before watchdog fault test");return;}
  if(!watchdog_started){puts("WATCHDOG NOT ENABLED");return;}
  bool core0=!strcmp(line,"WDOG TEST CORE0");puts(core0?"WDOG fault test: halting core0; hardware timeout will reboot":"WDOG fault test: halting core1; supervisor will stop feeding");stdio_flush();
  if(core0)for(;;)tight_loop_contents();
  atomic_store_explicit(&watchdog_test_core1_hang,true,memory_order_relaxed);
 }
 else if(!strcmp(line,"RESETREPORT"))reset_watch_report();
 else if(!strcmp(line,"PINS")){uint32_t p=sio_hw->gpio_in;printf("PINS RESET=%u IRQ_GATE=%u RESET_GATE=%u COMMAND=%u OPTION=%u raw=%08lx reset_pulses=%u\n",!!(p&RESET),!!(p&(1u<<15)),!!(p&(1u<<19)),!!(p&(1u<<20)),!!(p&(1u<<21)),(unsigned long)p,reset_pulses);}
 else if(!strcmp(line,"CMDSTATS")){printf("COMMANDS");for(unsigned i=0;i<256;i++)if(command_hist[i])printf(" %02x:%u",i,command_hist[i]);puts("");}
 else if(!strcmp(line,"PIOCHECK"))pio_check();
 else if(!strcmp(line,"PIOINFO")){printf("DMA lookups=%lu table=%08lx\n",(unsigned long)(address_dma<0?0:0x0fffffffu-dma_hw->ch[address_dma].transfer_count),(unsigned long)(uintptr_t)response_table);printf("PIO replies=%lu last=%08lx fdebug=%08lx tx=%u rx=%u\n",(unsigned long)read_replies,(unsigned long)last_reply,(unsigned long)pio2->fdebug,pio_sm_get_tx_fifo_level(pio2,0),pio_sm_get_rx_fifo_level(pio2,0));for(unsigned i=0;i<16;i++)printf("%x:%lu ",i,(unsigned long)read_headers[i]);puts("");}
 else if(!strcmp(line,"PADTEST")){printf("READPAD mismatches=%lu mask=%02lx expected=%02lx observed=%02lx oe=%02lx pio_out=%02lx\n",(unsigned long)atomic_load(&read_mismatch_count),(unsigned long)atomic_load(&read_mismatch_mask),(unsigned long)atomic_load(&last_read_expected),(unsigned long)atomic_load(&last_read_seen),(unsigned long)atomic_load(&last_read_oe),(unsigned long)atomic_load(&last_read_out));}
 else if(!strcmp(line,"FREQ"))measure_ph0();
 else if(!strcmp(line,"BUSTEST"))bus_selftest(false);
 else if(!strcmp(line,"BUSWRITE"))bus_selftest(true);
 else if(!strcmp(line,"STATUS"))print_status();
 else if(!strcmp(line,"LED"))printf("KEY_LED=%u any_key_down=%u\n",gpio_get(39),input_any_key_down());
 else if(!strcmp(line,"INPUT")){input_report();unsigned state=atomic_load_explicit(&public_key_state,memory_order_relaxed);printf("GLU key=%02x mods=%02x irq_enable=%02x system_status=%02x\n",state&255,(state>>8)&255,(state>>16)&255,atomic_load(&public_system_status));}
 else if(!strcmp(line,"SELFTEST"))printf("SELFTEST failures=%d\n",model_selftest());
 else if(!strcmp(line,"ARM")){
  if(atomic_load(&armed)){puts("ALREADY ARMED");return;}
  ctrl_reset(&controller);trace_head=0;
  atomic_store(&host_to_ctrl.head,0);atomic_store(&host_to_ctrl.tail,0);atomic_store(&ctrl_to_glu.head,0);atomic_store(&ctrl_to_glu.tail,0);
  atomic_store(&armed,true);while(!atomic_load(&bus_ready))tight_loop_contents();input_init();puts(atomic_load(&adb_poll)?"ARMED; ADB autopoll ON":"ARMED; ADB autopoll OFF");
 }else if(!strcmp(line,"DISARM")){
  atomic_store(&armed,false);while(atomic_load(&bus_ready))tight_loop_contents();puts("DISARMED; GPIO0..23 INPUT");
 }else if(!strcmp(line,"RESET")){if(atomic_load(&bus_ready)){system_reset_pulse();puts("SYSTEM RESET pulse 4ms");}else puts("ARM FIRST");}
 else if(!strcmp(line,"POLL ON")){atomic_store(&adb_poll,true);puts("ADB autopoll ON");}
 else if(!strcmp(line,"POLL OFF")){atomic_store(&adb_poll,false);puts("ADB autopoll OFF");}
 else if(!strcmp(line,"BOOT")){if(atomic_load(&armed)){puts("DISARM FIRST");return;}watchdog_started=false;watchdog_disable();reset_usb_boot(0,0);}
 else if(!strcmp(line,"TRACE")){printf("TRACE count=%u\n",trace_head);for(unsigned i=0;i<trace_head;i++){printf("%03u %c %02x\n",i,(trace[i]&0x100)?'R':'C',(unsigned)(trace[i]&255));watchdog_service(to_ms_since_boot(get_absolute_time()));}}
 else if(!strncmp(line,"KEY ",4)){input_keycode(strtoul(line+4,NULL,16));puts("KEY queued");}
 else puts("Commands: STATUS SELFTEST WDOG INPUT LED BUSTEST BUSWRITE ARM DISARM POLL ON POLL OFF TRACE KEY xx BOOT");
}
int main(void){
 watchdog_timeout_boot=watchdog_enable_caused_reboot();
 passive();gpio_init(39);gpio_set_dir(39,GPIO_OUT);stdio_init_all();
 watchdog_enable(EMULATOR_WATCHDOG_TIMEOUT_MS,false);watchdog_started=true;
 ctrl_reset(&controller);irq_set_exclusive_handler(PIO2_IRQ_0,read_service);irq_set_priority(PIO2_IRQ_0,0);irq_set_enabled(PIO2_IRQ_0,true);multicore_launch_core1(bus_core);
 int failures=model_selftest();bool automatic_start_done=false;uint32_t epoch=0;bool response_pending=false,last_led_state=false;unsigned response_sequence=0;char line[64];unsigned used=0;
 for(;;){
  uint32_t now=to_ms_since_boot(get_absolute_time());
  watchdog_service(now);
  if(!automatic_start_done && now>=750){automatic_start_done=true;if(!failures){char arm_command[]="ARM";command(arm_command);if(watchdog_timeout_boot){busy_wait_us_32(1000);system_reset_pulse();}}}
  if(atomic_load(&bus_ready)){
   unsigned current=atomic_load_explicit(&reset_epoch,memory_order_acquire);
   if(current!=epoch){epoch=current;trace_head=0;ctrl_host_reset(&controller);input_host_reset();response_pending=false;}
   uint32_t data;
   if(!atomic_load_explicit(&host_reset,memory_order_acquire)&&pop(&host_to_ctrl,&data)&&((data>>16)==(epoch&0xffff))){
    data&=255;
    if(trace_head<1024)trace[trace_head++]=data;
    if(!controller.needed)command_hist[data]++;
    ctrl_feed(&controller,data);glu_write(8,0);actions();
   }
   if(response_pending&&atomic_load_explicit(&response_applied,memory_order_acquire)!=response_sequence)response_pending=false;
   uint8_t status=atomic_load_explicit(&public_status,memory_order_acquire);
   /* Unsolicited status must not split a multi-byte command reply or
    * overwrite its pending C026 byte. Keep it latched in input until idle. */
   if(!controller.needed&&controller.head==controller.tail&&!response_pending&&!(status&0x20)&&atomic_load(&ctrl_to_glu.head)==atomic_load(&ctrl_to_glu.tail)){
    uint8_t note=input_take_notification();if(note)put(&controller,note);
   }
   if(!response_pending&&!(status&0x20)&&atomic_load(&ctrl_to_glu.head)==atomic_load(&ctrl_to_glu.tail)){
    uint8_t byte;if(ctrl_pop(&controller,&byte)){response_sequence=atomic_load_explicit(&response_applied,memory_order_acquire);glu_write(7,byte);response_pending=true;if(trace_head<1024)trace[trace_head++]=0x100|byte;}
   }
   if(atomic_load(&adb_poll))input_service(now,status,controller.modes,controller.config[0],controller.synced);
  }
  int ch=getchar_timeout_us(0);if(ch>=0){if(ch=='\r'||ch=='\n'){if(used){line[used]=0;command(line);used=0;}}else if(used<sizeof(line)-1)line[used++]=(char)ch;}
  bool led_state=failures||(atomic_load(&bus_ready)&&input_any_key_down());
  if(led_state!=last_led_state){gpio_put(39,led_state);last_led_state=led_state;}
 }
}
