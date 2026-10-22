#include "sensor_reporting.h"
void sensor_report_elapsed(sensor_report_state *s, uint32_t seconds)
{
    /* Configuration allows at most one day. Saturate without overflow. */
    s->age = seconds >= 86400 || s->age >= 86400 - seconds ? 86400 : s->age + seconds;
}
int sensor_report_due(const sensor_report_state *s, int32_t value,
                      uint16_t delta, uint16_t minimum, uint32_t maximum, int force)
{
    int32_t change = value - s->previous;
    if (change < 0) change = -change;
    /* A heartbeat and manual refresh take precedence over the rate limit. */
    return force || !s->have_previous || s->age >= maximum ||
        (s->age >= minimum && change >= delta);
}
void sensor_report_sent(sensor_report_state *s, int32_t value)
{
    s->previous = value;
    s->have_previous = 1;
    s->age = 0;
}
