#include <jendefs.h>
#include "dbg.h"
#include "pwrm.h"
#include "AppHardwareApi.h"
#include "ZTimer.h"

#include "isr_system.h"
#include "app_main.h"
#include "device_config.h"
#include "app_resources.h"
#include "app_sensor_button.h"

/***************************************************************************
 *
 * NAME: vISR_SystemController
 *
 * DESCRIPTION:
 * System controller interrupt service routine. Handles wake timer and DIO
 * interrupts:
 * - for DIO interrupts, disables further interrupts and starts button scan timer.
 * - for wake timer 1 interrupts, calls wake timer callback.
 *
 * PARAMETERS: None
 *
 * RETURNS: None
 *
 ****************************************************************************/

PUBLIC void vISR_SystemController(void)
{
    uint8 u8WakeInt = u8AHI_WakeTimerFiredStatus();
    uint32 u32DioStatus = u32AHI_DioInterruptStatus();
    if (u32DioStatus & BTN_CTRL_MASK)
    {
        APP_vSensorButtonInterrupt(u32DioStatus);
    }

    if (u8WakeInt & E_AHI_WAKE_TIMER_MASK_1)
    {
        DBG_vPrintf(TRACE_ISR_SYSTEM, "ISR_SYSTEM: Wake Timer 1 Interrupt\n");
        APP_vOnWakeTimer1();
    }
}
