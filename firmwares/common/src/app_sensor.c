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
#include "sensor_bus_profile.h"
#include "sensor_reporting.h"
#include "ConfigurationCluster.h"
#include "app_battery_fresh.h"
#include "app_sensor_clock.h"
#include "dbg.h"
#include "diagnostic_suite.h" /* Diagnostic transport flags; normal calls use zero. */
#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
#include "sensor_gpio_i2c.h"
#endif
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
#include "app_diagnostic_suite.h"
#include "app_sensor_led.h"
PRIVATE uint32 diagnostic_idle;
#endif
#ifdef SENSOR_LED_DIAGNOSTICS
#include "sensor_diagnostic.h"
PRIVATE unsigned diagnostic_result = SENSOR_DIAG_TRANSPORT;
PUBLIC unsigned APP_uSensorDiagnosticResult(void) { return diagnostic_result; }
#endif

#define BUS_TIMEOUT_LOOPS 500000UL /* Bounded polling, including SHT clock stretching. */
#define SENSOR_DEFAULT_PRESCALER SENSOR_BUS_PRESCALER
#define SENSOR_MEASUREMENT_COMMAND 0x2416 /* SHT3x low-repeatability single shot. */

PRIVATE sensor_report_state report_t, report_h, report_b;
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
     * a conservative guard, not a confirmed root-cause correction. */
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

#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
PRIVATE void gpio_low(unsigned line, int enabled, void *context)
{
    uint32 mask = 1UL << (14 + line);
    (void)context;
    vAHI_DioSetDirection(enabled ? 0 : mask, enabled ? mask : 0);
}
PRIVATE int gpio_level(unsigned line, void *context)
{
    (void)context;
    return !!(u32AHI_DioReadInput() & (1UL << (14 + line)));
}
PRIVATE const sht_gpio_io gpio_io = {gpio_low, gpio_level, wire_wait_us, wire_stage};
#endif

PRIVATE bool_t command_at_configured(unsigned location, uint8 address, uint8 prescaler,
                          uint16 command, uint32 wait_us, uint8 *data,
                          unsigned length, const char *label,
                          uint32 settle_us, bool_t pullups, uint8 diagnostic_flags)
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
    vAHI_DioSetPullup(pullups ? pins : 0, pullups ? 0 : pins);
#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
    if (diagnostic_flags & DIAG_FLAG_GPIO) {
        /* The software transport is restricted to the confirmed DIO14/15
         * route. Output latches stay LOW; HIGH is always input/release. */
        if (location != 0 || !prescaler) return FALSE;
        vAHI_DioSetOutput(0, pins);
    } else
#endif
    {
        vAHI_SiMasterConfigure(!(diagnostic_flags & DIAG_FLAG_NO_FILTER), FALSE, prescaler);
        vAHI_SiSetLocation(location != 0);
    }
    sensor_stage = "bus-settle";
    sensor_fault = "none";
    sensor_byte = 0;
    received = wait_sensor_ticks(settle_us * 16UL);
    idle_levels = u32AHI_DioReadInput();
#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
    diagnostic_idle=idle_levels;
#endif
    if (received) {
#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
        if (diagnostic_flags & DIAG_FLAG_GPIO) {
            static const char *const faults[] = {"none","nack","tick-timeout",
                "transfer-timeout","arbitration-lost","invalid-command","bus-stuck"};
            sht_gpio_bus bus = {&gpio_io, NULL, (uint32)prescaler * 10UL, 0, 0, 0};
            received = sht_gpio_command_read(&bus, address, command, wait_us, data, length);
            sensor_fault = bus.fault < 7 ? faults[bus.fault] : "invalid-command";
        } else
#endif
            received = sht_command_read(&sensor_io, NULL, address,
                                          command, wait_us, data, length);
    }
    if (!received && !(diagnostic_flags & DIAG_FLAG_GPIO)) {
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
    DBG_vPrintf(TRUE, "SENSOR probe label=%s SCL=DIO%d SDA=DIO%d addr=0x%02x hz=%d cmd=0x%04x received=%d gpio=%d\n",
                label, location ? 16 : 14, location ? 17 : 15, address,
                (diagnostic_flags & DIAG_FLAG_GPIO) ? 50000UL / prescaler : 3200000UL / ((uint32)prescaler + 1),
                command, received, !!(diagnostic_flags & DIAG_FLAG_GPIO));
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

PRIVATE bool_t command_at(unsigned location, uint8 address, uint8 prescaler,
                          uint16 command, uint32 wait_us, uint8 *data,
                          unsigned length, const char *label)
{
    return command_at_configured(location,address,prescaler,command,wait_us,data,
        length,label,SENSOR_BUS_SETTLE_US,TRUE,0);
}

#ifdef SENSOR_PERSISTENT_DIAGNOSTICS
PRIVATE unsigned diagnostic_name(const char *name, const char *const *names, unsigned count)
{
    unsigned n;
    for (n=0;n<count;++n) if (strcmp(name,names[n])==0) return n;
    return 255;
}
PRIVATE bool_t diagnostic_clear_bus(void)
{
    const uint32 scl=1UL<<14, sda=1UL<<15;
    bool_t ok=TRUE;
    unsigned n;
    vAHI_SiMasterDisable();
    vAHI_DioSetDirection(scl|sda,0); vAHI_DioSetPullup(scl|sda,0);
    vAHI_DioSetOutput(0,scl); /* Preload LOW; HIGH is input/release only. */
    for (n=0;n<9;++n) {
        vAHI_DioSetDirection(0,scl);
        if (!wait_sensor_ticks(800)) {ok=FALSE; break;}
        vAHI_DioSetDirection(scl,0);
        if (!wait_sensor_ticks(800) || !(u32AHI_DioReadInput()&scl)) {ok=FALSE; break;}
    }
    vAHI_DioSetDirection(scl|sda,0); vAHI_DioSetPullup(0,scl|sda);
    return ok;
}
PUBLIC void APP_vSensorDiagnosticCapture(const diag_case *spec, uint8 row[DIAG_ROW_BYTES])
{
    static const char *const stages[]={"bus-settle","write-address","command-msb",
        "command-lsb","command-wait","read-address","read-data","complete"};
    static const char *const faults[]={"none","nack","tick-timeout",
        "transfer-timeout","arbitration-lost","invalid-command","bus-stuck"};
    uint32 mv=0, pins=spec->location ? (3UL<<16) : (3UL<<14);
    uint32 settle_us=(uint32)spec->settle_units * ((spec->flags&DIAG_FLAG_LONG_SETTLE) ? 1000UL : 100UL);
    uint8 data[6];
    bool_t received, control_ok=TRUE, voltage_ok;
    vAHI_WatchdogRestart();
    memset(row,0,DIAG_ROW_BYTES); memset(data,0xff,sizeof(data));
    vAHI_DioSetOutput((spec->flags&DIAG_FLAG_LED) ? 0 : SENSOR_LED_MASK,
                      (spec->flags&DIAG_FLAG_LED) ? SENSOR_LED_MASK : 0);
    (void)wait_sensor_ticks(16000); /* Same 1 ms load-settle for every case. */
    voltage_ok=APP_bBatteryMeasureMillivolts(&mv);
    if (spec->flags&DIAG_FLAG_CLEAR_BUS) control_ok=diagnostic_clear_bus();
    if (spec->flags&DIAG_FLAG_SOFT_RESET)
        control_ok=command_at_configured(spec->location,spec->address,spec->prescaler,
            0x30a2,2000,NULL,0,"suite-soft-reset",settle_us,TRUE,spec->flags);
    received=command_at_configured(spec->location,spec->address,spec->prescaler,
        spec->command,20000,data,6,"suite-measurement",settle_us,
        !(spec->flags&DIAG_FLAG_NO_PULLUP),spec->flags);
    row[1]=(uint8)diagnostic_name(sensor_stage,stages,sizeof(stages)/sizeof(stages[0]));
    row[2]=(uint8)diagnostic_name(sensor_fault,faults,sizeof(faults)/sizeof(faults[0]));
    row[3]=(received ? 1 : 0) | (received && data[2]==sht_crc(data,2) ? 2 : 0) |
        (received && data[5]==sht_crc(data+3,2) ? 4 : 0) |
        (diagnostic_idle & (spec->location ? 1UL<<16 : 1UL<<14) ? 8 : 0) |
        (diagnostic_idle & (spec->location ? 1UL<<17 : 1UL<<15) ? 16 : 0) |
        (control_ok ? 32 : 0) | (voltage_ok ? 64 : 0) | (spec->flags&DIAG_FLAG_LED ? 128 : 0);
    row[4]=(uint8)mv; row[5]=(uint8)(mv>>8); memcpy(row+6,data,6);
    row[12]=spec->flags | (spec->location ? 128 : 0);
    row[13]=spec->settle_units; row[14]=spec->prescaler; row[15]=spec->address;
    vAHI_SiMasterDisable(); vAHI_DioSetDirection(pins,0); vAHI_DioSetPullup(0,pins);
    vAHI_DioSetOutput(SENSOR_LED_MASK,0);
}
#endif

PRIVATE int read_at(unsigned location, uint8_t address, uint8_t data[6], void *context)
{
    int received;
    (void)context;
    received = command_at(location, address, SENSOR_DEFAULT_PRESCALER,
                      SENSOR_MEASUREMENT_COMMAND, 20000UL, data, 6, "measurement-low");
#ifdef SENSOR_LED_DIAGNOSTICS
    if (location == 0 && address == 0x44)
        diagnostic_result = sensor_diagnostic_transport(received,
            received && data[2] == sht_crc(data, 2) && data[5] == sht_crc(data + 3, 2),
            sensor_stage, sensor_fault);
#endif
    return received;
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
#ifdef SENSOR_LED_DIAGNOSTICS
    diagnostic_result = SENSOR_DIAG_TRANSPORT;
#endif
    bool_t valid = sht_probe(&sensor_route, read_at, NULL, temperature, humidity);
#ifdef SENSOR_LED_DIAGNOSTICS
    if (valid) diagnostic_result = SENSOR_DIAG_OK;
#endif
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
    DBG_vPrintf(TRUE, "SENSOR: low repeatability 0x2416, 100kHz, delta_centi=%d sample_seconds=%d\n",
                APP_psSensorConfig()->temperature_delta, APP_psSensorConfig()->sample_seconds);
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
    report_t.have_previous = report_h.have_previous = report_b.have_previous = FALSE;
}

PUBLIC void APP_vSensorElapsed(uint32 seconds)
{
    sensor_report_elapsed(&report_t, seconds);
    sensor_report_elapsed(&report_h, seconds);
    sensor_report_elapsed(&report_b, seconds);
}

PUBLIC void APP_vSensorBatteryReport(bool_t force)
{
    APP_vSensorClockSync();
    if ((force || !report_b.have_previous || report_b.age >= APP_psSensorConfig()->battery_seconds) &&
        APP_bBatteryReportFresh()) sensor_report_sent(&report_b, 0);
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
    const sensor_config *config = APP_psSensorConfig();
    /* Synchronize before every read, including button/configuration requests,
     * so a press just before a timer wake cannot overcount the report age. */
    APP_vSensorClockSync();
    (void)scheduled_sample;
    if (!read_sensor(&temperature, &humidity)) {
        /* Never publish fabricated, stale or CRC-invalid measurements. */
        DBG_vPrintf(TRUE, "SENSOR I2C/CRC failure; sample discarded\n");
        return;
    }
    temperature = sensor_calibrate_temperature(temperature, config->temperature_offset);
    humidity = sensor_calibrate_humidity(humidity, config->humidity_offset);
    if (eZCL_SearchForClusterEntry(1, 0x0402, TRUE, &t) != E_ZCL_SUCCESS ||
        eZCL_SearchForClusterEntry(1, 0x0405, TRUE, &h) != E_ZCL_SUCCESS) {
#ifdef SENSOR_LED_DIAGNOSTICS
        diagnostic_result = SENSOR_DIAG_CLUSTER;
#endif
        return;
    }
    ((tsCLD_TemperatureMeasurement *)t->pvEndPointSharedStructPtr)->i16MeasuredValue = temperature;
    ((tsCLD_RelativeHumidityMeasurement *)h->pvEndPointSharedStructPtr)->u16MeasuredValue = humidity;
    DBG_vPrintf(TRUE, "SENSOR temperature_centi=%d humidity_centi=%d\n", temperature, humidity);
    if (sensor_report_due(&report_t, temperature, config->temperature_delta,
                         config->temperature_min_seconds, config->temperature_max_seconds, force_report)) {
        if (APP_bSensorReportAttribute(0x0402, 0)) sensor_report_sent(&report_t, temperature);
#ifdef SENSOR_LED_DIAGNOSTICS
        else diagnostic_result = SENSOR_DIAG_REPORT;
#endif
    }
    if (sensor_report_due(&report_h, humidity, config->humidity_delta,
                         config->humidity_min_seconds, config->humidity_max_seconds, force_report)) {
        if (APP_bSensorReportAttribute(0x0405, 0)) sensor_report_sent(&report_h, humidity);
#ifdef SENSOR_LED_DIAGNOSTICS
        else diagnostic_result = SENSOR_DIAG_REPORT;
#endif
    }
}
