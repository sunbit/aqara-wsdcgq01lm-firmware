#include <assert.h>
#include <string.h>
#include "sdk.h"
#include "device_config.h"
#include "app_battery.h"

const tsDeviceConfig sDeviceConfig={1};
static tsCLD_PowerConfiguration power={99,99};
static tsZCL_ClusterInstance cluster={&power};
static const char *mode;
static unsigned starts,disables,lookups;
void DBG_vPrintf(int enabled,const char *format,...) {(void)enabled;(void)format;}
void vAHI_AdcEnable(int a,int b,int c) {assert(a==0&&b==1&&c==2);}
void vAHI_AdcStartSample(void) {++starts;}
int bAHI_AdcPoll(void) {return !strcmp(mode,"adc_timeout");}
void vAHI_AdcDisable(void) {++disables;}
uint16 u16AHI_AdcRead(void) {return !strcmp(mode,"low_voltage") ? 450 : 800;}
int eZCL_SearchForClusterEntry(uint8 ep,uint16 id,int server,tsZCL_ClusterInstance **out) {
    assert(ep==1&&id==1&&server);++lookups;
    *out=&cluster;
    if(!strcmp(mode,"lookup_error")) {*out=NULL;return 5;}
    if(!strcmp(mode,"null_instance")) *out=NULL;
    if(!strcmp(mode,"null_storage")) cluster.pvEndPointSharedStructPtr=NULL;
    return E_ZCL_SUCCESS;
}
bool_t APP_bSensorReportAttribute(uint16 id,uint16 attr) {
    (void)id;(void)attr;assert(!"Cold-start battery measurement must not transmit");return FALSE;
}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];APP_vMeasureRemainingBattery();
    assert(starts==1&&disables==1);
    if(!strcmp(mode,"adc_timeout")) assert(!lookups&&power.u8BatteryVoltage==99);
    else if(!strcmp(mode,"valid")) {
        assert(lookups==1&&power.u8BatteryVoltage==28&&power.u8BatteryPercentageRemaining==178);
    } else if(!strcmp(mode,"low_voltage")) {
        assert(lookups==1&&!power.u8BatteryVoltage&&!power.u8BatteryPercentageRemaining);
    } else assert(lookups==1&&power.u8BatteryVoltage==99&&power.u8BatteryPercentageRemaining==99);
    return 0;
}
