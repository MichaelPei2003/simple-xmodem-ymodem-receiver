#ifndef YMODEM_HELPER_H
#define YMODEM_HELPER_H

#include "./app_cfg.h"
#include "./ymodem_helper_types.h"
#include "./queue/queue.h"
#include "./window_checker/window_checker.h"
#include "./xmodem/xmodem.h"

typedef struct ymodem_helper_cfg_t {
    ymodem_queue_t *ptIn;                /* Incoming protocol bytes. */
    ymodem_queue_t *ptOut;               /* Outgoing protocol replies. */
    ymodem_report_handler_t *fnHandler;  /* Optional report callback. */
    void *pObj;                         /* User callback context. */
} ymodem_helper_cfg_t;

/*
 * Caller-owned storage. Treat all members as private; use the functions below.
 * Child types are included only to provide a complete allocation layout.
 * An initialized instance must not be copied or moved.
 */
typedef struct ymodem_t {
    ymodem_internal_win_checker_t tWindowChecker;
    ymodem_internal_xmodem_t tXmodem;
    ymodem_helper_cfg_t tCFG;
} ymodem_t;

/* Copies the configuration and starts reception. Queues must remain valid. */
ymodem_t *ymodem_helper_init(
    ymodem_t *ptThis,
    ymodem_helper_cfg_t *ptCFG);

/* Poll from the main loop. cpl is a task result, not a transfer-end report. */
ymodem_fsm_rt_t ymodem_helper_task(ymodem_t *ptThis);

#endif /* YMODEM_HELPER_H */
