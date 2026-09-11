#ifndef YMODEM_HELPER_WINDOW_CHECKER_H
#define YMODEM_HELPER_WINDOW_CHECKER_H

#include "../app_cfg.h"
#include "ymodem_helper_types.h"
#include "queue/queue.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA = ymodem_fsm_rt_on_going,
    YMODEM_INTERNAL_WIN_CHK_RT_CPL = ymodem_fsm_rt_cpl,
    YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV = ymodem_fsm_rt_user,
    YMODEM_INTERNAL_WIN_CHK_RT_YIELD,
    YMODEM_INTERNAL_WIN_CHK_RT_USER,
} ymodem_internal_win_checker_rt_t;

typedef struct ymodem_internal_win_checker_item_t ymodem_internal_win_checker_item_t;

typedef
ymodem_internal_win_checker_rt_t ymodem_internal_win_checker_item_fsm_t(ymodem_internal_win_checker_item_t *ptThis,
                                        ymodem_queue_t *ptQueue);

typedef
ymodem_fsm_rt_t ymodem_internal_win_checker_data_out_t(  void *pObj,
                                    uint8_t chByte);

typedef struct ymodem_internal_win_checker_data_out_evt_t {
    ymodem_internal_win_checker_data_out_t *fnHandler;
    void *pObj;
} ymodem_internal_win_checker_data_out_evt_t;

struct ymodem_internal_win_checker_item_t {
    ymodem_internal_win_checker_item_t      *ptNext;
    ymodem_internal_win_checker_item_fsm_t  *fnHandler;
};


typedef struct ymodem_internal_win_checker_cfg_t {
    ymodem_internal_win_checker_item_t          *ptList;
    ymodem_queue_t                *ptFIFO;
    ymodem_internal_win_checker_data_out_evt_t   evtOnDataOut;
} ymodem_internal_win_checker_cfg_t;

typedef struct ymodem_internal_win_checker_t {
    uint8_t                 chState;
    ymodem_internal_win_checker_cfg_t       tCFG;
    uint8_t                 chByte;
    bool                    bWait;
    ymodem_internal_win_checker_item_t     *ptCurrent;
} ymodem_internal_win_checker_t;

ymodem_internal_win_checker_t *ymodem_internal_win_checker_init             (ymodem_internal_win_checker_t *ptThis,
                                             ymodem_internal_win_checker_cfg_t *ptCFG);
ymodem_fsm_rt_t ymodem_internal_win_checker                        (ymodem_internal_win_checker_t *ptThis);
bool ymodem_internal_win_checker_register                   (ymodem_internal_win_checker_t *ptThis,
                                             ymodem_internal_win_checker_item_t *ptItem);

#endif /* YMODEM_HELPER_WINDOW_CHECKER_H */
