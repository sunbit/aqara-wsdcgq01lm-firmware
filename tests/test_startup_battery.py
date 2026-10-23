"""Exercise the real cold-start battery path, including failed registration."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_button_runtime import STUB

ROOT = Path(__file__).resolve().parents[1]

class StartupBatteryTests(unittest.TestCase):
    def test_missing_cluster_null_storage_and_adc_timeout_are_safe(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-startup-battery-') as tmp:
            folder = Path(tmp)
            sdk = STUB.replace('typedef int bool_t;', 'typedef int bool_t; typedef int teZCL_Status;')
            sdk = sdk.replace('#define BATTERY_MIN_MV 2000\n', '').replace('#define BATTERY_DELTA_MV 1000\n', '')
            (folder/'sdk.h').write_text(sdk)
            for name in ('jendefs.h','AppHardwareApi.h','dbg.h','zcl.h',
                         'zcl_customcommand.h','PowerConfiguration.h'):
                (folder/name).write_text('#include "sdk.h"\n')
            (folder/'device_config.h').write_text('#include "sdk.h"\n'
                'typedef struct {uint8 u8BasicEndpoint;} tsDeviceConfig;\n'
                'extern const tsDeviceConfig sDeviceConfig;\n')
            binary = folder/'test'
            command = [compiler,'-std=c99','-Wall','-Wextra','-Werror',
                '-I'+str(folder),'-I'+str(ROOT/'firmwares/common/include'),
                str(ROOT/'firmwares/common/src/app_battery.c'),
                str(ROOT/'firmwares/common/src/app_battery_fresh.c'),
                str(ROOT/'tests/startup_battery_test.c'),'-o',str(binary)]
            result = subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            for mode in ('valid','lookup_error','null_instance','null_storage','adc_timeout','low_voltage'):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=3)
                    self.assertEqual(result.returncode,0,result.stderr)
