#ifndef SENSOR_GPIO_I2C_H
#define SENSOR_GPIO_I2C_H
#include <stdint.h>

/* Diagnostic-only open-drain transport. A line is either LOW or released;
 * the implementation never asks the adapter to drive either line HIGH. */
enum { SHT_GPIO_SCL = 0, SHT_GPIO_SDA = 1 };
typedef struct {
    void (*low)(unsigned line, int enabled, void *context);
    int (*level)(unsigned line, void *context);
    int (*wait_us)(uint32_t us, void *context);
    void (*stage)(const char *name, unsigned byte, void *context);
} sht_gpio_io;

typedef struct {
    const sht_gpio_io *io;
    void *context;
    uint32_t half_period_us;
    unsigned fault; /* Same fault IDs as the persistent diagnostic protocol. */
    uint8_t tx, rx;
} sht_gpio_bus;

int sht_gpio_command_read(sht_gpio_bus *bus, uint8_t address, uint16_t command,
                          uint32_t wait_us, uint8_t *data, unsigned length);
#endif
