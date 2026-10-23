#ifndef APP_DIAGNOSTIC_SUITE_H
#define APP_DIAGNOSTIC_SUITE_H
#include <jendefs.h>
#include "zcl.h"
#include "diagnostic_suite.h"
#define DIAG_CLUSTER_ID 0xfc01
#define DIAG_PDM_ID 0xB
typedef struct {tsZCL_OctetString packet;} diagnostic_cluster_data;
PUBLIC void APP_vDiagnosticCluster(tsZCL_ClusterInstance *instance, diagnostic_cluster_data *data);
PUBLIC void APP_vDiagnosticInit(void);
PUBLIC void APP_vDiagnosticRequest(void);
PUBLIC void APP_vDiagnosticWake(void);
PUBLIC void APP_vDiagnosticProcess(void);
PUBLIC bool_t APP_bDiagnosticBusy(void);
PUBLIC void APP_cbTimerDiagnostic(void *context);
PUBLIC void APP_vSensorDiagnosticCapture(const diag_case *spec, uint8 row[DIAG_ROW_BYTES]);
#endif
