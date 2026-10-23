"""Exercise the GPIO transport against an edge-driven I2C slave."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GpioI2cTests(unittest.TestCase):
    def test_wire_bytes_ack_stop_faults_and_release(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-gpio-i2c-') as tmp:
            binary = str(Path(tmp) / 'test')
            args = [compiler, '-std=gnu89', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'firmwares/common/include'),
                    str(ROOT / 'firmwares/common/src/sensor_wire.c'),
                    str(ROOT / 'firmwares/common/src/sensor_gpio_i2c.c'),
                    str(ROOT / 'tests/gpio_i2c_test.c'), '-o', binary]
            result = subprocess.run(args, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([binary], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
