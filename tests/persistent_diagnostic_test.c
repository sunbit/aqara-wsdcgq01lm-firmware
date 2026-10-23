#include <assert.h>
#include <string.h>
#include "sdk.h"
#include "app_diagnostic_suite.h"
#include "app_sensor_led.h"

uint8 u8TimerDiagnostic=5;
uint8 u8TimerZclTick=2;
static diag_record nvm;
static int have_nvm,save_fail,read_corrupt,timer_fail,activities,timer,joined=1;
static unsigned saves,captures,sends;
static uint8 sent[200][DIAG_PACKET_BYTES];
static diagnostic_cluster_data data;
void vZCL_InitializeClusterInstance(tsZCL_ClusterInstance *i,int server,tsZCL_ClusterDefinition *d,
        void *shared,uint8 *control,void *a,void *b) {
    (void)i;(void)control;(void)a;(void)b;
    assert(server && d->id==DIAG_CLUSTER_ID && d->count==1 && shared==&data);
}
int eZCL_SetReportableFlag(uint8 ep,uint16 cluster,int server,int ms,uint16 attr) {
    assert(ep==1 && cluster==DIAG_CLUSTER_ID && server && !ms && !attr);return 0;
}
int PDM_eReadDataFromRecord(uint16 id,void *out,uint16 size,uint16 *bytes) {
    assert(id==DIAG_PDM_ID && size==sizeof(nvm));
    if(!have_nvm) {*bytes=0;return 1;}
    memcpy(out,&nvm,size);*bytes=size;
    if(read_corrupt) ((uint8 *)out)[20]^=1;
    return 0;
}
int PDM_eSaveRecordData(uint16 id,void *in,uint16 size) {
    assert(id==DIAG_PDM_ID && size==sizeof(nvm));++saves;
    if(save_fail)return 1;
    memcpy(&nvm,in,size);have_nvm=1;return 0;
}
int PWRM_eStartActivity(void) {assert(!activities);return ++activities;}
int PWRM_eFinishActivity(void) {assert(activities==1);return --activities;}
int ZTIMER_eStop(uint8 id) {assert(id==5);timer=0;return 0;}
int ZTIMER_eStart(uint8 id,uint32 ms) {if(id==2){assert(ms==1000);return 0;}assert(id==5&&(ms==250||ms==500));if(timer_fail)return 1;timer=1;return 0;}
void vAHI_DioSetOutput(uint32 high,uint32 low) {assert(high==SENSOR_LED_MASK&&!low);}
int bNodeJoined(void) {return joined;}
int ZPS_eAplAfSendKeepAlive(void) {return 0;}
void APP_vStartPolling(int mode) {assert(mode==0);}
void DBG_vPrintf(int enabled,const char *format,...) {(void)enabled;(void)format;}
void APP_vSetupLeds(void) {}
void APP_vSensorDiagnosticCapture(const diag_case *spec,uint8 row[DIAG_ROW_BYTES]) {
    assert(spec);++captures;memset(row,0,DIAG_ROW_BYTES);row[1]=1;row[2]=1;row[3]=64;row[4]=0xf0;row[5]=0x0a;
}
bool_t APP_bSensorReportAttribute(uint16 cluster,uint16 attr) {
    assert(cluster==DIAG_CLUSTER_ID&&!attr&&data.packet.u8Length==DIAG_PACKET_BYTES);
    assert(sends<200);memcpy(sent[sends++],data.packet.pu8Data,DIAG_PACKET_BYTES);return TRUE;
}
static void drain(void) {
    unsigned budget=1000;
    while(APP_bDiagnosticBusy()&&budget--) {
        assert(timer);timer=0;APP_cbTimerDiagnostic(NULL);APP_vDiagnosticProcess();
    }
    assert(budget && !activities);
}
int main(void) {
    tsZCL_ClusterInstance instance;
    diag_record test;
    uint8 packet[DIAG_PACKET_BYTES];
    assert(sizeof(test)==272);
    assert(diag_case_at(15)&&!diag_case_at(16));
    diag_record_begin(&test,0x12345678);diag_record_finish(&test);assert(!diag_record_valid(&test));
    for(unsigned n=0;n<DIAG_CASES;++n)diag_record_row(&test,n)[0]=n+1;
    diag_record_finish(&test);assert(diag_record_valid(&test));
    for(unsigned n=0;n<sizeof(test);++n) {test.bytes[n]^=1;assert(!diag_record_valid(&test));test.bytes[n]^=1;}
    diag_packet(&test,0,1,0,packet);assert(packet[2]==0x78&&packet[5]==0x12&&packet[7]==3&&packet[8]==1);
    diag_packet(&test,16,1,1,packet);assert(packet[6]==255&&packet[7]==7&&packet[8]==16);
    APP_vDiagnosticCluster(&instance,&data);APP_vDiagnosticInit();APP_vDiagnosticProcess();
    assert(!APP_bDiagnosticBusy()&&!saves&&!captures);
    APP_vDiagnosticRequest();APP_vDiagnosticRequest();drain();
    assert(captures==16&&saves==1&&diag_record_valid(&nvm));
    APP_vDiagnosticProcess();drain();assert(sends==18&&sent[0][7]==3&&sent[17][6]==255);
    APP_vDiagnosticWake();APP_vDiagnosticProcess();drain();assert(sends==36&&captures==16&&saves==1);
    /* Reboot replays the saved suite without touching the sensor or writing. */
    APP_vDiagnosticInit();APP_vDiagnosticProcess();drain();assert(sends==54&&sent[36][7]==7&&captures==16&&saves==1);
    /* Failed save preserves the previous durable run and reports RAM as not durable. */
    save_fail=1;APP_vDiagnosticRequest();drain();APP_vDiagnosticProcess();drain();
    assert(captures==32&&saves==2&&diag_record_run(&nvm)==1&&sent[54][7]==2);
    save_fail=0;APP_vDiagnosticInit();read_corrupt=1;
    APP_vDiagnosticRequest();drain();APP_vDiagnosticProcess();drain();assert(!(sent[72][7]&1));
    read_corrupt=0;
    unsigned previous=saves;
    timer_fail=1;APP_vDiagnosticRequest();assert(!APP_bDiagnosticBusy()&&!activities&&saves==previous);
    timer_fail=0;APP_vDiagnosticRequest();
    timer=0;APP_cbTimerDiagnostic(NULL);APP_vDiagnosticProcess();
    timer_fail=1;timer=0;APP_cbTimerDiagnostic(NULL);APP_vDiagnosticProcess();
    assert(!APP_bDiagnosticBusy()&&!activities&&saves==previous);
    /* Collection needs no network; joining later sends the saved lot. */
    timer_fail=0;joined=0;
    unsigned previous_sends=sends,previous_captures=captures;
    APP_vDiagnosticRequest();drain();APP_vDiagnosticProcess();
    assert(saves==previous+1&&captures==previous_captures+16&&sends==previous_sends);
    joined=1;APP_vDiagnosticProcess();drain();
    assert(sends==previous_sends+18&&captures==previous_captures+16);
    return 0;
}
