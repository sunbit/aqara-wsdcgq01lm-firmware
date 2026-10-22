"""Exercise optional LED diagnostics without hardware or MQTT."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_button_runtime import STUB

ROOT = Path(__file__).resolve().parents[1]


class DiagnosticTests(unittest.TestCase):
    def test_codes_led_timing_cancellation_and_reset_separation(self):
        self.compile_and_run('app_sensor_led.c', 'diagnostic_led_test.c')

    def test_only_manual_events_display_the_measurement_result(self):
        self.compile_and_run('app_sensor_events.c', 'diagnostic_event_test.c')

    def compile_and_run(self, source, test):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-diagnostic-') as tmp:
            folder = Path(tmp)
            (folder / 'sdk.h').write_text(STUB)
            for name in ('jendefs.h', 'AppHardwareApi.h', 'ZTimer.h', 'dbg.h',
                         'app_resources.h', 'app_events.h', 'MicroSpecific.h',
                         'pwrm.h', 'zps_apl_af.h', 'bdb_api.h', 'app_node.h',
                         'app_polling.h', 'app_battery.h'):
                (folder / name).write_text('#include "sdk.h"\n')
            command = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                       '-DSENSOR_LED_DIAGNOSTICS=1', '-I' + str(folder),
                       '-I' + str(ROOT / 'firmwares/common/include'),
                       str(ROOT / 'firmwares/common/src' / source),
                       str(ROOT / 'tests' / test),
                       '-o', str(folder / 'test')]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(folder / 'test')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
