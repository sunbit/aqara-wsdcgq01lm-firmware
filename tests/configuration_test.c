#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sdk.h"
#include "ConfigurationCluster.h"
static uint8 store[256];
static uint16 stored_bytes;
static unsigned saves, events, deletions;
static int fail_save, bad_length;
static tsCLD_Configuration config;
static tsZCL_ClusterInstance instance;
void DBG_vPrintf(int log,const char *format,...) {(void)log;(void)format;}
void vZCL_InitializeClusterInstance(tsZCL_ClusterInstance *i,bool_t server,tsZCL_ClusterDefinition *d,void *s,uint8 *c,void *a,void *b) {
    (void)i;(void)d;(void)s;(void)c;(void)a;(void)b;assert(server);
}
int PDM_eReadDataFromRecord(uint16 id,void *result,uint16 size,uint16 *bytes) {
    if(id==PDM_ID_CONFIGURATION_CLUSTER) {assert(size==2);((uint8*)result)[0]=2;((uint8*)result)[1]=0;*bytes=2;return 0;}
    assert(id==PDM_ID_SENSOR_CONFIGURATION);
    if(!stored_bytes)return 1;
    assert(size>=stored_bytes);memcpy(result,store,stored_bytes);
    *bytes=bad_length ? stored_bytes-1 : stored_bytes;return 0;
}
int PDM_eSaveRecordData(uint16 id,void *value,uint16 bytes) {
    assert(id==PDM_ID_SENSOR_CONFIGURATION&&bytes<=sizeof(store));
    ++saves;if(fail_save)return 1;memcpy(store,value,bytes);stored_bytes=bytes;return 0;
}
void PDM_vDeleteDataRecord(uint16 id) {
    assert(id==PDM_ID_SENSOR_CONFIGURATION||id==PDM_ID_CONFIGURATION_CLUSTER);
    if(id==PDM_ID_SENSOR_CONFIGURATION)stored_bytes=0;
    ++deletions;
}
void APP_vSendConfigurationEvent(void) {++events;}
static void boot(void) {
    memset(&config,0xa5,sizeof(config));
    assert(eCLD_ConfigurationCreateConfiguration(&instance,1,&sCLD_Configuration,&config,au8ConfigurationAttributeControlBits)==0);
}
static int write_setting(uint16 id,int32_t value) {
    uint8 unaligned[5]={0,(uint8)value,(uint8)(value>>8),(uint8)(value>>16),(uint8)(value>>24)};
    tsZCL_CallBackEvent event={0};
    event.uMessage.sIndividualAttributeResponse.u16AttributeEnum=id;
    event.uMessage.sIndividualAttributeResponse.pvAttributeData=unaligned+1;
    vHandleConfigurationClusterAttrsRange(&event);
    if(event.uMessage.sIndividualAttributeResponse.eAttributeStatus)return 0;
    assert(sensor_config_set(&config.sensor,id,value)); /* SDK copies after successful range check. */
    vHandleConfigurationClusterAttrsWrite(&event,&config);
    return !event.uMessage.sIndividualAttributeResponse.eAttributeStatus;
}
int main(void) {
    boot();assert(config.eButtonMode==2&&config.sensor.sample_seconds==60&&config.sensor.temperature_delta==10);
    assert(APP_psSensorConfig()==&config.sensor&&!saves&&!events);
    assert(write_setting(SENSOR_ATTR_TEMPERATURE_OFFSET,-125));
    assert(write_setting(SENSOR_ATTR_HUMIDITY_OFFSET,250));
    assert(write_setting(SENSOR_ATTR_SAMPLE_SECONDS,120));
    assert(saves==3&&events==3);
    assert(write_setting(SENSOR_ATTR_SAMPLE_SECONDS,120)&&saves==3); /* No EEPROM write for unchanged values. */
    boot();assert(config.sensor.temperature_offset==-125&&config.sensor.humidity_offset==250&&config.sensor.sample_seconds==120);
    assert(write_setting(SENSOR_ATTR_TEMPERATURE_MAX_SECONDS,86400));
    assert(!write_setting(SENSOR_ATTR_TEMPERATURE_MAX_SECONDS,86401));
    assert(!write_setting(SENSOR_ATTR_TEMPERATURE_MAX_SECONDS,-1));
    assert(!write_setting(SENSOR_ATTR_SAMPLE_SECONDS,0));
    assert(!write_setting(SENSOR_ATTR_TEMPERATURE_DELTA,0));
    assert(!write_setting(SENSOR_ATTR_TEMPERATURE_OFFSET,-1001));
    assert(!write_setting(SENSOR_ATTR_HUMIDITY_OFFSET,2001));
    unsigned before=events;
    fail_save=1;assert(!write_setting(SENSOR_ATTR_SAMPLE_SECONDS,30));
    assert(config.sensor.sample_seconds==120&&events==before);fail_save=0;
    boot();assert(config.sensor.sample_seconds==120&&config.sensor.temperature_max_seconds==86400);
    /* Truncated/unknown stored layouts fall back safely without deleting network records. */
    bad_length=1;boot();assert(config.sensor.sample_seconds==60&&!deletions);bad_length=0;
    store[0]^=1;boot();assert(config.sensor.temperature_offset==0&&!deletions);store[0]^=1;
    boot();assert(config.sensor.temperature_offset==-125);
    vResetConfigurationCluster();assert(deletions==2);boot();assert(config.sensor.temperature_offset==0);
    puts("PASS: wire ranges, signed offsets, persistence, migration, rollback and record validation");
}
