/* Thermometer-only events: physical button never emits switch commands. */
#include <jendefs.h>
#include "MicroSpecific.h"
#include "pwrm.h"
#include "zps_apl_af.h"
#include "bdb_api.h"
#include "ZTimer.h"
#include "dbg.h"
#include "app_events.h"
#include "app_resources.h"
#include "app_node.h"
#include "app_polling.h"
#include "app_battery.h"
#include "app_sensor.h"
#include "app_sensor_button.h"
#include "app_sensor_events.h"
#include "app_battery_fresh.h"
#include "AppHardwareApi.h"
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
#include "app_diagnostic_suite.h"
#endif
#ifdef SENSOR_LED_DIAGNOSTICS
#include "app_sensor_led.h"
#include "sensor_diagnostic.h"
#endif

#define REQUEST_WAKE 1
#define REQUEST_PERIODIC 2
#define REQUEST_BUTTON 4
#define REQUEST_RESET 8
#define REQUEST_CONFIG 16
PRIVATE volatile uint8 pending;

PRIVATE void request(uint8 flags)
{
    uint32 saved;
    MICRO_DISABLE_AND_SAVE_INTERRUPTS(saved);
    if (!pending) PWRM_eStartActivity();
    pending |= flags;
    MICRO_RESTORE_INTERRUPTS(saved);
}

PUBLIC void APP_vSendWakeUpEvent(void) { request(REQUEST_WAKE); }
PUBLIC void APP_vSendSensorTimerEvent(void) { request(REQUEST_PERIODIC); }
PUBLIC void APP_vSendFreshSensorEvent(void) { request(REQUEST_BUTTON); }
PUBLIC void APP_vSendConfigurationEvent(void) { request(REQUEST_CONFIG); }

PUBLIC void APP_vSendResetDeviceEvent(void)
{
    request(REQUEST_RESET);
}

PUBLIC void APP_vSendButtonEvent(uint8 endpoint, teButtonAction action)
{
    (void)endpoint; (void)action;
    DBG_vPrintf(TRUE, "BUTTON legacy switch action disabled on thermometer\n");
}

PUBLIC void APP_vProcessEvents(void)
{
    uint8 flags;
    uint32 saved;
    /* Wait at most the debounce interval before merging a simultaneous
     * timer/button request into one fresh conversion. Pending flags hold a
     * power-manager activity, just like the SDK queue, and cannot be lost
     * between a timer interrupt and clearing a processed request. */
    MICRO_DISABLE_AND_SAVE_INTERRUPTS(saved);
    if (APP_bSensorButtonDebouncing()) {
        MICRO_RESTORE_INTERRUPTS(saved);
        return;
    }
    flags = pending;
    if (pending) {
        pending = 0;
        PWRM_eFinishActivity();
    }
    MICRO_RESTORE_INTERRUPTS(saved);
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
    /* This temporary image keeps normal sensor traffic out of the suite.
     * Matrix execution and PDM writes happen only in the main-loop task. */
    if (flags & REQUEST_BUTTON) APP_vDiagnosticRequest();
    if (flags & REQUEST_PERIODIC) {
        if (bNodeJoined()) {
            ZPS_eAplAfSendKeepAlive();
            APP_vStartPolling(POLL_REGULAR);
        }
        APP_vDiagnosticWake();
    }
    return;
#endif
    if (flags & REQUEST_RESET) {
        /* Network-only reset: preserve offsets and reporting preferences. */
        APP_vResetNetworkRecords();
        vAHI_SwReset();
        return;
    }
    if (!(flags & (REQUEST_PERIODIC | REQUEST_BUTTON | REQUEST_CONFIG))) return;
    if (flags & REQUEST_CONFIG) APP_vSensorInvalidate();
    if (!bNodeJoined()) {
#ifdef SENSOR_LED_DIAGNOSTICS
        if (flags & REQUEST_BUTTON) APP_vSensorLedDiagnostic(SENSOR_DIAG_NOT_JOINED);
#endif
        DBG_vPrintf(TRUE, "SENSOR request skipped: node not joined; no cached reports sent\n");
        return;
    }
    ZPS_eAplAfSendKeepAlive();
    ZTIMER_eStart(u8TimerZclTick, ZCL_TICK_TIME_MSEC);
    APP_vStartPolling(POLL_REGULAR);
    DBG_vPrintf(TRUE, "SENSOR request source=%s periodic=%d\n",
                (flags & REQUEST_BUTTON) ? "button" : "timer", !!(flags & REQUEST_PERIODIC));
    APP_vSensorSampleRequested(!!(flags & (REQUEST_BUTTON | REQUEST_CONFIG)), !!(flags & REQUEST_PERIODIC));
    APP_vSensorBatteryReport(!!(flags & (REQUEST_BUTTON | REQUEST_CONFIG)));
#ifdef SENSOR_LED_DIAGNOSTICS
    if (flags & REQUEST_BUTTON) APP_vSensorLedDiagnostic(APP_uSensorDiagnosticResult());
#endif
}
