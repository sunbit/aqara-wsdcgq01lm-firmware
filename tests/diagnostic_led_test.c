#include <assert.h>
#include "sdk.h"
#include "app_sensor_led.h"
#include "sensor_diagnostic.h"

uint8 u8TimerLedBlink = 3;
static unsigned duration, pulses, resets;
static int timer, on, fail;
void DBG_vPrintf(int enabled, const char *format, ...) {(void)enabled; (void)format;}
void vAHI_DioSetDirection(uint32 in, uint32 out) {assert(!in && out == SENSOR_LED_MASK);}
void vAHI_DioSetPullup(uint32 up, uint32 down) {assert(!up && down == SENSOR_LED_MASK);}
void vAHI_DioSetOutput(uint32 high, uint32 low) {
    assert((high | low) == SENSOR_LED_MASK && !(high & low));
    if (low && !on) ++pulses;
    on = !!low;
}
int ZTIMER_eStop(uint8 id) {assert(id == 3); timer = 0; return 0;}
int ZTIMER_eStart(uint8 id, uint32 ms) {
    assert(id == 3);
    if (fail) return 1;
    timer = 1; duration = ms; return 0;
}
void APP_vSendResetDeviceEvent(void) {++resets;}
static void tick(void) {assert(timer); timer = 0; APP_cbTimerLedBlink(NULL);}

int main(void)
{
    assert(sensor_diagnostic_transport(1, 1, "complete", "none") == SENSOR_DIAG_OK);
    assert(sensor_diagnostic_transport(1, 0, "complete", "none") == SENSOR_DIAG_CRC);
    assert(sensor_diagnostic_transport(0, 0, "write-address", "nack") == SENSOR_DIAG_WRITE_ADDRESS);
    assert(sensor_diagnostic_transport(0, 0, "read-address", "nack") == SENSOR_DIAG_READ_ADDRESS);
    assert(sensor_diagnostic_transport(0, 0, "command-wait", "tick-timeout") == SENSOR_DIAG_TIMEOUT);
    assert(sensor_diagnostic_transport(0, 0, "read-data", "transfer-timeout") == SENSOR_DIAG_TIMEOUT);
    assert(sensor_diagnostic_transport(0, 0, "command-msb", "nack") == SENSOR_DIAG_TRANSPORT);
    assert(sensor_diagnostic_transport(0, 0, NULL, NULL) == SENSOR_DIAG_TRANSPORT);
    for (unsigned code = 1; code <= 9; ++code) {
        APP_vSetupLeds(); pulses = 0;
        APP_vSensorLedPress(); assert(on && duration == 150);
        APP_vSensorLedDiagnostic(code); assert(on && duration == 150);
        tick(); assert(!on && duration == 1000); /* Original flash, then gap. */
        tick(); assert(on && duration == 300);
        while (timer) tick();
        assert(!on && pulses == code + 1 && resets == 0);
    }
    APP_vSetupLeds(); pulses = 0;
    APP_vSensorLedDiagnostic(4); assert(!on && duration == 1000);
    APP_vSensorLedPress(); assert(on && duration == 150);
    tick(); assert(!on && !timer && pulses == 1); /* New press cancels old code. */
    APP_vSensorLedDiagnostic(0); APP_vSensorLedDiagnostic(10); assert(!timer);
    APP_vSensorLedDiagnostic(5); tick(); assert(on);
    APP_vSensorLedPairing(); assert(on && duration == 150);
    APP_vSensorLedDiagnostic(9); /* Cannot replace reset confirmation. */
    while (timer) tick();
    assert(!on && resets == 1);
    APP_vSetupLeds(); fail = 1;
    APP_vSensorLedDiagnostic(2); assert(!on && !timer && resets == 1);
    fail = 0; APP_vSensorLedDiagnostic(2); tick();
    fail = 1; tick(); assert(!on && !timer && resets == 1);
    return 0;
}
