#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "dbg.h"
#include "app_resources.h"
#include "app_sensor_led.h"
#include "app_events.h"

PRIVATE unsigned pairing_pulses;
PRIVATE bool_t led_on;

PRIVATE bool_t start_phase(void)
{
    if (ZTIMER_eStart(u8TimerLedBlink, ZTIMER_TIME_MSEC(SENSOR_LED_BLINK_MS)) == E_ZTIMER_OK)
        return TRUE;
    pairing_pulses = 0;
    led_on = FALSE;
    vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
    DBG_vPrintf(TRUE, "LED: timer failed; no re-pair requested\n");
    return FALSE;
}

PUBLIC void APP_vSetupLeds(void)
{
    /* Preload OFF before enabling the output: no spurious boot flash.
     * Output and level are retained in OSCON_RAMON sleep. */
    vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
    vAHI_DioSetDirection(0, SENSOR_LED_MASK);
    vAHI_DioSetPullup(0, SENSOR_LED_MASK);
    ZTIMER_eStop(u8TimerLedBlink);
    pairing_pulses = 0;
    led_on = FALSE;
    DBG_vPrintf(TRUE, "LED: DIO9 active-low; 150ms on debounced PCB press\n");
}

PUBLIC void APP_vSensorLedPress(void)
{
    /* Called from debounce task, never ISR; works without a Zigbee network.
     * Existing LED timer is PREVENT_SLEEP and app_main counts it as active.
     * Another accepted press restarts the interval from that new press. */
    if (pairing_pulses) return;
    ZTIMER_eStop(u8TimerLedBlink);
    led_on = TRUE;
    vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    if (start_phase()) {
        DBG_vPrintf(TRUE, "LED: button blink ON DIO9\n");
    }
}

PUBLIC void APP_vSensorLedPairing(void)
{
    if (pairing_pulses) return;
    ZTIMER_eStop(u8TimerLedBlink);
    pairing_pulses = 3;
    led_on = TRUE;
    vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    (void)start_phase();
}

PUBLIC void APP_cbTimerLedBlink(void *context)
{
    (void)context;
    ZTIMER_eStop(u8TimerLedBlink);
    if (!pairing_pulses) {
        led_on = FALSE;
        vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
        return;
    }
    led_on = !led_on;
    if (led_on) vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    else {
        vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
        if (--pairing_pulses == 0) {
            /* Only erase the network after all three visible pulses end. */
            APP_vSendResetDeviceEvent();
            return;
        }
    }
    (void)start_phase();
}
