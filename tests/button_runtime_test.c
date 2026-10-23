#include <assert.h>
#include <stdio.h>
#include "sdk.h"
#include "app_sensor.h"
#include "app_sensor_button.h"
#include "app_sensor_events.h"
#include "app_battery_fresh.h"
#include "app_sensor_led.h"

uint8 u8TimerButtonScan=1, u8TimerZclTick=2, u8TimerLedBlink=3;
static uint32 input=0xfffff, rising, falling, enabled;
static int scan_timer, led_timer, led_on, led_fail, activities, joined=1;
static unsigned samples, forced, periodic, reports, adc_samples, battery_cycles;
static unsigned network_resets, software_resets, invalidations, led_pulses;
static int adc_stuck, cluster_missing;
static tsCLD_PowerConfiguration power;
static tsZCL_ClusterInstance cluster={&power};
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
static unsigned diagnostic_runs, diagnostic_wakes;
static int diagnostic_busy;
int APP_bDiagnosticBusy(void) {return diagnostic_busy;}
void APP_vDiagnosticRequest(void) {if(!diagnostic_busy){++diagnostic_runs;diagnostic_busy=1;}}
void APP_vDiagnosticWake(void) {++diagnostic_wakes;}
#endif
void DBG_vPrintf(int enabled_log,const char *format,...) {(void)enabled_log;(void)format;}
uint32 u32AHI_DioReadInput(void) {return input;}
uint32 u32AHI_DioInterruptStatus(void) {return 0;}
void vAHI_DioInterruptEnable(uint32 on,uint32 off) {enabled=(enabled|on)&~off;}
void vAHI_DioInterruptEdge(uint32 up,uint32 down) {rising=up;falling=down;}
void vAHI_DioSetDirection(uint32 in,uint32 out) {
    assert((in==SENSOR_BUTTON_MASK && !out) || (!in && out==SENSOR_LED_MASK));
}
void vAHI_DioSetPullup(uint32 on,uint32 off) {
    assert((on==SENSOR_BUTTON_MASK && !off) || (!on && off==SENSOR_LED_MASK));
}
void vAHI_DioSetOutput(uint32 on,uint32 off) {
    assert((on|off)==SENSOR_LED_MASK && !(on&off));
    if(off && !led_on) ++led_pulses;
    led_on=!!off;
}
int ZTIMER_eStop(uint8 timer) {if(timer==1)scan_timer=0;if(timer==3)led_timer=0;return 0;}
int ZTIMER_eStart(uint8 timer,uint32 period) {
    if(timer==1) {assert(period==10);scan_timer=1;}
    if(timer==3) {assert(period==150);if(led_fail)return 1;led_timer=1;}
    return 0;
}
int PWRM_eStartActivity(void) {return ++activities;}
int PWRM_eFinishActivity(void) {assert(activities>0);return --activities;}
int ZPS_eAplAfSendKeepAlive(void) {return 0;}
int bNodeJoined(void) {return joined;}
void APP_vResetNetworkRecords(void) {++network_resets;joined=0;}
void vAHI_SwReset(void) {++software_resets;}
void APP_vSensorInvalidate(void) {++invalidations;}
void APP_vStartPolling(int mode) {assert(mode==POLL_REGULAR);}
void APP_vMeasureRemainingBattery(void) {++battery_cycles;}
void APP_vSensorBatteryReport(bool_t force) {
    if(force) (void)APP_bBatteryReportFresh(); else ++battery_cycles;
}
void APP_vSensorSampleRequested(bool_t force,bool_t scheduled) {++samples;forced+=!!force;periodic+=!!scheduled;}
void vAHI_AdcEnable(int mode,int range,int source) {assert(mode==0&&range==1&&source==2);}
void vAHI_AdcStartSample(void) {++adc_samples;}
int bAHI_AdcPoll(void) {return adc_stuck;}
void vAHI_AdcDisable(void) {}
uint16 u16AHI_AdcRead(void) {return 800;}
int eZCL_SearchForClusterEntry(uint8 ep,uint16 id,int server,tsZCL_ClusterInstance **result) {
    assert(ep==1&&id==1&&server);*result=&cluster;return cluster_missing;
}
bool_t APP_bSensorReportAttribute(uint16 id,uint16 attr) {assert(id==1&&(attr==0x20||attr==0x21));++reports;return TRUE;}
static void level(int pressed) {if(pressed)input&=~SENSOR_BUTTON_MASK;else input|=SENSOR_BUTTON_MASK;}
static void irq(int pressed) {level(pressed);APP_vSensorButtonInterrupt(SENSOR_BUTTON_MASK);assert(scan_timer);}
static void tick(void) {assert(scan_timer);scan_timer=0;APP_cbTimerButtonScan(NULL);APP_vProcessEvents();}
static void edge(void) {tick();tick();tick();}
#ifdef SENSOR_LED_FEEDBACK
static void led_tick(void) {assert(led_timer);led_timer=0;APP_cbTimerLedBlink(NULL);APP_vProcessEvents();}
#endif
int main(void)
{
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
    APP_vSetupLeds();APP_vConfigureButtons();
    irq(1);edge();assert(diagnostic_runs==1&&!scan_timer&&!samples&&!reports);
    led_tick();assert(!led_on);
    APP_vSendResetDeviceEvent();APP_vProcessEvents();assert(!network_resets);
    irq(0);edge();assert(!scan_timer);
    irq(1);edge();assert(diagnostic_runs==1&&!led_on&&!scan_timer);
    irq(0);edge();diagnostic_busy=0;
    irq(1);edge();assert(diagnostic_runs==2&&led_on&&!scan_timer);
    led_tick();APP_vSendSensorTimerEvent();APP_vProcessEvents();
    assert(diagnostic_wakes==1&&!samples&&!reports&&!network_resets&&!activities);
    puts("PASS: diagnostic press/release rearming, busy guard and no network reset");
    return 0;
#endif
#ifdef SENSOR_LED_FEEDBACK
    APP_vSetupLeds();assert(!led_on&&!led_timer);
#endif
    APP_vConfigureButtons();assert(enabled==SENSOR_BUTTON_MASK&&falling==SENSOR_BUTTON_MASK&&!rising);
    /* Bounce creates neither a report nor a long press. */
    irq(1);level(0);tick();assert(!scan_timer&&!samples&&!activities);
    APP_vSendSensorTimerEvent();irq(1);APP_vProcessEvents();assert(!samples&&activities==1);
    edge();assert(samples==1&&forced==1&&periodic==1&&reports==2&&adc_samples==1&&!activities);
    assert(!APP_bSensorButtonDebouncing()&&scan_timer); /* Fresh read before release. */
#ifdef SENSOR_LED_FEEDBACK
    assert(led_on&&led_pulses==1);led_tick();assert(!led_on);
#endif
    /* Release immediately below 5 s: no confirmation or reset. */
    for(unsigned n=0;n<499;++n)tick();
    assert(!network_resets);irq(0);edge();assert(!scan_timer&&!activities);
    /* A full hold triggers once and does not emit repeated measurements. */
    irq(1);edge();assert(samples==2&&forced==2&&reports==4);
#ifdef SENSOR_LED_FEEDBACK
    led_tick();
#endif
    for(unsigned n=0;n<500;++n)tick();
    assert(!scan_timer);
#ifdef SENSOR_LED_FEEDBACK
    assert(!network_resets&&led_on);
    unsigned before=led_pulses;
    APP_vSensorLedPress(); /* Cannot interrupt a reset confirmation. */
    led_tick();assert(!led_on&&!network_resets);
    led_tick();assert(led_on&&!network_resets);
    led_tick();assert(!led_on&&!network_resets);
    led_tick();assert(led_on&&!network_resets);
    led_tick();assert(!led_on&&!led_timer&&led_pulses==before+2);
#endif
    assert(network_resets==1&&software_resets==1&&!activities);
    for(unsigned n=0;n<10;++n) {APP_vSensorButtonWake(0);APP_vProcessEvents();}
    assert(!scan_timer&&network_resets==1);
    /* Still held across reboot: ignore until released, no reset loop. */
    APP_vConfigureButtons();assert(!scan_timer);
    irq(0);edge();assert(!scan_timer);
    joined=1;
    APP_vSendConfigurationEvent();APP_vProcessEvents();
    assert(invalidations==1&&samples==3&&forced==3&&reports==6);
    APP_vSendSensorTimerEvent();APP_vProcessEvents();assert(samples==4&&periodic==2&&battery_cycles==1);
    adc_stuck=1;assert(!APP_bBatteryReportFresh()&&reports==6);adc_stuck=0;
    cluster_missing=1;assert(!APP_bBatteryReportFresh()&&reports==6);cluster_missing=0;
#ifdef SENSOR_LED_FEEDBACK
    /* LED timer failure must never silently erase the pairing. */
    led_fail=1;APP_vSensorLedPairing();APP_vProcessEvents();
    assert(!led_on&&!led_timer&&network_resets==1);led_fail=0;
#endif
    APP_vSendFreshSensorEvent();APP_vSendSensorTimerEvent();APP_vSendResetDeviceEvent();
    APP_vProcessEvents();assert(network_resets==2&&samples==4&&!activities);
    puts("PASS: debounce, fresh press, 5s boundary, three blinks before reset, held/reboot guard, configuration and failure handling");
    return 0;
}
