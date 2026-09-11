#ifndef YMODEM_HELPER_TYPES_H
#define YMODEM_HELPER_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Shared result and report types; child headers do not include the parent API. */
typedef enum ymodem_fsm_rt_t {
    ymodem_fsm_rt_err = -1,
    ymodem_fsm_rt_on_going = 0,
    ymodem_fsm_rt_cpl = 1,
    ymodem_fsm_rt_user,
} ymodem_fsm_rt_t;

typedef enum ymodem_report_t {
    YMODEM_START,
    YMODEM_NEW_FRAME,
    YMODEM_TIME_OUT,
    YMODEM_COMPLETE,
    YMODEM_CANCELLED,
    YMODEM_ERROR,
} ymodem_report_t;

/* Data belongs to the helper and is valid only during the callback. */
typedef void ymodem_report_handler_t(
    void *pObj,
    ymodem_report_t tReport,
    uint8_t *pchData,
    size_t uSize);

#endif /* YMODEM_HELPER_TYPES_H */
