#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "dbg.h"
#include "app_resources.h"
#include "app_sensor_led.h"

PUBLIC void APP_vSetupLeds(void)
{
    /* Preload OFF before enabling the output: no spurious boot flash.
     * Output and level are retained in OSCON_RAMON sleep. */
    vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
    vAHI_DioSetDirection(0, SENSOR_LED_MASK);
    vAHI_DioSetPullup(0, SENSOR_LED_MASK);
    ZTIMER_eStop(u8TimerLedBlink);
    DBG_vPrintf(TRUE, "LED v12: DIO9 active-low; 150ms on debounced PCB press\n");
}

PUBLIC void APP_vSensorLedPress(void)
{
    /* Called from debounce task, never ISR; works without a Zigbee network.
     * Existing LED timer is PREVENT_SLEEP and app_main counts it as active.
     * Another accepted press restarts the interval from that new press. */
    ZTIMER_eStop(u8TimerLedBlink);
    vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    if (ZTIMER_eStart(u8TimerLedBlink, ZTIMER_TIME_MSEC(SENSOR_LED_BLINK_MS)) != E_ZTIMER_OK) {
        vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
        DBG_vPrintf(TRUE, "LED v12: timer failed; DIO9 restored OFF\n");
    } else {
        DBG_vPrintf(TRUE, "LED v12: button blink ON DIO9\n");
    }
}

PUBLIC void APP_cbTimerLedBlink(void *context)
{
    (void)context;
    ZTIMER_eStop(u8TimerLedBlink);
    vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
    DBG_vPrintf(TRUE, "LED v12: button blink OFF DIO9\n");
}
