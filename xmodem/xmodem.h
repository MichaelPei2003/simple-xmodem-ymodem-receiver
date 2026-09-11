#ifndef YMODEM_HELPER_XMODEM_H
#define YMODEM_HELPER_XMODEM_H

#include "../app_cfg.h"
#include "ymodem_helper_types.h"
#include "window_checker/window_checker.h"
#include "fsm_event/fsm_event.h"

typedef struct ymodem_internal_xmodem_report_evt_t {
    ymodem_report_handler_t *fnHandler;  //user register count data received and validate
    void *pObj;
} ymodem_internal_xmodem_report_evt_t;

typedef struct ymodem_internal_xmodem_cfg_t {
    ymodem_internal_xmodem_report_evt_t evtReport;
    ymodem_queue_t       *ptFIFOOut;
} ymodem_internal_xmodem_cfg_t;

typedef struct ymodem_internal_xmodem_t {
    ymodem_internal_win_checker_item_t use_as_ymodem_internal_win_checker_item_t;
    uint8_t         chGetFrameState;
    uint8_t         chTaskState;
    uint8_t         chPrepareState;
    uint8_t         chCheckerState;
    ymodem_internal_xmodem_cfg_t    tCFG;
    uint32_t        wDelayCnt;      //timer
    uint8_t         chSeq;          //sequence
    size_t          uSize;          //size of data(128)
    uint8_t         pchFrame[1034]; //full frame
    size_t          uIndex;         //index in current frame
    bool            bBusy;          //is receiving 
    bool            bErrCnt;        //if NACK sent
    uint16_t        hwCheckSum;     //crc value,  
    uint8_t         chSignal;       //Start signal
    ymodem_internal_event_t     tTaskActive;
    bool            bIsFirstFrame;
    bool            bIsYmodem;
    bool            bComplete;
} ymodem_internal_xmodem_t;

ymodem_internal_xmodem_t *ymodem_internal_xmodem_init(ymodem_internal_xmodem_t *ptThis, ymodem_internal_xmodem_cfg_t *ptCFG);

ymodem_internal_xmodem_t *ymodem_internal_xmodem_start_receive(ymodem_internal_xmodem_t *ptThis);


ymodem_fsm_rt_t ymodem_internal_xmodem_task(ymodem_internal_xmodem_t *ptThis);

/* Composition accesses the embedded checker through this interface. */
ymodem_internal_win_checker_item_t *ymodem_internal_xmodem_item(
    ymodem_internal_xmodem_t *ptThis);

#endif /* YMODEM_HELPER_XMODEM_H */
