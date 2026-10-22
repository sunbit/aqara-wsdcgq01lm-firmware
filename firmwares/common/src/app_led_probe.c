#include <jendefs.h>
#include "AppHardwareApi.h"
#include "PeripheralRegs.h"
#include "dbg.h"
#include "dbg_uart.h"
#include "sensor_timing.h"
#include "led_probe.h"
#include "app_led_probe.h"

PRIVATE uint32_t input(void *ctx) {(void)ctx; return u32AHI_DioReadInput();}
PRIVATE uint32_t direction(void *ctx) {(void)ctx; return u32REG_GpioRead(REG_GPIO_DIR);}
PRIVATE uint32_t output(void *ctx) {(void)ctx; return u32REG_GpioRead(REG_GPIO_DOUT);}
PRIVATE uint32_t pullups(void *ctx) {(void)ctx; return u32REG_SysRead(REG_SYS_PULLUP);}
PRIVATE void set_direction(void *ctx, uint32_t in, uint32_t out) {(void)ctx; vAHI_DioSetDirection(in, out);}
PRIVATE void set_output(void *ctx, uint32_t hi, uint32_t lo) {(void)ctx; vAHI_DioSetOutput(hi, lo);}
PRIVATE void set_pullup(void *ctx, uint32_t on, uint32_t off) {(void)ctx; vAHI_DioSetPullup(on, off);}
PRIVATE uint32_t tick(void *ctx) {(void)ctx; return u32AHI_TickTimerRead();}
PRIVATE int wait_ms(void *ctx, unsigned ms)
{
    (void)ctx;
    while (ms--) {
        vAHI_WatchdogRestart();
        if (!sht_wait_ticks(tick, NULL, 16000UL, 16000UL, 200000UL)) return 0;
    }
    return 1;
}
PRIVATE void log_stage(void *ctx, const char *stage, unsigned dio, int level)
{
    (void)ctx;
    DBG_vPrintf(TRUE, "LED PROBE DIO%d %s level=%d\n", dio, stage, level);
    DBG_vUartFlush();
}
PRIVATE const led_probe_io io = {
    input, direction, output, pullups, set_direction, set_output, set_pullup, wait_ms, log_stage
};
PUBLIC void APP_vLedProbe(void)
{
    int complete;
    DBG_vPrintf(TRUE, "\nLED PROBE v10: observe original LED; no pin assumed\n");
    DBG_vPrintf(TRUE, "LED PROBE: do not press PCB button or RESET during sequence; S1 OPEN\n");
    DBG_vPrintf(TRUE, "LED PROBE: protected DIO5/6/7/14/15; eligible-level pulses only, 200ms x3\n");
    DBG_vPrintf(TRUE, "LED PROBE: note DIO and level if LED blinks; this does not assign a pin\n");
    DBG_vUartFlush();
    complete = led_probe_run(&io, NULL);
    DBG_vPrintf(TRUE, "LED PROBE DONE complete=%d; inputs restored; normal sensor/network starts\n", complete);
    DBG_vUartFlush();
}

PUBLIC void APP_vLedProbeLowRemaining(void)
{
    int complete;
    DBG_vPrintf(TRUE, "\nLED PROBE v11: LOW trials DIO9/12/13; LED pin NOT confirmed\n");
    DBG_vPrintf(TRUE, "LED PROBE: remove TP9 probe; S1 OPEN; do not press PCB button or RESET\n");
    DBG_vPrintf(TRUE, "LED PROBE: deliberate LOW trials; readback abort does not guarantee pin current\n");
    DBG_vPrintf(TRUE, "LED PROBE: watch 150ms x3 per pin; note DIO at visible light\n");
    DBG_vUartFlush();
    complete = led_probe_low_remaining_run(&io, NULL);
    DBG_vPrintf(TRUE, "LED PROBE v11 DONE complete=%d; pins restored; normal sensor/network starts\n", complete);
    DBG_vUartFlush();
}
