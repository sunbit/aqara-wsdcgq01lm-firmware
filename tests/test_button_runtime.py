"""Compile the real button/event/battery modules against a simulated SDK.

No UART, MQTT or physical device is used. NXP cross-compilation is separate.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

KIT = Path(__file__).resolve().parents[1]
STUB = r'''
#ifndef AQARA_TEST_SDK_H
#define AQARA_TEST_SDK_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int bool_t;
typedef int teButtonAction;
#define PUBLIC extern
#define PRIVATE static
#define TRUE 1
#define FALSE 0
#define BUTTON_SCAN_TIME_MSEC 10
#define ZCL_TICK_TIME_MSEC 1000
#define POLL_REGULAR 0
#define BATTERY_MIN_MV 2000
#define BATTERY_DELTA_MV 1000
#define E_AHI_ADC_SINGLE_SHOT 0
#define E_AHI_AP_INPUT_RANGE_2 1
#define E_AHI_ADC_SRC_VOLT 2
#define GENERAL_CLUSTER_ID_POWER_CONFIGURATION 1
#define E_ZCL_SUCCESS 0
#define E_ZTIMER_OK 0
#define ZTIMER_TIME_MSEC(v) (v)
#define MICRO_DISABLE_AND_SAVE_INTERRUPTS(saved) do { (saved)=0; } while(0)
#define MICRO_RESTORE_INTERRUPTS(saved) do { (void)(saved); } while(0)
typedef struct {void *pvEndPointSharedStructPtr;} tsZCL_ClusterInstance;
typedef struct {uint8 u8BatteryVoltage, u8BatteryPercentageRemaining;} tsCLD_PowerConfiguration;
extern uint8 u8TimerButtonScan, u8TimerZclTick, u8TimerLedBlink;
void DBG_vPrintf(int enabled, const char *format, ...);
uint32 u32AHI_DioReadInput(void);
uint32 u32AHI_DioInterruptStatus(void);
void vAHI_DioInterruptEnable(uint32 enable, uint32 disable);
void vAHI_DioInterruptEdge(uint32 rising, uint32 falling);
void vAHI_DioSetDirection(uint32 inputs, uint32 outputs);
void vAHI_DioSetPullup(uint32 enable, uint32 disable);
void vAHI_DioSetOutput(uint32 on, uint32 off);
int ZTIMER_eStop(uint8 timer);
int ZTIMER_eStart(uint8 timer, uint32 period);
int PWRM_eStartActivity(void);
int PWRM_eFinishActivity(void);
int ZPS_eAplAfSendKeepAlive(void);
int bNodeJoined(void);
void APP_vResetNetworkRecords(void);
void vAHI_SwReset(void);
void APP_vSensorInvalidate(void);
void APP_vSensorBatteryReport(bool_t force);
void APP_vStartPolling(int mode);
void APP_vMeasureRemainingBattery(void);
void APP_vSendWakeUpEvent(void);
void APP_vSendResetDeviceEvent(void);
void APP_vSendButtonEvent(uint8 endpoint, teButtonAction action);
void APP_vProcessEvents(void);
void vAHI_AdcEnable(int mode, int range, int source);
void vAHI_AdcStartSample(void);
int bAHI_AdcPoll(void);
void vAHI_AdcDisable(void);
uint16 u16AHI_AdcRead(void);
int eZCL_SearchForClusterEntry(uint8 ep, uint16 cluster, int server, tsZCL_ClusterInstance **result);
#endif
'''


class ButtonRuntimeTests(unittest.TestCase):
    def test_real_button_event_and_fresh_battery_modules(self):
        self.compile_and_run(False)

    def test_real_button_led_and_fresh_reports_integration(self):
        self.compile_and_run(True)

    def test_persistent_profile_button_rearms_without_network_reset(self):
        self.compile_and_run(True, True)

    def compile_and_run(self, with_led, persistent=False):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler, 'A host C compiler is required')
        with tempfile.TemporaryDirectory(prefix='aqara-button-runtime-') as temporary:
            folder = Path(temporary)
            (folder / 'sdk.h').write_text(STUB)
            for name in ('jendefs.h', 'AppHardwareApi.h', 'ZTimer.h', 'dbg.h',
                         'app_resources.h', 'MicroSpecific.h', 'pwrm.h', 'zps_apl_af.h',
                         'bdb_api.h', 'app_events.h', 'app_node.h', 'app_polling.h',
                         'app_battery.h', 'PowerConfiguration.h', 'zcl.h', 'zcl_customcommand.h'):
                (folder / name).write_text('#include "sdk.h"\n')
            command = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                       '-I' + str(folder), '-I' + str(KIT / 'firmwares/common/include')]
            command += [str(KIT / 'firmwares/common/src' / name) for name in (
                'button_debounce.c', 'app_sensor_button.c', 'app_sensor_events.c',
                'app_battery_fresh.c')]
            if with_led:
                command += ['-DSENSOR_LED_FEEDBACK=1', str(KIT / 'firmwares/common/src/app_sensor_led.c')]
            if persistent:
                command += ['-DSENSOR_PERSISTENT_DIAGNOSTICS=1']
                (folder/'app_diagnostic_suite.h').write_text('#include "sdk.h"\n'
                    'void APP_vDiagnosticRequest(void);\nvoid APP_vDiagnosticWake(void);\n'
                    'int APP_bDiagnosticBusy(void);\n')
            executable = folder / 'test'
            command += [str(KIT / 'tests/button_runtime_test.c'), '-o', str(executable)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('PASS:', result.stdout)


if __name__ == '__main__':
    unittest.main()
