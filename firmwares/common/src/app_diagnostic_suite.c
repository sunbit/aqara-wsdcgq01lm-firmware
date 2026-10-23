#include <jendefs.h>
#include <stddef.h>
#include <string.h>
#include "app_diagnostic_suite.h"
#include "app_resources.h"
#include "app_sensor.h"
#include "app_sensor_led.h"
#include "bdb_api.h"
#include "app_node.h"
#include "app_polling.h"
#include "AppHardwareApi.h"
#include "ZTimer.h"
#include "PDM.h"
#include "pwrm.h"
#include "zps_apl_af.h"
#include "dbg.h"

PRIVATE diag_record record;
PRIVATE diagnostic_cluster_data *cluster_data;
PRIVATE uint8 packet[DIAG_PACKET_BYTES];
PRIVATE uint8 control[1];
PRIVATE bool_t durable, restored, pending_replay, timer_due;
PRIVATE unsigned state, case_index, retries, rounds;
PRIVATE uint32 last_run;
/* 0 idle, 1 initial settle, 2 tests, 3 report burst. One activity per phase. */
PRIVATE const tsZCL_AttributeDefinition attributes[] = {
    /* SDK eZCL_ReportAttribute supports non-MS attributes. The private cluster
     * and versioned packet carry diagnostics without abusing sensor values. */
    {0, E_ZCL_AF_RD|E_ZCL_AF_RP, E_ZCL_OSTRING,
        (uint32)offsetof(diagnostic_cluster_data, packet), 0}
};
PRIVATE tsZCL_ClusterDefinition definition = {
    DIAG_CLUSTER_ID, TRUE, E_ZCL_SECURITY_NETWORK, 1,
    (tsZCL_AttributeDefinition *)attributes, NULL
};

PUBLIC void APP_vDiagnosticCluster(tsZCL_ClusterInstance *instance, diagnostic_cluster_data *data)
{
    cluster_data=data;
    data->packet.u8MaxLength=DIAG_PACKET_BYTES;
    data->packet.u8Length=0;
    data->packet.pu8Data=packet;
    vZCL_InitializeClusterInstance(instance,TRUE,&definition,data,control,NULL,NULL);
}

PRIVATE void idle(void)
{
    ZTIMER_eStop(u8TimerDiagnostic);
    if (state) PWRM_eFinishActivity();
    state=0; timer_due=FALSE;
    vAHI_DioSetOutput(SENSOR_LED_MASK,0);
}
PRIVATE bool_t schedule(unsigned ms)
{
    if (ZTIMER_eStart(u8TimerDiagnostic,ZTIMER_TIME_MSEC(ms))==E_ZTIMER_OK) return TRUE;
    /* A partial RAM record is never persisted or labelled complete. */
    idle(); return FALSE;
}
PUBLIC bool_t APP_bDiagnosticBusy(void) {return state!=0;}
PUBLIC void APP_cbTimerDiagnostic(void *context) {(void)context; if (state) timer_due=TRUE;}

PUBLIC void APP_vDiagnosticInit(void)
{
    uint16 bytes=0;
    state=0; timer_due=FALSE; pending_replay=FALSE; rounds=0;
    memset(&record,0,sizeof(record));
    durable=PDM_eReadDataFromRecord(DIAG_PDM_ID,&record,sizeof(record),&bytes)==PDM_E_STATUS_OK &&
        bytes==sizeof(record) && diag_record_valid(&record);
    restored=durable;
    last_run=durable ? diag_record_run(&record) : 0;
    if (durable) {rounds=3; pending_replay=TRUE;}
    (void)eZCL_SetReportableFlag(1,DIAG_CLUSTER_ID,TRUE,FALSE,0);
}

PUBLIC void APP_vDiagnosticRequest(void)
{
    uint32 run;
    if (state) return;
    run=++last_run;
    if (!run) run=1;
    last_run=run; timer_due=FALSE;
    diag_record_begin(&record,run);
    durable=restored=pending_replay=FALSE; rounds=0; case_index=0;
    state=1; PWRM_eStartActivity();
    /* Allow the original feedback flash to finish before the LED-OFF case. */
    (void)schedule(500);
}
PUBLIC void APP_vDiagnosticWake(void)
{
    if (!state && rounds && diag_record_valid(&record)) pending_replay=TRUE;
}
PRIVATE void begin_replay(void)
{
    if (!state && pending_replay && bNodeJoined()) {
        pending_replay=FALSE; state=3; case_index=0; retries=0;
        PWRM_eStartActivity();
        (void)ZTIMER_eStart(u8TimerZclTick,ZCL_TICK_TIME_MSEC);
        ZPS_eAplAfSendKeepAlive(); APP_vStartPolling(POLL_REGULAR);
        (void)schedule(500);
    }
}

PUBLIC void APP_vDiagnosticProcess(void)
{
    begin_replay();
    if (!timer_due || !state) return;
    timer_due=FALSE;
    if (state==1) {
        APP_vSetupLeds(); state=2;
    }
    if (state==2) {
        uint8 *row=diag_record_row(&record,case_index);
        APP_vSensorDiagnosticCapture(diag_case_at(case_index),row);
        row[0]=(uint8)(case_index+1);
        if (++case_index<DIAG_CASES) {(void)schedule(250); return;}
        /* Exactly one write for this completed suite, not one per case. */
        {
            diag_record verify;
            uint16 bytes=0;
            diag_record_finish(&record);
            durable=PDM_eSaveRecordData(DIAG_PDM_ID,&record,sizeof(record))==PDM_E_STATUS_OK;
            if (durable) durable=PDM_eReadDataFromRecord(DIAG_PDM_ID,&verify,sizeof(verify),&bytes)==PDM_E_STATUS_OK &&
                bytes==sizeof(verify) && memcmp(&record,&verify,sizeof(record))==0;
        }
        rounds=3; pending_replay=TRUE;
        DBG_vPrintf(TRUE,"DIAGNOSTIC suite complete run=%d durable=%d\n",diag_record_run(&record),durable);
        idle();
        /* Hold a reporting activity before the main loop can enter sleep. */
        begin_replay(); return;
    }
    if (state==3) {
        /* Summary, cases, summary. Every packet contains run ID and case ID;
         * duplicates/reordering are handled by the receiver without guessing. */
        unsigned slot=(case_index==0 || case_index==DIAG_CASES+1) ? DIAG_CASES : case_index-1;
        diag_packet(&record,slot,durable,restored,packet);
        cluster_data->packet.u8Length=DIAG_PACKET_BYTES;
        if (!APP_bSensorReportAttribute(DIAG_CLUSTER_ID,0) && ++retries<3) {
            (void)schedule(500); return;
        }
        retries=0;
        if (++case_index<DIAG_CASES+2) {(void)schedule(500); return;}
        if (rounds) --rounds;
        idle();
    }
}
