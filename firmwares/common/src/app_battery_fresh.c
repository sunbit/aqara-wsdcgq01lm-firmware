#include <jendefs.h>
#include "AppHardwareApi.h"
#include "zcl.h"
#include "zcl_customcommand.h"
#include "PowerConfiguration.h"
#include "dbg.h"
#include "app_battery.h"
#include "app_battery_fresh.h"
#include "app_sensor.h"

PUBLIC void APP_vBatteryReportFresh(void)
{
    (void)APP_bBatteryReportFresh();
}

PUBLIC bool_t APP_bBatteryMeasureMillivolts(uint32 *millivolts)
{
    uint32 budget = 500000UL;
    uint16 raw;
    vAHI_AdcEnable(E_AHI_ADC_SINGLE_SHOT, E_AHI_AP_INPUT_RANGE_2, E_AHI_ADC_SRC_VOLT);
    vAHI_AdcStartSample();
    while (bAHI_AdcPoll()) {
        if (!--budget) {
            vAHI_AdcDisable();
            DBG_vPrintf(TRUE, "BATTERY fresh ADC timeout; no cached battery report sent\n");
            return FALSE;
        }
    }
    vAHI_AdcDisable();
    raw = u16AHI_AdcRead();
    *millivolts = ((uint32)raw * 7410UL) >> 11;
    return TRUE;
}

PUBLIC bool_t APP_bBatteryReportFresh(void)
{
    uint32 millivolts, percentage;
    tsZCL_ClusterInstance *power;
    bool_t voltage_sent, percentage_sent;
    if (!APP_bBatteryMeasureMillivolts(&millivolts)) return FALSE;
    percentage = millivolts <= BATTERY_MIN_MV ? 0 :
                 (millivolts - BATTERY_MIN_MV) * 100 / BATTERY_DELTA_MV;
    if (percentage > 100) percentage = 100;
    if (eZCL_SearchForClusterEntry(1, GENERAL_CLUSTER_ID_POWER_CONFIGURATION, TRUE, &power) != E_ZCL_SUCCESS) {
        DBG_vPrintf(TRUE, "BATTERY fresh power cluster unavailable; report skipped\n");
        return FALSE;
    }
    ((tsCLD_PowerConfiguration *)power->pvEndPointSharedStructPtr)->u8BatteryVoltage = millivolts / 100;
    ((tsCLD_PowerConfiguration *)power->pvEndPointSharedStructPtr)->u8BatteryPercentageRemaining = percentage * 2;
    DBG_vPrintf(TRUE, "BATTERY fresh voltage_mv=%d percentage=%d\n", millivolts, percentage);
    voltage_sent = APP_bSensorReportAttribute(GENERAL_CLUSTER_ID_POWER_CONFIGURATION, 0x0020);
    percentage_sent = APP_bSensorReportAttribute(GENERAL_CLUSTER_ID_POWER_CONFIGURATION, 0x0021);
    DBG_vPrintf(TRUE, "BATTERY fresh reports submitted voltage=%d percentage=%d\n",
                voltage_sent, percentage_sent);
    return voltage_sent && percentage_sent;
}
