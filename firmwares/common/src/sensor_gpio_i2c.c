#include "sensor_gpio_i2c.h"
#include "sensor_wire.h"

static void low(sht_gpio_bus *b, unsigned line, int enabled)
{
    b->io->low(line, enabled, b->context);
}
static int delay(sht_gpio_bus *b, uint32_t us)
{
    if (b->io->wait_us(us, b->context)) return 1;
    b->fault = 2; /* tick-timeout */
    return 0;
}
static int clock_high(sht_gpio_bus *b)
{
    unsigned budget = 200; /* At most 1 ms of clock stretching per edge. */
    low(b, SHT_GPIO_SCL, 0);
    while (!b->io->level(SHT_GPIO_SCL, b->context)) {
        if (!budget--) {b->fault = 3; return 0;}
        if (!delay(b, 5)) return 0;
    }
    return delay(b, b->half_period_us);
}
static int start(sht_gpio_bus *b)
{
    low(b, SHT_GPIO_SDA, 0);
    if (!clock_high(b)) return 0;
    if (!b->io->level(SHT_GPIO_SDA, b->context)) {
        b->fault = 6; /* bus-stuck */
        return 0;
    }
    low(b, SHT_GPIO_SDA, 1);
    if (!delay(b, b->half_period_us)) return 0;
    low(b, SHT_GPIO_SCL, 1);
    return 1;
}
static int stop(sht_gpio_bus *b)
{
    low(b, SHT_GPIO_SCL, 1);
    low(b, SHT_GPIO_SDA, 1);
    if (!delay(b, b->half_period_us) || !clock_high(b)) return 0;
    low(b, SHT_GPIO_SDA, 0);
    if (!delay(b, b->half_period_us)) return 0;
    if (!b->io->level(SHT_GPIO_SDA, b->context)) {
        b->fault = 6;
        return 0;
    }
    return 1;
}
static int write_byte(sht_gpio_bus *b)
{
    unsigned bit;
    int ack;
    for (bit = 0; bit < 8; ++bit) {
        int one = !!(b->tx & (0x80u >> bit));
        low(b, SHT_GPIO_SDA, !one);
        if (!delay(b, b->half_period_us) || !clock_high(b)) return 0;
        if (one && !b->io->level(SHT_GPIO_SDA, b->context)) {
            b->fault = 4; /* arbitration-lost: released SDA was held low */
            return 0;
        }
        low(b, SHT_GPIO_SCL, 1);
    }
    low(b, SHT_GPIO_SDA, 0);
    if (!delay(b, b->half_period_us) || !clock_high(b)) return 0;
    ack = !b->io->level(SHT_GPIO_SDA, b->context);
    low(b, SHT_GPIO_SCL, 1);
    if (!ack) {b->fault = 1; return 0;}
    return 1;
}
static int read_byte(sht_gpio_bus *b, int nack)
{
    unsigned bit;
    b->rx = 0;
    low(b, SHT_GPIO_SDA, 0);
    for (bit = 0; bit < 8; ++bit) {
        if (!delay(b, b->half_period_us) || !clock_high(b)) return 0;
        b->rx = (uint8_t)((b->rx << 1) | !!b->io->level(SHT_GPIO_SDA, b->context));
        low(b, SHT_GPIO_SCL, 1);
    }
    low(b, SHT_GPIO_SDA, !nack);
    if (!delay(b, b->half_period_us) || !clock_high(b)) return 0;
    low(b, SHT_GPIO_SCL, 1);
    low(b, SHT_GPIO_SDA, 0);
    return 1;
}
static void address(uint8_t addr, int read, void *context)
{
    sht_gpio_bus *b = context;
    b->tx = (uint8_t)((addr << 1) | !!read);
}
static void write(uint8_t value, void *context) {((sht_gpio_bus *)context)->tx = value;}
static uint8_t read(void *context) {return ((sht_gpio_bus *)context)->rx;}
static int transfer(int begin, int end, int receive, int nack, void *context)
{
    sht_gpio_bus *b = context;
    if (begin && !start(b)) return 0;
    if (!(receive ? read_byte(b, nack) : write_byte(b))) return 0;
    return !end || stop(b);
}
static int wait_us(uint32_t us, void *context) {return delay(context, us);}
static void stage(const char *name, unsigned byte, void *context)
{
    sht_gpio_bus *b = context;
    b->io->stage(name, byte, b->context);
}
int sht_gpio_command_read(sht_gpio_bus *b, uint8_t addr, uint16_t command,
                          uint32_t wait, uint8_t *data, unsigned length)
{
    static const sht_wire_io wire = {address, write, read, transfer, wait_us, stage};
    int result;
    unsigned fault;
    if (!b || !b->io || !b->io->low || !b->io->level || !b->io->wait_us ||
        !b->io->stage || !b->half_period_us) return 0;
    b->fault = 0;
    result = sht_command_read(&wire, b, addr, command, wait, data, length);
    fault = b->fault;
    /* Try a STOP after a NACK. On arbitration loss or stuck lines just release
     * them; never force a HIGH or leave either output actively LOW. */
    if (!result && fault == 1) (void)stop(b);
    if (fault) b->fault = fault;
    low(b, SHT_GPIO_SCL, 0);
    low(b, SHT_GPIO_SDA, 0);
    return result;
}
