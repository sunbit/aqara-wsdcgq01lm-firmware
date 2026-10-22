/* Experimental WSDCGQ01LM measurement/reporting path. */
#include <jendefs.h>
#include <string.h>
#include "AppHardwareApi.h"
#include "zcl.h"
#include "zcl_customcommand.h"
#include "TemperatureMeasurement.h"
#include "RelativeHumidityMeasurement.h"
#include "app_basic_endpoint.h"
#include "app_sensor.h"
#include "sensor_math.h"
#include "sensor_probe.h"
#include "sensor_timing.h"
#include "sensor_wire.h"
#include "sensor_policy.h"
#include "dbg.h"

#define SAMPLE_SECONDS SENSOR_SAMPLE_SECONDS
#define BUS_TIMEOUT_LOOPS 500000UL /* Bounded polling, including SHT clock stretching. */
#define SENSOR_DEFAULT_PRESCALER 31 /* 100 kHz: CRC-valid ht01 measurement in v3. */
#define SENSOR_MEASUREMENT_COMMAND 0x2416 /* Low repeatability: v5 passed 3/3 with no resets. */

PRIVATE int16_t previous_t;
PRIVATE uint16_t previous_h;
PRIVATE unsigned age_t, age_h;
PRIVATE bool_t have_t, have_h;
PRIVATE sht_route sensor_route;
PRIVATE const char *sensor_stage, *sensor_fault;
PRIVATE unsigned sensor_byte;

PRIVATE uint32_t read_stack_tick(void *context)
{
    (void)context;
    return u32AHI_TickTimerRead();
}

PRIVATE bool_t wait_sensor_ticks(uint32 required)
{
    /* SDK ZTimer.c restarts this counter at 16000 (1 ms), not at 2^32.
     * Count these wraps without resetting or reconfiguring the stack timer. */
    if (sht_wait_ticks(read_stack_tick, NULL, 16000UL, required, 3000000UL)) return TRUE;
    sensor_fault = "tick-timeout";
    return FALSE;
}

/* A stuck bus cannot trap the firmware indefinitely. The final NACK while
 * receiving is intentional; do not treat it as a slave-address rejection. */
PRIVATE bool_t transfer(bool_t start, bool_t stop, bool_t read, bool_t nack)
{
    uint32 budget = BUS_TIMEOUT_LOOPS;
    if (!bAHI_SiMasterSetCmdReg(start, stop, read, !read, nack, E_AHI_SI_NO_IRQ_ACK)) {
        sensor_fault = "invalid-command";
        return FALSE;
    }
    /* Let the peripheral latch its command before first polling TIP. This is
     * a conservative guard, not a confirmed diagnosis of the v2 failure. */
    if (!wait_sensor_ticks(32UL)) return FALSE; /* 2 us, at either bus rate. */
    while (bAHI_SiMasterPollTransferInProgress()) {
        if (--budget == 0) {
            sensor_fault = "transfer-timeout";
            return FALSE;
        }
    }
    if (bAHI_SiMasterPollArbitrationLost()) {
        sensor_fault = "arbitration-lost";
        return FALSE;
    }
    if (!read && bAHI_SiMasterCheckRxNack()) {
        sensor_fault = "nack";
        return FALSE;
    }
    return TRUE;
}

PRIVATE void wire_address(uint8_t address, int read, void *context)
{
    (void)context;
    vAHI_SiMasterWriteSlaveAddr(address, read != 0);
}

PRIVATE void wire_write(uint8_t value, void *context)
{
    (void)context;
    vAHI_SiMasterWriteData8(value);
}

PRIVATE uint8_t wire_read(void *context)
{
    (void)context;
    return u8AHI_SiMasterReadData8();
}

PRIVATE int wire_transfer(int start, int stop, int read, int nack, void *context)
{
    (void)context;
    return transfer(start != 0, stop != 0, read != 0, nack != 0);
}

PRIVATE int wire_wait_us(uint32_t microseconds, void *context)
{
    (void)context;
    return wait_sensor_ticks(microseconds * 16UL);
}

PRIVATE void wire_stage(const char *name, unsigned byte, void *context)
{
    (void)context;
    sensor_stage = name;
    sensor_byte = byte;
}

PRIVATE const sht_wire_io sensor_io = {
    wire_address, wire_write, wire_read, wire_transfer, wire_wait_us, wire_stage
};

PRIVATE bool_t command_at(unsigned location, uint8 address, uint8 prescaler,
                          uint16 command, uint32 wait_us, uint8 *data,
                          unsigned length, const char *label)
{
    uint32 pins = location ? ((1UL << 16) | (1UL << 17)) :
                             ((1UL << 14) | (1UL << 15));
    bool_t received;
    uint32 idle_levels;
    /* Only the two documented I2C routes are exercised. Never drive an
     * unknown GPIO high to guess sensor power/reset, and release after every
     * attempt, including address NACK, timeout and invalid sensor response. */
    vAHI_SiMasterDisable();
    vAHI_DioSetDirection(pins, 0);
    vAHI_DioSetPullup(pins, 0);
    vAHI_SiMasterConfigure(TRUE, FALSE, prescaler);
    vAHI_SiSetLocation(location != 0);
    sensor_stage = "bus-settle";
    sensor_fault = "none";
    sensor_byte = 0;
    received = wait_sensor_ticks(1600UL); /* 100 us for the idle lines to settle. */
    idle_levels = u32AHI_DioReadInput();
    if (received) received = sht_command_read(&sensor_io, NULL, address,
                                              command, wait_us, data, length);
    if (!received) {
        uint32 budget = BUS_TIMEOUT_LOOPS;
        /* End an incomplete transaction before disabling the controller.
         * Preserve the original failure stage/reason for the UART report. */
        (void)bAHI_SiMasterSetCmdReg(FALSE, TRUE, FALSE, FALSE,
                                    E_AHI_SI_SEND_ACK, E_AHI_SI_NO_IRQ_ACK);
        while (bAHI_SiMasterPollTransferInProgress() && --budget) {}
    }
    vAHI_SiMasterDisable();
    vAHI_DioSetDirection(pins, 0);
    vAHI_DioSetPullup(0, pins);
    DBG_vPrintf(TRUE, "SENSOR probe label=%s SCL=DIO%d SDA=DIO%d addr=0x%02x hz=%d cmd=0x%04x received=%d\n",
                label, location ? 16 : 14, location ? 17 : 15, address,
                3200000UL / ((uint32)prescaler + 1), command, received);
    DBG_vPrintf(TRUE, "SENSOR detail stage=%s reason=%s byte=%d idle_scl=%d idle_sda=%d\n",
                sensor_stage, sensor_fault, sensor_byte,
                !!(idle_levels & (1UL << (location ? 16 : 14))),
                !!(idle_levels & (1UL << (location ? 17 : 15))));
    if (received && length >= 3) {
        uint8 expected = sht_crc(data, 2);
        DBG_vPrintf(TRUE, "SENSOR word1 raw=%02x%02x crc=%02x expected=%02x ok=%d\n",
                    data[0], data[1], data[2], expected, data[2] == expected);
    }
    if (received && length >= 6) {
        uint8 expected = sht_crc(data + 3, 2);
        DBG_vPrintf(TRUE, "SENSOR word2 raw=%02x%02x crc=%02x expected=%02x ok=%d\n",
                    data[3], data[4], data[5], expected, data[5] == expected);
    }
    return received;
}

PRIVATE int read_at(unsigned location, uint8_t address, uint8_t data[6], void *context)
{
    (void)context;
    return command_at(location, address, SENSOR_DEFAULT_PRESCALER,
                      SENSOR_MEASUREMENT_COMMAND, 20000UL, data, 6, "measurement-low");
}

PRIVATE void diagnostic_registers(uint8 prescaler)
{
    uint8 data[6];
    /* v2 found an ACK only on this route. These commands exercise the
     * documented SHT3x interface, never unknown GPIO power/reset pins. */
    (void)command_at(0, 0x44, prescaler, 0xf32d, 1000UL, data, 3, "status-before");
    (void)command_at(0, 0x44, prescaler, 0x3780, 1000UL, data, 6, "serial");
    (void)command_at(0, 0x44, prescaler, 0x3093, 1000UL, NULL, 0, "break-periodic");
    (void)command_at(0, 0x44, prescaler, 0x30a2, 2000UL, NULL, 0, "soft-reset");
    (void)command_at(0, 0x44, prescaler, 0xf32d, 1000UL, data, 3, "status-after-reset");
}

PRIVATE bool_t diagnostic_status(const char *label, uint16 *status)
{
    uint8 data[3];
    if (!command_at(0, 0x44, SENSOR_DEFAULT_PRESCALER,
                    0xf32d, 1000UL, data, 3, label) ||
        sht_crc(data, 2) != data[2]) return FALSE;
    *status = ((uint16)data[0] << 8) | data[1];
    DBG_vPrintf(TRUE, "SENSOR status label=%s value=0x%04x reset=%d command_error=%d write_crc_error=%d\n",
                label, *status, !!(*status & 0x10), !!(*status & 2), !!(*status & 1));
    return TRUE;
}

PRIVATE bool_t read_sensor(int16_t *temperature, uint16_t *humidity)
{
    bool_t valid = sht_probe(&sensor_route, read_at, NULL, temperature, humidity);
    if (valid) {
        DBG_vPrintf(TRUE, "SENSOR CRC OK SCL=DIO%d SDA=DIO%d addr=0x%02x temperature_centi=%d humidity_centi=%d\n",
                    sensor_route.location ? 16 : 14, sensor_route.location ? 17 : 15,
                    sensor_route.address, *temperature, *humidity);
    } else {
        DBG_vPrintf(TRUE, "SENSOR no CRC-valid measurement; inspect command/stage/CRC diagnostics\n");
    }
    return valid;
}

PUBLIC void APP_vSensorProbe(void)
{
    unsigned sample, valid = 0;
    int16_t temperature;
    uint16_t humidity;
    uint16 before = 0xffff, after = 0xffff;
    bool_t clear_sent, before_ok, after_ok, baseline_clear;
    DBG_vPrintf(TRUE, "SENSOR candidate v6: low repeatability 0x2416, 100kHz, report delta 0.1C\n");
    diagnostic_registers(SENSOR_DEFAULT_PRESCALER);
    clear_sent = command_at(0, 0x44, SENSOR_DEFAULT_PRESCALER, 0x3041,
                             1000UL, NULL, 0, "clear-status");
    before_ok = diagnostic_status("before-normal-reads", &before);
    baseline_clear = clear_sent && before_ok && !(before & 0x10);
    /* Use the same low-repeatability transport as joined sampling. No reset,
     * clear-status or register reads between these three normal conversions. */
    for (sample = 1; sample <= 3; ++sample) {
        vAHI_WatchdogRestart();
        DBG_vPrintf(TRUE, "SENSOR verification sample=%d/3 mode=low hz=100000\n", sample);
        if (read_sensor(&temperature, &humidity)) ++valid;
        if (!wait_sensor_ticks(16000UL)) break;
    }
    after_ok = diagnostic_status("after-normal-reads", &after);
    DBG_vPrintf(TRUE, "SENSOR verification valid=%d/3 baseline_clear=%d status_after_valid=%d reset_after=%d\n",
                valid, baseline_clear, after_ok, after_ok ? !!(after & 0x10) : -1);
}

PUBLIC bool_t APP_bSensorReportAttribute(uint16 cluster, uint16 attribute)
{
    teZCL_Status status;
    tsZCL_Address address;
    PDUM_thAPduInstance pdu = hZCL_AllocateAPduInstance();
    if (pdu == PDUM_INVALID_HANDLE) return FALSE;
    memset(&address, 0, sizeof(address));
    address.eAddressMode = E_ZCL_AM_SHORT;
    address.uAddress.u16DestinationAddress = 0x0000;
    status = eZCL_ReportAttribute(&address, cluster, attribute, 1, 1, pdu);
    /* The SDK copies on success and frees on transmission failure itself. */
    if (status != E_ZCL_ERR_ZTRANSMIT_FAIL) PDUM_eAPduFreeAPduInstance(pdu);
    DBG_vPrintf(TRUE, "SENSOR report cluster=%04x attribute=%04x status=%d\n", cluster, attribute, status);
    return status == E_ZCL_SUCCESS;
}

PUBLIC void APP_vSensorInvalidate(void)
{
    have_t = have_h = FALSE; /* Any failed APS confirm forces retry next wake. */
}

PUBLIC void APP_vSensorSample(void)
{
    APP_vSensorSampleRequested(FALSE, TRUE);
}

PUBLIC void APP_vSensorSampleRequested(bool_t force_report, bool_t scheduled_sample)
{
    int16_t temperature;
    uint16_t humidity;
    tsZCL_ClusterInstance *t, *h;
    /* Manual presses and join are fresh reads, not extra 30-second cycles. */
    if (scheduled_sample) {
        if (age_t < 3600) age_t += SAMPLE_SECONDS;
        if (age_h < 3600) age_h += SAMPLE_SECONDS;
    }
    if (!read_sensor(&temperature, &humidity)) {
        /* Never publish fabricated, stale or CRC-invalid measurements. */
        DBG_vPrintf(TRUE, "SENSOR I2C/CRC failure; sample discarded\n");
        return;
    }
    if (eZCL_SearchForClusterEntry(1, 0x0402, TRUE, &t) != E_ZCL_SUCCESS ||
        eZCL_SearchForClusterEntry(1, 0x0405, TRUE, &h) != E_ZCL_SUCCESS) return;
    ((tsCLD_TemperatureMeasurement *)t->pvEndPointSharedStructPtr)->i16MeasuredValue = temperature;
    ((tsCLD_RelativeHumidityMeasurement *)h->pvEndPointSharedStructPtr)->u16MeasuredValue = humidity;
    DBG_vPrintf(TRUE, "SENSOR temperature_centi=%d humidity_centi=%d\n", temperature, humidity);
    if ((force_report || report_due(temperature, previous_t, SENSOR_TEMPERATURE_DELTA_CENTI, age_t, SENSOR_TEMPERATURE_MAX_SECONDS, have_t)) &&
        APP_bSensorReportAttribute(0x0402, 0)) {
        previous_t = temperature; have_t = TRUE; age_t = 0;
    }
    if ((force_report || report_due(humidity, previous_h, 100, age_h, SENSOR_HUMIDITY_MAX_SECONDS, have_h)) &&
        APP_bSensorReportAttribute(0x0405, 0)) {
        previous_h = humidity; have_h = TRUE; age_h = 0;
    }
}
