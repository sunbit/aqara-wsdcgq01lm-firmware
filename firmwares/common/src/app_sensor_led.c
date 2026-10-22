#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "dbg.h"
#include "app_resources.h"
#include "app_sensor_led.h"
#include "app_events.h"

PRIVATE unsigned pairing_pulses;
PRIVATE bool_t led_on;
#ifdef SENSOR_LED_DIAGNOSTICS
PRIVATE unsigned diagnostic_pulses;
/* 1: preserve the ordinary press flash, 2: one-second gap, 3: result pulses. */
PRIVATE unsigned diagnostic_phase;
#endif

PRIVATE bool_t start_phase(unsigned milliseconds)
{
    if (ZTIMER_eStart(u8TimerLedBlink, ZTIMER_TIME_MSEC(milliseconds)) == E_ZTIMER_OK)
        return TRUE;
    pairing_pulses = 0;
#ifdef SENSOR_LED_DIAGNOSTICS
    diagnostic_pulses = diagnostic_phase = 0;
#endif
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
#ifdef SENSOR_LED_DIAGNOSTICS
    diagnostic_pulses = diagnostic_phase = 0;
#endif
    led_on = FALSE;
    DBG_vPrintf(TRUE, "LED: DIO9 active-low; 150ms on debounced PCB press\n");
}

PUBLIC void APP_vSensorLedPress(void)
{
    /* Called from debounce task, never ISR; works without a Zigbee network.
     * Existing LED timer is PREVENT_SLEEP and app_main counts it as active.
     * Another accepted press restarts the interval from that new press. */
    if (pairing_pulses) return;
#ifdef SENSOR_LED_DIAGNOSTICS
    diagnostic_pulses = diagnostic_phase = 0;
#endif
    ZTIMER_eStop(u8TimerLedBlink);
    led_on = TRUE;
    vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    if (start_phase(SENSOR_LED_BLINK_MS)) {
        DBG_vPrintf(TRUE, "LED: button blink ON DIO9\n");
    }
}

PUBLIC void APP_vSensorLedPairing(void)
{
    if (pairing_pulses) return;
#ifdef SENSOR_LED_DIAGNOSTICS
    diagnostic_pulses = diagnostic_phase = 0;
#endif
    ZTIMER_eStop(u8TimerLedBlink);
    pairing_pulses = 3;
    led_on = TRUE;
    vAHI_DioSetOutput(0, SENSOR_LED_MASK);
    (void)start_phase(SENSOR_LED_BLINK_MS);
}

#ifdef SENSOR_LED_DIAGNOSTICS
PUBLIC void APP_vSensorLedDiagnostic(unsigned code)
{
    if (pairing_pulses || code < 1 || code > 9) return;
    diagnostic_pulses = code;
    if (led_on) {
        diagnostic_phase = 1; /* Let the original 150 ms flash finish. */
    } else {
        diagnostic_phase = 2;
        ZTIMER_eStop(u8TimerLedBlink);
        (void)start_phase(1000);
    }
}
#endif

PUBLIC void APP_cbTimerLedBlink(void *context)
{
    (void)context;
    ZTIMER_eStop(u8TimerLedBlink);
#ifdef SENSOR_LED_DIAGNOSTICS
    if (!pairing_pulses && diagnostic_phase) {
        if (diagnostic_phase == 1) {
            led_on = FALSE;
            vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
            diagnostic_phase = 2;
            (void)start_phase(1000);
            return;
        }
        if (diagnostic_phase == 2) {
            diagnostic_phase = 3;
            led_on = TRUE;
        } else {
            led_on = !led_on;
            if (!led_on && --diagnostic_pulses == 0) {
                diagnostic_phase = 0;
                vAHI_DioSetOutput(SENSOR_LED_MASK, 0);
                return;
            }
        }
        vAHI_DioSetOutput(led_on ? 0 : SENSOR_LED_MASK, led_on ? SENSOR_LED_MASK : 0);
        (void)start_phase(300);
        return;
    }
#endif
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
    (void)start_phase(SENSOR_LED_BLINK_MS);
}
