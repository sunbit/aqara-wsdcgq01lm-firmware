#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef DEBUG_MAIN
#define TRACE_MAIN TRUE
#else
#define TRACE_MAIN FALSE
#endif

#include <jendefs.h>

PUBLIC void APP_vOnWakeTimer1(void);

#endif /* APP_MAIN_H */
