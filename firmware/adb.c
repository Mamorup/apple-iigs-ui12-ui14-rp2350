// Electrical polarity: GPIO18 HIGH enables motherboard Q4 => ADB LOW.
#include "hal.h"
#include "pico/stdlib.h"
static void release(void){gpio_put(18,0);}
static void low(void){gpio_put(18,1);}
static bool wait_level(bool high,uint32_t timeout){uint32_t start=time_us_32();while(gpio_get(17)!=high){if((uint32_t)(time_us_32()-start)>=timeout)return false;}return true;}
static void cell(bool one){low();busy_wait_us_32(one?35:65);release();busy_wait_us_32(one?65:35);}
static void byte(uint8_t v){for(unsigned m=128;m;m>>=1)cell(v&m);}
static void command(uint8_t v){low();busy_wait_us_32(800);release();busy_wait_us_32(70);byte(v);cell(0);}
void adb_phy_init(void){gpio_init(17);gpio_set_dir(17,GPIO_IN);gpio_disable_pulls(17);gpio_init(18);release();gpio_set_dir(18,GPIO_OUT);}
void adb_phy_reset(void){low();busy_wait_us_32(4000);release();}
uint8_t adb_phy_talk(uint8_t cmd,uint8_t *data,uint8_t cap){uint8_t n=0,bits=0,v=0;bool start=true;
 if(!gpio_get(17))return 0;
 command(cmd);if(!wait_level(0,260))goto done;
 while(n<cap){uint32_t t=time_us_32();if(!wait_level(1,130))break;uint32_t width=time_us_32()-t;
  if(!wait_level(0,130))break;
  if(start){start=false;continue;}
  v=(v<<1)|(width<50);if(++bits==8){data[n++]=v;bits=0;v=0;}}
 done:release();return n;}
void adb_phy_listen(uint8_t cmd,const uint8_t *data,uint8_t n){if(!gpio_get(17))return;command(cmd);busy_wait_us_32(160);cell(1);for(unsigned i=0;i<n;i++)byte(data[i]);cell(0);release();}
