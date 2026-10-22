"""Host tests of the actual configuration/persistence and report policy code."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#ifndef TEST_CONFIG_SDK_H
#define TEST_CONFIG_SDK_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint8_t zenum8;
typedef uint8_t zbool;
typedef int bool_t;
typedef int teZCL_Status;
#define PUBLIC extern
#define PRIVATE static
#define TRUE 1
#define FALSE 0
#define E_ZCL_SUCCESS 0
#define E_ZCL_FAIL 1
#define E_ZCL_CMDS_SUCCESS 0
#define E_ZCL_CMDS_FAILURE 1
#define E_ZCL_ERR_ATTRIBUTE_RANGE 0x87
#define E_ZCL_AF_RD 1
#define E_ZCL_AF_WR 2
#define E_ZCL_AF_MS 4
#define E_ZCL_ENUM8 0x30
#define E_ZCL_BOOL 0x10
#define E_ZCL_INT16 0x29
#define E_ZCL_UINT16 0x21
#define E_ZCL_UINT32 0x23
#define E_ZCL_SECURITY_NETWORK 0
#define PDM_E_STATUS_OK 0
typedef struct {uint16 id; int flags,type; uint32 offset; int storage;} tsZCL_AttributeDefinition;
typedef struct {uint16 id; int server,security; unsigned count; tsZCL_AttributeDefinition *attributes; void *handler;} tsZCL_ClusterDefinition;
typedef struct {int unused;} tsZCL_ClusterInstance;
typedef struct {struct {struct {uint16 u16AttributeEnum; int eAttributeStatus; void *pvAttributeData;} sIndividualAttributeResponse;} uMessage;} tsZCL_CallBackEvent;
void vZCL_InitializeClusterInstance(tsZCL_ClusterInstance *,bool_t,tsZCL_ClusterDefinition *,void *,uint8 *,void *,void *);
int PDM_eReadDataFromRecord(uint16,void *,uint16,uint16 *);
int PDM_eSaveRecordData(uint16,void *,uint16);
void PDM_vDeleteDataRecord(uint16);
void DBG_vPrintf(int,const char *,...);
#endif
'''

class ConfigurationTests(unittest.TestCase):
    def compile_and_run(self, test, sources, stub=False):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-config-') as tmp:
            folder = Path(tmp)
            args = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror']
            if stub:
                (folder/'sdk.h').write_text(STUB)
                for name in ('jendefs.h','zcl.h','dbg.h','PDM.h'):
                    (folder/name).write_text('#include "sdk.h"\n')
                args += ['-DZCL_OPTIONS_H', '-I'+str(folder)]
            args += ['-I'+str(ROOT/'firmwares/common/include')]
            args += [str(ROOT/'firmwares/common/src'/name) for name in sources]
            args += [str(ROOT/'tests'/test), '-o', str(folder/'test')]
            result = subprocess.run(args, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(folder/'test')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_persisted_settings_and_range_callbacks(self):
        self.compile_and_run('configuration_test.c', ['ConfigurationCluster.c','sensor_config.c'], True)

    def test_calibration_and_reporting(self):
        self.compile_and_run('reporting_test.c', ['sensor_config.c','sensor_reporting.c'])

if __name__ == '__main__': unittest.main()
