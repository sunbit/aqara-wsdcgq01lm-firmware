#include <assert.h>
#include <stdint.h>
#include "sensor_config.h"
#include "sensor_reporting.h"
int main(void) {
    sensor_config c; sensor_config_defaults(&c);assert(sensor_config_valid(&c));
    assert(c.sample_seconds==60&&c.temperature_delta==10&&c.temperature_max_seconds==300&&c.humidity_max_seconds==600&&c.battery_seconds==3600);
    assert(sensor_calibrate_temperature(2050,-125)==1925);
    assert(sensor_calibrate_temperature(12490,100)==12500);
    assert(sensor_calibrate_temperature(-3990,-100)==-4000);
    assert(sensor_calibrate_humidity(100,-250)==0);
    assert(sensor_calibrate_humidity(9900,250)==10000);
    assert(sensor_calibrate_humidity(5000,-250)==4750);
    sensor_report_state s={0};
    assert(sensor_report_due(&s,2000,10,0,300,0));sensor_report_sent(&s,2000);
    sensor_report_elapsed(&s,60);assert(!sensor_report_due(&s,2009,10,0,300,0));
    assert(sensor_report_due(&s,2010,10,0,300,0));assert(sensor_report_due(&s,1990,10,0,300,0));
    assert(!sensor_report_due(&s,2020,10,120,300,0)); /* Minimum interval. */
    sensor_report_elapsed(&s,60);assert(sensor_report_due(&s,2020,10,120,300,0));
    sensor_report_sent(&s,2020);sensor_report_elapsed(&s,300);
    assert(sensor_report_due(&s,2020,10,600,300,0)); /* Heartbeat takes precedence. */
    sensor_report_sent(&s,2020);assert(sensor_report_due(&s,2020,10,600,300,1));
    /* Failed submissions do not reset age or the comparison baseline. */
    sensor_report_elapsed(&s,300);assert(sensor_report_due(&s,2020,10,0,300,0));
    sensor_report_elapsed(&s,60);assert(s.age==360&&s.previous==2020);
    sensor_report_elapsed(&s,UINT32_MAX);assert(s.age==86400);
    sensor_report_elapsed(&s,1);assert(s.age==86400);
    c.temperature_delta=0;assert(!sensor_config_valid(&c));
    return 0;
}
