#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sensor_gpio_i2c.h"

/* Edge-driven slave: reconstruct actual bytes and ACK clocks from GPIO changes.
 * No calls into the master's byte or transaction functions. */
typedef struct {
    int low[2], slave_sda, active, sending, address, clocked, ended;
    unsigned clocks, starts, stops, read_bytes, ack_count, nack_count;
    unsigned writes, timeouts, stretch, stretch_reads, levels, delays;
    int reject, stuck_sda, stuck_scl, fail_wait, collide;
    uint8_t byte, written[16], reply[6];
    const char *stage;
    unsigned stage_byte, conversion_waits;
} slave;
static int sda(slave *s) {return !(s->low[1] || s->slave_sda || s->stuck_sda);}
static void set_low(unsigned line, int enabled, void *context)
{
    slave *s=context;
    int before=sda(s), old=s->low[line];
    assert(line<2 && (enabled==0 || enabled==1));
    s->low[line]=enabled;
    if(line==SHT_GPIO_SDA && !s->low[SHT_GPIO_SCL]) {
        if(before && !sda(s)) {
            ++s->starts;s->active=1;s->address=1;s->sending=0;
            s->clocks=0;s->clocked=0;s->byte=0;s->ended=0;
        } else if(!before && sda(s)) {
            ++s->stops;s->active=0;s->slave_sda=0;
        }
    }
    if(line!=SHT_GPIO_SCL || old==enabled || !s->active) return;
    if(!enabled) {
        s->clocked=1;
        if(s->clocks<8) {
            if(!s->sending) s->byte=(uint8_t)((s->byte<<1)|sda(s));
            else assert(!s->low[SHT_GPIO_SDA]);
        } else if(s->sending) {
            if(sda(s)) {++s->nack_count;s->ended=1;} else ++s->ack_count;
            ++s->read_bytes;
        }
        ++s->clocks;
    } else if(s->clocked) {
        s->clocked=0;
        if(s->clocks==9) {
            if(s->ended) {s->active=0;s->slave_sda=0;return;}
            if(!s->sending) {
                assert(s->writes<16);s->written[s->writes++]=s->byte;
                if(s->address) {s->sending=!!(s->byte&1);s->address=0;}
            }
            s->byte=0;s->clocks=0;
        }
        if(s->clocks==8) s->slave_sda=!s->sending && !s->reject;
        else if(s->sending && s->read_bytes<6)
            s->slave_sda=!(s->reply[s->read_bytes] & (0x80u>>s->clocks));
        else s->slave_sda=0;
    }
}
static int level(unsigned line, void *context)
{
    slave *s=context;++s->levels;
    if(line==SHT_GPIO_SCL) {
        if(s->stuck_scl) return 0;
        if(s->stretch_reads<s->stretch) {++s->stretch_reads;return 0;}
        return !s->low[0];
    }
    if(s->collide && s->active && s->clocks && s->clocks<8) return 0;
    return sda(s);
}
static int wait_us(uint32_t us, void *context)
{
    slave *s=context;++s->delays;
    if(us==20000) {++s->conversion_waits; assert(s->stops==1 && !s->active);}
    return !s->fail_wait;
}
static void stage(const char *name,unsigned byte,void *context)
{
    slave *s=context;s->stage=name;s->stage_byte=byte;
}
static const sht_gpio_io io={set_low,level,wait_us,stage};
static sht_gpio_bus setup(slave *s)
{
    const uint8_t reply[]={0x65,0x75,0xae,0x99,0x1c,0x31};
    sht_gpio_bus bus={&io,s,100,0,0,0};
    memset(s,0,sizeof(*s));memcpy(s->reply,reply,6);return bus;
}
static void released(const slave *s) {assert(!s->low[0] && !s->low[1]);}
int main(void)
{
    slave s;
    uint8_t out[6];
    sht_gpio_bus bus=setup(&s);
    assert(sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(!bus.fault && !memcmp(out,s.reply,6));
    assert(s.starts==2 && s.stops==2 && s.writes==4);
    assert(!memcmp(s.written,"\x88\x24\x16\x89",4));
    assert(s.ack_count==5 && s.nack_count==1 && s.conversion_waits==1);
    assert(!strcmp(s.stage,"complete") && s.stage_byte==5);released(&s);

    bus=setup(&s);s.reject=1;
    assert(!sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(bus.fault==1 && !strcmp(s.stage,"write-address"));
    assert(s.writes==1 && s.stops==1 && !s.conversion_waits);released(&s);

    bus=setup(&s);s.stuck_sda=1;
    assert(!sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(bus.fault==6 && !s.writes && !s.conversion_waits);released(&s);
    bus=setup(&s);s.stuck_scl=1;
    assert(!sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(bus.fault==3 && s.levels<=202 && !s.conversion_waits);released(&s);
    bus=setup(&s);s.fail_wait=1;
    assert(!sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(bus.fault==2);released(&s);
    bus=setup(&s);s.collide=1;
    assert(!sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(bus.fault==4 && !s.conversion_waits);released(&s);
    bus=setup(&s);s.stretch=3;
    assert(sht_gpio_command_read(&bus,0x44,0x2416,20000,out,6));
    assert(s.stretch_reads==3 && !memcmp(out,s.reply,6));released(&s);
    bus=setup(&s);
    assert(sht_gpio_command_read(&bus,0x44,0x30a2,2000,0,0));
    assert(s.starts==1 && s.stops==1 && s.writes==3);
    assert(!memcmp(s.written,"\x88\x30\xa2",3));released(&s);
    return 0;
}
