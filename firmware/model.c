#include "model.h"
#include <string.h>
void glu_reset(glu_t *g){memset(g,0,sizeof(*g));}
__attribute__((always_inline)) inline uint8_t glu_status(const glu_t *g){return (g->mouse_full?0x80:0)|(g->command_full?0x40:0)|(g->data_full?0x20:0)|(g->key_strobe?0x10:0)|(g->anykey&0x80?1:0);}
__attribute__((always_inline)) inline bool glu_irq(const glu_t *g){return ((g->irq_enable&1)&&g->key_strobe)||((g->irq_enable&2)&&g->data_full)||((g->irq_enable&4)&&g->mouse_full);}
__attribute__((always_inline)) inline uint8_t glu_host_peek(const glu_t *g,uint8_t s){
 switch(s){case 0:return g->key|(g->key_strobe?0x80:0);case 1:return g->anykey&0x80;
 case 2:return (g->mouse_full?0x80:0)|((g->irq_enable&4)?0x40:0)|(g->data_full?0x20:0)|((g->irq_enable&2)?0x10:0)|(g->key_strobe?8:0)|((g->irq_enable&1)?4:0)|(g->y_phase?2:0)|(g->command_full?1:0);
 case 3:return g->response;case 4:return g->y_phase?g->my:g->mx;case 5:return g->mods;default:return 0xff;}
}
__attribute__((always_inline)) inline void glu_host_end(glu_t *g,uint8_t s,bool w,uint8_t d){
 if(s==1)g->key_strobe=false;
 if(w){if(s==3){g->command=d;g->command_full=true;}else if(s==2)g->irq_enable=((d>>2)&1)|((d>>3)&2)|((d>>4)&4);}
 else{if(s==2)g->status_read=true;
  if(s==3&&g->status_read){g->data_full=false;g->status_read=false;}
  if(s==4){if(g->y_phase){g->y_phase=false;g->mouse_full=false;}else g->y_phase=true;}}
}
__attribute__((always_inline)) inline void glu_mcu_write(glu_t *g,uint8_t r,uint8_t d){switch(r){
 case 0:g->key=d&0x7f;g->key_strobe=true;break;case 2:g->mx=d;g->mouse_full=true;g->y_phase=false;break;
 case 3:g->my=d;break;case 5:g->anykey=d;break;case 6:g->mods=d;break;
 case 7:g->response=d;g->data_full=true;g->status_read=false;break;
 case 8:g->command_full=false;break; /* internal command ACK, not an external GLU register */
 default:break;}}
static void put(controller_t *c,uint8_t d){uint8_t n=(c->head+1)&63;if(n==c->tail)c->error|=1;else{c->reply[c->head]=d;c->head=n;}}
bool ctrl_pop(controller_t *c,uint8_t *d){if(c->head==c->tail)return false;*d=c->reply[c->tail];c->tail=(c->tail+1)&63;return true;}
void ctrl_reset(controller_t *c){memset(c,0,sizeof(*c));c->config[0]=0x32;c->config[2]=0x33;c->ram[1]=0xea;c->ram[3]=0xaa;}
/* The original UI14's RC reset is independent of system RESET. Preserve its
 * private RAM/modes/configuration; only discard the interrupted host exchange.
 * ROM01 checks RAM[$51]==$A5 and clears CPU $03F3/$03F4 if that byte is lost. */
void ctrl_host_reset(controller_t *c){
 c->head=c->tail=0;c->cmd=c->needed=c->received=0;
 c->action=c->physical=c->length=c->keycode=0;c->synced=false;
 memset(c->args,0,sizeof(c->args));memset(c->payload,0,sizeof(c->payload));
}
void ctrl_adb_reply(controller_t *c,const uint8_t *d,uint8_t n){if(n){put(c,0x80|(n-1));while(n)put(c,d[--n]);}}
// The input translator implements US layout/character set 0 only. Advertise
// exactly that list; the host reads count+1 bytes and times out on a short list.
static const uint8_t supported_charsets[]={0};
static const uint8_t supported_layouts[]={0};
static void put_id_list(controller_t *c,const uint8_t *ids,uint8_t n){put(c,n);for(unsigned i=0;i<n;i++)put(c,ids[i]);}
static uint8_t count(uint8_t cmd){switch(cmd){case 4:case 5:case 0x11:case 0x48:return 1;case 8:case 9:return 2;case 6:return 3;case 7:return 4;default:if(cmd>=0x49&&cmd<=0x4f)return (cmd&7)+2;if((cmd&0xc0)==0x80)return 2;return 0;}}
static void execute(controller_t *c){uint8_t cmd=c->cmd;uint16_t addr; c->commands++;
 switch(cmd){
 case 1:c->head=c->tail=0;break;case 2:ctrl_reset(c);break;case 3:c->action=3;break;
 case 4:c->modes|=c->args[0];break;case 5:c->modes&=~c->args[0];break;case 6:memcpy(c->config,c->args,3);break;
 case 7:c->modes=c->args[0];memcpy(c->config,c->args+1,3);c->synced=true;c->action=1;break;
 case 8:if(c->args[0]<sizeof(c->ram))c->ram[c->args[0]]=c->args[1];else c->error|=2;break;
 case 9:addr=c->args[0]|((uint16_t)c->args[1]<<8);
  if(addr>=0x1000&&addr<0x2000){c->rom_reads++;put(c,addr==0x1400?0x72:addr==0x1401?0xf7:0);}
  else if(addr<sizeof(c->ram))put(c,c->ram[addr]);else{put(c,0);c->error|=2;}
  break;
 case 0x0a:put(c,c->modes);break;case 0x0b:for(int i=0;i<3;i++)put(c,c->config[i]);break;
 case 0x0c:put(c,c->error);c->error=0;break;case 0x0d:put(c,5);break;
 case 0x0e:put_id_list(c,supported_charsets,sizeof(supported_charsets));break;case 0x0f:put_id_list(c,supported_layouts,sizeof(supported_layouts));break;
 case 0x10:c->action=2;break;case 0x11:c->keycode=c->args[0];c->action=4;break;
 case 0x40:c->action=1;c->synced=true;break;
 case 0x48:c->action=5;c->physical=c->args[0];break;
 default:
  if((cmd&0xf0)==0x50)c->srq|=(uint16_t)1<<(cmd&15);
  else if((cmd&0xf0)==0x70)c->srq&=~((uint16_t)1<<(cmd&15));
  else if((cmd&0xf0)==0x60){c->action=6;c->physical=(cmd<<4)|1;c->length=0;}
  else if((cmd&0xc0)==0xc0){c->action=5;c->physical=((cmd&15)<<4)|(((cmd>>4)&3)<<2)|12;}
  else if((cmd&0xc0)==0x80){c->action=6;c->physical=((cmd&15)<<4)|(((cmd>>4)&3)<<2)|8;c->length=2;memcpy(c->payload,c->args,2);}
  else if(cmd>=0x49&&cmd<=0x4f){c->action=6;c->physical=c->args[0];c->length=(cmd&7)+1;memcpy(c->payload,c->args+1,c->length);}
  else c->error|=4;
  break;
 }
}
void ctrl_feed(controller_t *c,uint8_t d){if(c->needed){c->args[c->received++]=d;if(c->received==c->needed){c->needed=0;execute(c);}}else{c->cmd=d;c->received=0;c->needed=count(d);if(!c->needed)execute(c);}}
int model_selftest(void){int errors=0;glu_t g;controller_t c;uint8_t d=0;glu_reset(&g);ctrl_reset(&c);
 #define CHECK(x) do{if(!(x))errors++;}while(0)
 CHECK(glu_host_peek(&g,2)==0);CHECK(!glu_irq(&g));
 glu_host_end(&g,2,true,0x54);CHECK(glu_host_peek(&g,2)==0x54);
 glu_mcu_write(&g,0,0x41);CHECK(glu_host_peek(&g,0)==0xc1);CHECK(glu_irq(&g));
 glu_host_end(&g,1,false,0);CHECK(!g.key_strobe);CHECK(!glu_irq(&g));
 glu_mcu_write(&g,0,0xe1);CHECK(glu_host_peek(&g,0)==0xe1);
 glu_host_end(&g,1,false,0);CHECK(glu_host_peek(&g,0)==0x61);
 glu_host_end(&g,3,true,0x09);CHECK(g.command_full&&g.command==9);glu_mcu_write(&g,8,0);CHECK(!g.command_full);
 glu_mcu_write(&g,7,0xa5);glu_host_end(&g,3,false,0);CHECK(g.data_full);
 glu_host_end(&g,2,false,0);CHECK(glu_host_peek(&g,3)==0xa5);glu_host_end(&g,3,false,0);CHECK(!g.data_full);
 glu_mcu_write(&g,3,0x12);glu_mcu_write(&g,2,0x34);CHECK(glu_host_peek(&g,4)==0x34);glu_host_end(&g,4,false,0);CHECK(glu_host_peek(&g,4)==0x12);glu_host_end(&g,4,false,0);CHECK(!g.mouse_full);
 ctrl_feed(&c,7);ctrl_feed(&c,0);ctrl_feed(&c,0x32);ctrl_feed(&c,0);ctrl_feed(&c,0x33);CHECK(c.synced&&c.config[0]==0x32&&c.config[2]==0x33);c.action=0;
 for(int a=0;a<96;a++){ctrl_feed(&c,8);ctrl_feed(&c,a);ctrl_feed(&c,a^0x5a);ctrl_feed(&c,9);ctrl_feed(&c,a);ctrl_feed(&c,0);CHECK(ctrl_pop(&c,&d)&&d==(uint8_t)(a^0x5a));}
 for(int a=0x1000;a<0x2000;a++){ctrl_feed(&c,9);ctrl_feed(&c,a);ctrl_feed(&c,a>>8);CHECK(ctrl_pop(&c,&d)&&d==(a==0x1400?0x72:a==0x1401?0xf7:0));}
 ctrl_feed(&c,0x80);CHECK(c.needed==2);ctrl_feed(&c,1);ctrl_feed(&c,2);CHECK(c.action==6&&c.length==2);c.action=0;
 for(unsigned v=0;v<256;v++){glu_reset(&g);glu_host_end(&g,2,true,v);CHECK((glu_host_peek(&g,2)&0x54)==(v&0x54));}
 // Protocol regression: every advertised capability ID must actually follow
 // its count byte, with no trailing bytes or missing IDs (old 08/0A,00 failed).
 for(unsigned cmd=0x0e;cmd<=0x0f;cmd++){
  ctrl_reset(&c);ctrl_feed(&c,cmd);uint8_t n=0;CHECK(ctrl_pop(&c,&n));CHECK(n==1);
  for(unsigned i=0;i<n;i++){CHECK(ctrl_pop(&c,&d));CHECK(d==0);}
  CHECK(!ctrl_pop(&c,&d));
 }
 // A CPU RESET must not look like a freshly powered-on keyboard MCU.
 ctrl_reset(&c);ctrl_feed(&c,8);ctrl_feed(&c,0x51);ctrl_feed(&c,0xa5);
 c.modes=0x55;c.config[0]=0x23;
 ctrl_feed(&c,9);ctrl_feed(&c,0x51);CHECK(c.needed!=0);
 ctrl_host_reset(&c);CHECK(!c.synced&&!c.needed&&!c.received&&!c.action);
 CHECK(c.modes==0x55&&c.config[0]==0x23&&c.ram[0x51]==0xa5);
 CHECK(!ctrl_pop(&c,&d));ctrl_feed(&c,9);ctrl_feed(&c,0x51);ctrl_feed(&c,0);
 CHECK(ctrl_pop(&c,&d)&&d==0xa5);
 ctrl_feed(&c,7);ctrl_feed(&c,0);ctrl_feed(&c,0x32);ctrl_feed(&c,0);ctrl_feed(&c,0x24);
 CHECK(c.synced&&c.ram[0x51]==0xa5);
 ctrl_feed(&c,2);CHECK(c.ram[0x51]==0); // explicit Reset MCU still resets RAM
 #undef CHECK
 return errors;
}
