#ifndef LED_PROBE_H
#define LED_PROBE_H
#include <stdint.h>

/* Never exercise the confirmed button, UART or actual SHT30 bus. */
#define LED_PROBE_EXCLUDED ((1UL<<5)|(1UL<<6)|(1UL<<7)|(1UL<<14)|(1UL<<15))
#define LED_PROBE_MASK (((1UL<<20)-1) & ~LED_PROBE_EXCLUDED)
typedef struct {
    uint32_t (*input)(void *);
    uint32_t (*direction)(void *);
    uint32_t (*output)(void *);
    uint32_t (*pullup_disabled)(void *);
    void (*set_direction)(void *, uint32_t inputs, uint32_t outputs);
    void (*set_output)(void *, uint32_t high, uint32_t low);
    void (*set_pullup)(void *, uint32_t on, uint32_t off);
    int (*wait_ms)(void *, unsigned ms);
    void (*log)(void *, const char *stage, unsigned dio, int level);
} led_probe_io;

/* Human observation identifies the LED; this never assigns/saves a pin.
 * A digital input profile cannot prove the PCB circuit or absolute current.
 * Drive only previously stable levels, one eligible pin at a time, 200 ms.
 * Skip existing outputs/unstable inputs and restore the pin on every exit. */
int led_probe_run(const led_probe_io *io, void *context);
/* Owner-requested v11: deliberately tries LOW on unresolved HIGH pins.
 * Readback bounds duration, not current; unknown loads can still contend. */
#define LED_PROBE_LOW_REMAINING_MASK ((1UL<<9)|(1UL<<12)|(1UL<<13))
int led_probe_low_remaining_run(const led_probe_io *io, void *context);
#endif
