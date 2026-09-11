#include "window_checker.h"
#include "./__common.h"

#undef this
#define this (*ptThis)

ymodem_internal_win_checker_t * ymodem_internal_win_checker_init (ymodem_internal_win_checker_t *ptThis, ymodem_internal_win_checker_cfg_t *ptCFG) 
{
    assert (NULL != ptThis);
    assert (NULL != ptCFG);

    memset (ptThis, 0, sizeof(ymodem_internal_win_checker_t));

    this.tCFG = *ptCFG;
    
    return ptThis;
}


#define WIN_CHECKER_RESET_FSM() \
    do {this.chState = START;} while(0)

ymodem_fsm_rt_t ymodem_internal_win_checker (ymodem_internal_win_checker_t *ptThis) 
{
    assert (NULL != ptThis);
    enum {
        START = 0,
        CHECK_ITEM,
        IS_END,
        CHECK_WAIT,
        DATA_OUT,
    };

    switch (this.chState) {
        case START:
            this.bWait = false;
            this.ptCurrent = this.tCFG.ptList;
            /* fall through */

        case CHECK_ITEM:
            do {
                RESTART:;
                ymodem_internal_win_checker_rt_t emRet = YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;

                assert(NULL != this.ptCurrent);
                assert(NULL != this.ptCurrent->fnHandler);

                emRet = this.ptCurrent->fnHandler(this.ptCurrent, this.tCFG.ptFIFO);
                if (YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV == emRet) {
                    this.chState = IS_END;
                } else if (YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA == emRet) {
                    this.bWait = true;
                    this.chState = IS_END;
                } else if (YMODEM_INTERNAL_WIN_CHK_RT_CPL == emRet) {
                    ymodem_internal_queue_drop_all_peeked(this.tCFG.ptFIFO);
                    WIN_CHECKER_RESET_FSM();
                    return ymodem_fsm_rt_cpl;
                } else if (YMODEM_INTERNAL_WIN_CHK_RT_YIELD == emRet) {
                    break;
                }
            } while(0);
            break;

        case IS_END:
            assert(NULL != this.ptCurrent);
            ymodem_internal_queue_reset_peek(this.tCFG.ptFIFO);
            if (NULL == this.ptCurrent->ptNext) {
                this.chState = CHECK_WAIT;
            } else {
                this.ptCurrent = this.ptCurrent->ptNext;
                this.chState = CHECK_ITEM;
                goto RESTART;
            }
            break;

        case CHECK_WAIT:
            if (false == this.bWait) {
                ymodem_queue_read_byte(this.tCFG.ptFIFO, &this.chByte);
                this.chState = DATA_OUT;
                break;
            }
            WIN_CHECKER_RESET_FSM();
            break;

        case DATA_OUT:
            if (NULL != this.tCFG.evtOnDataOut.fnHandler) {
                ymodem_fsm_rt_t emRet = this.tCFG.evtOnDataOut.fnHandler(this.tCFG.evtOnDataOut.pObj, this.chByte);
                if (ymodem_fsm_rt_on_going == emRet) {
                    break;
                }
            }
            WIN_CHECKER_RESET_FSM();
            break;

    }

    return ymodem_fsm_rt_on_going;
}

bool ymodem_internal_win_checker_register(  ymodem_internal_win_checker_t *ptThis, 
                            ymodem_internal_win_checker_item_t *ptItem)
{
    assert (NULL != ptThis);
    assert (NULL != ptItem);

    if (NULL == ptThis->tCFG.ptList) {
        ptThis->tCFG.ptList = ptItem;
        return true;
    }
    ymodem_internal_win_checker_item_t *ptCurrent;
    ptCurrent = ptThis->tCFG.ptList;
    while (ptCurrent != NULL) {
        if (ptCurrent == ptItem) {
            return false;
        }
        ptCurrent = ptCurrent->ptNext;
    }
    ptItem->ptNext = this.tCFG.ptList->ptNext;
    this.tCFG.ptList->ptNext = ptItem;
    return true;
}
