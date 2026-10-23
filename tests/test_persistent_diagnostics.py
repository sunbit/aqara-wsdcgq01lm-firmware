"""Compile the real suite and persistence/replay tasks against a simulated SDK."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#ifndef DIAG_TEST_SDK
#define DIAG_TEST_SDK
#include <stdint.h>
#include <stddef.h>
typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32;
typedef int bool_t;
#define PUBLIC extern
#define PRIVATE static
#define TRUE 1
#define FALSE 0
#define E_ZCL_AF_RD 1
#define E_ZCL_AF_RP 2
#define E_ZCL_AF_MS 4
#define E_ZCL_OSTRING 0x41
#define E_ZCL_SECURITY_NETWORK 0
#define PDM_E_STATUS_OK 0
#define E_ZTIMER_OK 0
#define ZTIMER_TIME_MSEC(n) (n)
#define ZCL_TICK_TIME_MSEC 1000
#define POLL_REGULAR 0
typedef struct {uint8 u8MaxLength,u8Length; uint8 *pu8Data;} tsZCL_OctetString;
typedef struct {uint16 id; int flags,type; uint32 offset; int storage;} tsZCL_AttributeDefinition;
typedef struct {uint16 id; int manufacturer_specific,security; unsigned count; tsZCL_AttributeDefinition *attrs; void *handler;} tsZCL_ClusterDefinition;
typedef struct {int unused;} tsZCL_ClusterInstance;
extern uint8 u8TimerDiagnostic,u8TimerZclTick;
void vZCL_InitializeClusterInstance(tsZCL_ClusterInstance *,int,tsZCL_ClusterDefinition *,void *,uint8 *,void *,void *);
int eZCL_SetReportableFlag(uint8,uint16,int,int,uint16);
int PDM_eReadDataFromRecord(uint16,void *,uint16,uint16 *);
int PDM_eSaveRecordData(uint16,void *,uint16);
int PWRM_eStartActivity(void); int PWRM_eFinishActivity(void);
int ZTIMER_eStop(uint8); int ZTIMER_eStart(uint8,uint32);
void vAHI_DioSetOutput(uint32,uint32);
int bNodeJoined(void); int ZPS_eAplAfSendKeepAlive(void);
void APP_vStartPolling(int);
void DBG_vPrintf(int,const char *,...);
#endif
'''

class PersistentDiagnosticTests(unittest.TestCase):
    def test_actual_record_storage_replay_and_failure_handling(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='aqara-suite-') as tmp:
            folder = Path(tmp)
            (folder / 'sdk.h').write_text(STUB)
            for name in ('jendefs.h','zcl.h','app_resources.h','app_node.h',
                         'app_polling.h','AppHardwareApi.h','ZTimer.h','PDM.h',
                         'pwrm.h','zps_apl_af.h','dbg.h','bdb_api.h'):
                (folder / name).write_text('#include "sdk.h"\n')
            args = [compiler,'-std=c99','-Wall','-Wextra','-Werror',
                    '-I'+str(folder),'-I'+str(ROOT/'firmwares/common/include'),
                    str(ROOT/'firmwares/common/src/diagnostic_suite.c'),
                    str(ROOT/'firmwares/common/src/app_diagnostic_suite.c'),
                    str(ROOT/'tests/persistent_diagnostic_test.c'),'-o',str(folder/'test')]
            result = subprocess.run(args,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result = subprocess.run([str(folder/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_diagnostic_descriptor_is_generated_without_mutating_normal_xml(self):
        source = ROOT/'firmwares/WSDCGQ01LM_DEV/app.zpscfg'
        before = source.read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp)/'diagnostic.zpscfg'
            subprocess.run(['python3',str(ROOT/'scripts/diagnostic-zpscfg.py'),str(source),str(dest)],check=True)
            root = ET.parse(dest).getroot()
            self.assertTrue(any(n.get('Id')=='0xfc01' for n in root.iter('Clusters')))
            self.assertTrue(any(n.get('Cluster')=='Diagnostics' for n in root.iter('InputClusters')))
        self.assertEqual(before,source.read_bytes())

if __name__ == '__main__': unittest.main()
