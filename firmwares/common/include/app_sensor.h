#ifndef APP_SENSOR_H
#define APP_SENSOR_H
#include <jendefs.h>
PUBLIC void APP_vSensorSample(void);
PUBLIC void APP_vSensorSampleRequested(bool_t force_report, bool_t scheduled_sample);
PUBLIC bool_t APP_bSensorReportAttribute(uint16 cluster, uint16 attribute);
PUBLIC void APP_vSensorInvalidate(void);
PUBLIC void APP_vSensorElapsed(uint32 seconds);
PUBLIC void APP_vSensorBatteryReport(bool_t force);
PUBLIC void APP_vSensorProbe(void);
#endif
