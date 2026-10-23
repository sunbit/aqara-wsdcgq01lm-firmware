"""Check that each diagnostic image varies just its intended bus parameter."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BusProfileTests(unittest.TestCase):
    def test_profile_parameters_and_invalid_selection(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-bus-profile-') as tmp:
            for profile, prescaler, settle in ((0, 31, 100), (1, 127, 100), (2, 31, 5000)):
                code = f'''#include "sensor_bus_profile.h"
                #if SENSOR_BUS_PRESCALER != {prescaler} || SENSOR_BUS_SETTLE_US != {settle}
                #error Profile changed an unintended parameter
                #endif
                int main(void) {{return 0;}}
                '''
                args = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'firmwares/common/include'),
                        '-DSENSOR_I2C_PROFILE=' + str(profile), '-x', 'c', '-',
                        '-o', str(Path(tmp) / 'test')]
                result = subprocess.run(args, input=code, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([compiler, '-DSENSOR_I2C_PROFILE=3',
                '-I' + str(ROOT / 'firmwares/common/include'), '-x', 'c', '-fsyntax-only', '-'],
                input='#include "sensor_bus_profile.h"\n', capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unknown sensor I2C comparison profile', result.stderr)


if __name__ == '__main__':
    unittest.main()
