"""Elapsed time around button wakes is not a whole measurement cycle."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class ClockTests(unittest.TestCase):
    def test_real_clock_module(self):
        with tempfile.TemporaryDirectory(prefix='aqara-clock-') as tmp:
            folder=Path(tmp)
            (folder/'jendefs.h').write_text('''#ifndef CLOCK_TEST_TYPES
#define CLOCK_TEST_TYPES
#include <stdint.h>
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint16_t uint16;
typedef int bool_t;
#define PUBLIC extern
#define PRIVATE static
#define FALSE 0
#endif
''')
            (folder/'AppHardwareApi.h').write_text('''#include <jendefs.h>
#define E_AHI_WAKE_TIMER_0 0
void vAHI_WakeTimerStop(int);
void vAHI_WakeTimerEnable(int,int);
void vAHI_WakeTimerStart(int,uint32);
uint32 u32AHI_WakeTimerRead(int);
''')
            (folder/'test.c').write_text('''#include <assert.h>
#include <stdint.h>
#include "app_sensor_clock.h"
static uint32 counter,elapsed,starts;
void vAHI_WakeTimerStop(int timer) {assert(timer==0);}
void vAHI_WakeTimerEnable(int timer,int interrupt) {assert(timer==0&&!interrupt);}
void vAHI_WakeTimerStart(int timer,uint32 count) {assert(timer==0&&count==UINT32_MAX);counter=count;++starts;}
uint32 u32AHI_WakeTimerRead(int timer) {assert(timer==0);return counter;}
void APP_vSensorElapsed(uint32 seconds) {elapsed+=seconds;}
int main(void) {
 APP_vSensorClockInit();assert(starts==1);
 counter-=16000;APP_vSensorClockSync();assert(elapsed==0);
 counter-=16000;APP_vSensorClockSync();assert(elapsed==1);
 counter-=58*32000;APP_vSensorClockSync();assert(elapsed==59);
 elapsed=0; /* Manual report at t=59 resets report age. */
 counter-=32000;APP_vSensorClockSync();assert(elapsed==1); /* t=60 is NOT 60 s since that report. */
 counter=7200*32000-1;APP_vSensorClockSync();assert(starts==2);
 elapsed=0;counter-=32000;APP_vSensorClockSync();assert(elapsed==1);
 return 0;
}
''')
            cmd=[shutil.which('clang') or shutil.which('cc'),'-std=c99','-Wall','-Wextra','-Werror',
                 '-I'+str(folder),'-I'+str(ROOT/'firmwares/common/include'),
                 str(ROOT/'firmwares/common/src/app_sensor_clock.c'),str(folder/'test.c'),'-o',str(folder/'test')]
            result=subprocess.run(cmd,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(folder/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__': unittest.main()
