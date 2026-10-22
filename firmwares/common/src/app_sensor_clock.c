#include <jendefs.h>
#include <stdint.h>
#include "AppHardwareApi.h"
#include "app_sensor.h"
#include "app_sensor_clock.h"

/* PWRM uses wake timer 1. Timer 0 provides elapsed time while awake AND in
 * OSCON_RAMON sleep, with no interrupt and no additional scheduled wakeups.
 * Use the same nominal 32 ticks/ms timebase as PWRM's scheduled activities. */
#define TICKS_PER_SECOND 32000UL
PRIVATE uint32 previous, fraction;

PUBLIC void APP_vSensorClockInit(void)
{
    vAHI_WakeTimerStop(E_AHI_WAKE_TIMER_0);
    vAHI_WakeTimerEnable(E_AHI_WAKE_TIMER_0, FALSE);
    previous = UINT32_MAX;
    fraction = 0;
    vAHI_WakeTimerStart(E_AHI_WAKE_TIMER_0, previous);
}

PUBLIC void APP_vSensorClockSync(void)
{
    uint32 current = u32AHI_WakeTimerRead(E_AHI_WAKE_TIMER_0);
    uint64 ticks = (uint64)(previous - current) + fraction;
    uint32 seconds = (uint32)(ticks / TICKS_PER_SECOND);
    fraction = (uint32)(ticks % TICKS_PER_SECOND);
    previous = current;
    /* Reload well before the 37-hour countdown expires. Normal sampling
     * synchronizes at least hourly; fractional ticks are retained. */
    if (current < TICKS_PER_SECOND * 7200UL) {
        vAHI_WakeTimerStop(E_AHI_WAKE_TIMER_0);
        previous = UINT32_MAX;
        vAHI_WakeTimerStart(E_AHI_WAKE_TIMER_0, previous);
    }
    APP_vSensorElapsed(seconds);
}
