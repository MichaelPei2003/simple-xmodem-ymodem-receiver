#ifndef YMODEM_HELPER_FSM_EVENT_H
#define YMODEM_HELPER_FSM_EVENT_H

#include "../app_cfg.h"

#include <stdbool.h>

typedef struct ymodem_internal_event_t {
    bool bIsSet;
    bool bIsAutoReset;
} ymodem_internal_event_t;

void ymodem_internal_event_init(
    ymodem_internal_event_t *ptThis,
    bool bIsSet,
    bool bIsManual);
void ymodem_internal_event_set(ymodem_internal_event_t *ptThis);
bool ymodem_internal_event_wait(ymodem_internal_event_t *ptThis);

#endif /* YMODEM_HELPER_FSM_EVENT_H */
