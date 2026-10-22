#include <assert.h>
#include "sdk.h"
#include "app_sensor_events.h"
#include "sensor_diagnostic.h"

uint8 u8TimerZclTick = 2;
static int activities, joined = 1;
static unsigned samples, battery, displays, result, last_code;
void DBG_vPrintf(int enabled, const char *format, ...) {(void)enabled; (void)format;}
int PWRM_eStartActivity(void) {return ++activities;}
int PWRM_eFinishActivity(void) {assert(activities > 0); return --activities;}
bool_t APP_bSensorButtonDebouncing(void) {return FALSE;}
int bNodeJoined(void) {return joined;}
void APP_vResetNetworkRecords(void) {}
void vAHI_SwReset(void) {}
void APP_vSensorInvalidate(void) {}
int ZPS_eAplAfSendKeepAlive(void) {return 0;}
int ZTIMER_eStart(uint8 timer, uint32 ms) {assert(timer == 2 && ms == 1000); return 0;}
void APP_vStartPolling(int mode) {assert(mode == 0);}
void APP_vSensorSampleRequested(bool_t force, bool_t scheduled) {
    (void)force; (void)scheduled; ++samples;
}
void APP_vSensorBatteryReport(bool_t force) {(void)force; ++battery;}
unsigned APP_uSensorDiagnosticResult(void) {return result;}
void APP_vSensorLedDiagnostic(unsigned code) {
    ++displays; last_code = code;
    if (joined) assert(samples == battery); /* Capture after both operations. */
}
int main(void)
{
    result = SENSOR_DIAG_CRC;
    APP_vSendSensorTimerEvent(); APP_vProcessEvents();
    assert(samples == 1 && battery == 1 && !displays && !activities);
    APP_vSendConfigurationEvent(); APP_vProcessEvents();
    assert(samples == 2 && battery == 2 && !displays);
    APP_vSendFreshSensorEvent(); APP_vSendSensorTimerEvent(); APP_vProcessEvents();
    assert(samples == 3 && displays == 1 && last_code == SENSOR_DIAG_CRC);
    joined = 0;
    APP_vSendFreshSensorEvent(); APP_vProcessEvents();
    assert(samples == 3 && displays == 2 && last_code == SENSOR_DIAG_NOT_JOINED);
    APP_vSendSensorTimerEvent(); APP_vProcessEvents();
    assert(displays == 2 && !activities);
    return 0;
}
