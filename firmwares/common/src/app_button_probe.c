/* Boot-only diagnostic, deliberately separate from functional button support. */
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "PeripheralRegs.h"
#include "dbg.h"
#include "dbg_uart.h"
#include "app_button_probe.h"
#include "button_probe_filter.h"
#include "sensor_timing.h"

/* Do not configure either documented SHT route; observe DIO16/17 passively.
 * Actual CRC-valid SHT measurements independently established DIO14/15. */
#define PROBE_EXCLUDED ((1UL << 6) | (1UL << 7) | (1UL << 14) | \
                        (1UL << 15))
#define PROBE_MASK (((1UL << 20) - 1) & ~PROBE_EXCLUDED)
#define PROBE_INPUT_MASK (PROBE_MASK & ~((1UL << 16) | (1UL << 17)))
#define PROBE_SAMPLES 9000U /* At least 45 seconds, then normal v6 operation. */

PRIVATE uint32_t probe_tick(void *context)
{
    (void)context;
    return u32AHI_TickTimerRead();
}

PRIVATE bool_t probe_delay(void)
{
    vAHI_WatchdogRestart();
    /* Read the SDK 1 ms tick without resetting/reconfiguring its timer. */
    return sht_wait_ticks(probe_tick, NULL, 16000UL, 80000UL, 500000UL);
}

PUBLIC void APP_vButtonProbe(void)
{
    button_probe_filter filter;
    uint32 input, changed, seen = 0, candidates;
    unsigned sample, dio;
    DBG_vPrintf(TRUE, "\nBUTTON diagnostic v7: UART0 two-wire; no button mapping assumed\n");
    DBG_vPrintf(TRUE, "BUTTON before inputs DIR=%08x DIN=%08x PULLUP=%08x\n",
                u32REG_GpioRead(REG_GPIO_DIR), u32AHI_DioReadInput(),
                u32REG_SysRead(REG_SYS_PULLUP));
    /* Release candidate GPIOs as inputs with weak pull-ups, never drive an
     * unknown output. RTS/CTS have already been released before UART enable.
     * DO0/DO1, UART6/7 and both I2C routes are excluded from this operation. */
    vAHI_DioSetDirection(PROBE_INPUT_MASK, 0);
    vAHI_DioSetPullup(PROBE_INPUT_MASK, 0);
    for (sample = 0; sample < 10; ++sample) {
        if (!probe_delay()) {
            DBG_vPrintf(TRUE, "BUTTON probe aborted: tick-timeout during settle\n");
            return;
        }
    }
    input = u32AHI_DioReadInput();
    button_probe_init(&filter, PROBE_MASK, input);
    DBG_vPrintf(TRUE, "BUTTON baseline mask=%08x DIN=%08x DIR=%08x\n",
                (uint32)PROBE_MASK, input, u32REG_GpioRead(REG_GPIO_DIR));
    DBG_vPrintf(TRUE, "BUTTON READY: press/release ORIGINAL PCB button 3 times; hold each state 1 second\n");
    DBG_vPrintf(TRUE, "BUTTON observe for 45 seconds; keep S1 OPEN; do not press S2 RESET\n");
    DBG_vUartFlush();
    for (sample = 0; sample < PROBE_SAMPLES; ++sample) {
        if (!probe_delay()) {
            DBG_vPrintf(TRUE, "BUTTON probe aborted: tick-timeout sample=%d\n", sample);
            return;
        }
        input = u32AHI_DioReadInput();
        changed = button_probe_update(&filter, input);
        if (!changed) continue;
        seen |= changed;
        DBG_vPrintf(TRUE, "BUTTON edge sample=%d changed=%08x stable=%08x raw=%08x\n",
                    sample, changed, filter.stable, input);
        for (dio = 0; dio < BUTTON_PROBE_DIOS; ++dio) {
            if (changed & (1UL << dio))
                DBG_vPrintf(TRUE, "BUTTON DIO%d level=%d baseline=%d edges=%d\n", dio,
                            (unsigned)((filter.stable >> dio) & 1),
                            (unsigned)((filter.baseline >> dio) & 1), filter.edges[dio]);
        }
        DBG_vUartFlush();
    }
    candidates = button_probe_candidates(&filter);
    for (dio = 0; dio < BUTTON_PROBE_DIOS; ++dio) {
        if (candidates & (1UL << dio))
            DBG_vPrintf(TRUE, "BUTTON candidate DIO%d baseline=%d opposite=%d edges=%d\n", dio,
                        (unsigned)((filter.baseline >> dio) & 1),
                        (unsigned)(((filter.baseline >> dio) & 1) ^ 1), filter.edges[dio]);
    }
    DBG_vPrintf(TRUE, "BUTTON DONE seen=%08x candidates=%08x; correlate with physical presses\n",
                seen, candidates);
    DBG_vPrintf(TRUE, "BUTTON diagnostic ended; normal sensor/network starts; button action still disabled\n");
    DBG_vUartFlush();
}
