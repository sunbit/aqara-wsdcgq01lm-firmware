#include <jendefs.h>
#include <stddef.h>
#include <string.h>
#include "dbg.h"
#include "zcl.h"
#include "PDM.h"
#include "ConfigurationCluster.h"
#include "app_sensor_events.h"

#define CONFIG_MAGIC 0x53434601UL
#define CONFIG_ATTR(id, type, member) \
    {id, E_ZCL_AF_RD | E_ZCL_AF_WR | E_ZCL_AF_MS, type, \
     (uint32)offsetof(tsCLD_Configuration, member), 0}

const tsZCL_AttributeDefinition asCLD_ConfigurationClusterAttributeDefinitions[] = {
    CONFIG_ATTR(E_CLD_BUTTON_MODE_ATTR_ID_MODE_TYPE, E_ZCL_ENUM8, eButtonMode),
    CONFIG_ATTR(E_CLD_PREVENT_RESET_ATTR_ID_MODE_TYPE, E_ZCL_BOOL, bPreventReset),
    CONFIG_ATTR(SENSOR_ATTR_TEMPERATURE_OFFSET, E_ZCL_INT16, sensor.temperature_offset),
    CONFIG_ATTR(SENSOR_ATTR_HUMIDITY_OFFSET, E_ZCL_INT16, sensor.humidity_offset),
    CONFIG_ATTR(SENSOR_ATTR_TEMPERATURE_DELTA, E_ZCL_UINT16, sensor.temperature_delta),
    CONFIG_ATTR(SENSOR_ATTR_HUMIDITY_DELTA, E_ZCL_UINT16, sensor.humidity_delta),
    CONFIG_ATTR(SENSOR_ATTR_SAMPLE_SECONDS, E_ZCL_UINT16, sensor.sample_seconds),
    CONFIG_ATTR(SENSOR_ATTR_TEMPERATURE_MAX_SECONDS, E_ZCL_UINT32, sensor.temperature_max_seconds),
    CONFIG_ATTR(SENSOR_ATTR_HUMIDITY_MAX_SECONDS, E_ZCL_UINT32, sensor.humidity_max_seconds),
    CONFIG_ATTR(SENSOR_ATTR_BATTERY_SECONDS, E_ZCL_UINT32, sensor.battery_seconds),
    CONFIG_ATTR(SENSOR_ATTR_TEMPERATURE_MIN_SECONDS, E_ZCL_UINT16, sensor.temperature_min_seconds),
    CONFIG_ATTR(SENSOR_ATTR_HUMIDITY_MIN_SECONDS, E_ZCL_UINT16, sensor.humidity_min_seconds)
};

tsZCL_ClusterDefinition sCLD_Configuration = {
    GENERAL_CLUSTER_ID_CONFIGURATION, TRUE, E_ZCL_SECURITY_NETWORK,
    sizeof(asCLD_ConfigurationClusterAttributeDefinitions) / sizeof(tsZCL_AttributeDefinition),
    (tsZCL_AttributeDefinition *)asCLD_ConfigurationClusterAttributeDefinitions, NULL
};
uint8 au8ConfigurationAttributeControlBits[
    sizeof(asCLD_ConfigurationClusterAttributeDefinitions) / sizeof(tsZCL_AttributeDefinition)];

typedef struct { uint32 magic; tsCLD_Configuration config; } config_record;
PRIVATE tsCLD_Configuration *active;
PRIVATE tsCLD_Configuration saved;

PUBLIC const sensor_config *APP_psSensorConfig(void)
{
    return &active->sensor; /* Registered and loaded before application tasks. */
}

PUBLIC teZCL_Status eCLD_ConfigurationCreateConfiguration(
    tsZCL_ClusterInstance *instance, bool_t server, tsZCL_ClusterDefinition *definition,
    void *shared, uint8 *control)
{
    config_record record;
    uint16 bytes = 0;
    uint8 legacy[2];
    vZCL_InitializeClusterInstance(instance, server, definition, shared, control, NULL, NULL);
    active = (tsCLD_Configuration *)shared;
    memset(active, 0, sizeof(*active));
    sensor_config_defaults(&active->sensor);
    /* Migrate the original two-byte record without changing the network/PDM
     * format marker. An upgrade must not silently erase the pairing. */
    if (PDM_eReadDataFromRecord(PDM_ID_CONFIGURATION_CLUSTER, legacy, sizeof(legacy), &bytes) == PDM_E_STATUS_OK &&
        bytes == sizeof(legacy) && legacy[0] <= E_CLD_BUTTON_MODE_MULTISTATE_INPUT && legacy[1] <= 1) {
        active->eButtonMode = legacy[0];
        active->bPreventReset = legacy[1];
    }
    bytes = 0;
    memset(&record, 0, sizeof(record));
    if (PDM_eReadDataFromRecord(PDM_ID_SENSOR_CONFIGURATION, &record, sizeof(record), &bytes) == PDM_E_STATUS_OK &&
        bytes == sizeof(record) && record.magic == CONFIG_MAGIC &&
        record.config.eButtonMode <= E_CLD_BUTTON_MODE_MULTISTATE_INPUT &&
        record.config.bPreventReset <= 1 && sensor_config_valid(&record.config.sensor))
        *active = record.config;
    saved = *active;
    return E_ZCL_SUCCESS;
}

PUBLIC void vHandleConfigurationClusterAttrsRange(tsZCL_CallBackEvent *event)
{
    uint16 id = event->uMessage.sIndividualAttributeResponse.u16AttributeEnum;
    const uint8 *raw = event->uMessage.sIndividualAttributeResponse.pvAttributeData;
    sensor_config candidate = active->sensor;
    int32_t value;
    bool_t valid;
    /* The range callback points at unaligned little-endian APDU bytes, not
     * native attribute storage. Never dereference it as uint16/uint32. */
    if (id == E_CLD_BUTTON_MODE_ATTR_ID_MODE_TYPE)
        valid = raw[0] <= E_CLD_BUTTON_MODE_MULTISTATE_INPUT;
    else if (id == E_CLD_PREVENT_RESET_ATTR_ID_MODE_TYPE)
        valid = raw[0] <= 1;
    else {
        value = (int32_t)raw[0] | ((int32_t)raw[1] << 8);
        if (id == SENSOR_ATTR_TEMPERATURE_OFFSET || id == SENSOR_ATTR_HUMIDITY_OFFSET) {
            if (value & 0x8000) value -= 65536;
        } else if (id >= SENSOR_ATTR_TEMPERATURE_MAX_SECONDS && id <= SENSOR_ATTR_BATTERY_SECONDS) {
            if (raw[3] & 0x80) {
                event->uMessage.sIndividualAttributeResponse.eAttributeStatus = E_ZCL_ERR_ATTRIBUTE_RANGE;
                return;
            }
            value |= ((int32_t)raw[2] << 16) | ((int32_t)raw[3] << 24);
        }
        valid = sensor_config_set(&candidate, id, value);
    }
    if (!valid) event->uMessage.sIndividualAttributeResponse.eAttributeStatus = E_ZCL_ERR_ATTRIBUTE_RANGE;
}

PUBLIC void vHandleConfigurationClusterAttrsWrite(tsZCL_CallBackEvent *event, tsCLD_Configuration *config)
{
    config_record record;
    if (event->uMessage.sIndividualAttributeResponse.eAttributeStatus != E_ZCL_CMDS_SUCCESS ||
        memcmp(config, &saved, sizeof(saved)) == 0) return;
    memset(&record, 0, sizeof(record));
    record.magic = CONFIG_MAGIC;
    record.config = *config;
    if (PDM_eSaveRecordData(PDM_ID_SENSOR_CONFIGURATION, &record, sizeof(record)) != PDM_E_STATUS_OK) {
        *config = saved;
        event->uMessage.sIndividualAttributeResponse.eAttributeStatus = E_ZCL_CMDS_FAILURE;
        DBG_vPrintf(TRUE, "CONFIG: persistence failed; restored previous settings\n");
        /* This SDK does not propagate post-write callback errors in its write
         * response. The converter verifies every setting with read-after-write. */
        return;
    }
    saved = *config;
    APP_vSendConfigurationEvent();
}

PUBLIC void vResetConfigurationCluster(void)
{
    PDM_vDeleteDataRecord(PDM_ID_CONFIGURATION_CLUSTER);
    PDM_vDeleteDataRecord(PDM_ID_SENSOR_CONFIGURATION);
}
