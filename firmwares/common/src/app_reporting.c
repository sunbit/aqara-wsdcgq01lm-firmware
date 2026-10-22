#include "zcl.h"
#include "app_reporting.h"

PUBLIC void vMakeSupportedAttributesReportable(void)
{
    /* The application owns elapsed-time reporting, including time asleep.
     * SDK report timers only run while awake and must not create an additional
     * fixed-rate battery reporting path beside the configurable policy. */
    eZCL_SetReportableFlag(1, 0x0402, TRUE, FALSE, 0);
    eZCL_SetReportableFlag(1, 0x0405, TRUE, FALSE, 0);
    eZCL_SetReportableFlag(1, 0x0001, TRUE, FALSE, 0x0020);
    eZCL_SetReportableFlag(1, 0x0001, TRUE, FALSE, 0x0021);
}
