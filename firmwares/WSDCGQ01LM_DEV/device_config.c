#include <jendefs.h>
#include "zps_gen.h"
#include "device_config.h"
PRIVATE const tsEndpointButtonConfig buttons[1] = {{0}};
const tsDeviceConfig sDeviceConfig = {
 .u8BasicEndpoint=1, .u8ZdoEndpoint=0,
 .sDioConfig={0,0}, .sResetButtonConfig={0,0,FALSE}, .psEndpointButtons=buttons
};
