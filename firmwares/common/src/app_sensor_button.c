#include <jendefs.h>
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "dbg.h"
#include "app_resources.h"
#include "app_sensor_button.h"
#include "app_sensor_events.h"
#include "button_debounce.h"
#ifdef SENSOR_LED_FEEDBACK
#include "app_sensor_led.h"
#endif

PRIVATE button_debounce button;
PRIVATE volatile bool_t scanning;
PRIVATE bool_t configured;

PRIVATE bool_t pressed_now(void)
{
    return !(u32AHI_DioReadInput() & SENSOR_BUTTON_MASK);
}

PRIVATE void start_scan(void)
{
    vAHI_DioInterruptEnable(0, SENSOR_BUTTON_MASK);
    if (!scanning) {
        scanning = TRUE;
        ZTIMER_eStop(u8TimerButtonScan);
        ZTIMER_eStart(u8TimerButtonScan, BUTTON_SCAN_TIME_MSEC);
    }
}

PRIVATE void arm_next_edge(void)
{
    /* Sleep on either stable level. A held press waits for release, with no
     * repeating scan timer or repeated report. Clear before arming; compare
     * the live input afterwards so an edge during rearming is not lost. */
    (void)u32AHI_DioInterruptStatus();
    if (button.pressed)
        vAHI_DioInterruptEdge(SENSOR_BUTTON_MASK, 0);
    else
        vAHI_DioInterruptEdge(0, SENSOR_BUTTON_MASK);
    vAHI_DioInterruptEnable(SENSOR_BUTTON_MASK, 0);
    if (pressed_now() != button.pressed) start_scan();
}

PUBLIC void APP_vConfigureButtons(void)
{
    vAHI_DioInterruptEnable(0, SENSOR_BUTTON_MASK);
    vAHI_DioSetDirection(SENSOR_BUTTON_MASK, 0);
    vAHI_DioSetPullup(SENSOR_BUTTON_MASK, 0);
    button.pressed = button.count = 0;
    scanning = FALSE;
    configured = TRUE;
#ifdef SENSOR_LED_FEEDBACK
    DBG_vPrintf(TRUE, "BUTTON: DIO5 active-low; LED DIO9; fresh temperature/humidity/battery on press\n");
#else
    DBG_vPrintf(TRUE, "BUTTON: DIO5 active-low; fresh temperature/humidity/battery on press\n");
#endif
    arm_next_edge();
}

PUBLIC void APP_vSensorButtonInterrupt(uint32 status)
{
    /* No UART, ADC, I2C or Zigbee operation in an interrupt handler. */
    if (configured && (status & SENSOR_BUTTON_MASK)) start_scan();
}

PUBLIC void APP_vSensorButtonWake(uint32 status)
{
    if (!configured) return;
    /* UART0 RTS/CTS is released before UART enable on every wake. Reapply
     * input/pull-up only to the physically confirmed button. */
    vAHI_DioSetDirection(SENSOR_BUTTON_MASK, 0);
    vAHI_DioSetPullup(SENSOR_BUTTON_MASK, 0);
    if ((status & SENSOR_BUTTON_MASK) || scanning || pressed_now() != button.pressed) {
        DBG_vPrintf(TRUE, "BUTTON wake DIO5 level=%d\n", !pressed_now());
        start_scan();
    } else {
        arm_next_edge();
    }
}

PUBLIC bool_t APP_bSensorButtonDebouncing(void)
{
    return scanning;
}

PUBLIC void APP_cbTimerButtonScan(void *context)
{
    int edge;
    (void)context;
    edge = button_debounce_update(&button, pressed_now());
    if (edge > 0) {
#ifdef SENSOR_LED_FEEDBACK
        APP_vSensorLedPress();
#endif
        DBG_vPrintf(TRUE, "BUTTON pressed DIO5: request fresh temperature/humidity/battery\n");
        APP_vSendFreshSensorEvent();
    } else if (edge < 0) {
        DBG_vPrintf(TRUE, "BUTTON released DIO5\n");
    }
    if (button.count) {
        ZTIMER_eStart(u8TimerButtonScan, BUTTON_SCAN_TIME_MSEC);
    } else {
        ZTIMER_eStop(u8TimerButtonScan);
        scanning = FALSE;
        arm_next_edge();
    }
}
