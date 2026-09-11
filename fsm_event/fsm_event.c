#include "fsm_event.h"
#include "./__common.h"

#undef this
#define this (*ptThis)

void ymodem_internal_event_init(
    ymodem_internal_event_t *ptThis,
    bool bIsSet,
    bool bIsManual)
{
    assert(NULL != ptThis);
    this.bIsSet = bIsSet;
    this.bIsAutoReset = !bIsManual;
}

void ymodem_internal_event_set(ymodem_internal_event_t *ptThis)
{
    assert(NULL != ptThis);
    this.bIsSet = true;
}

static void ymodem_internal_event_reset(ymodem_internal_event_t *ptThis)
{
    assert(NULL != ptThis);
    this.bIsSet = false;
}

bool ymodem_internal_event_wait(ymodem_internal_event_t *ptThis)
{
    assert(NULL != ptThis);

    if (this.bIsSet) {
        if (this.bIsAutoReset) {
            ymodem_internal_event_reset(ptThis);
        }
        return true;
    }

    return false;
}
