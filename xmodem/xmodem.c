#include "xmodem.h"
#include "./__common.h"

#undef this
#define this (*ptThis)

#define SOH128B 0x01u
#define SOH1K 0x02u
#define ACK 0x06u
#define NACK 0x15u
#define EOT 0x04u
#define CAN 0x18u
#define DELAY_COUNT YMODEM_HELPER_CFG_RETRY_POLLS

typedef enum {
    YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA = YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA,
    YMODEM_INTERNAL_XMODEM_RT_GET_FRAME = YMODEM_INTERNAL_WIN_CHK_RT_CPL,
    YMODEM_INTERNAL_XMODEM_RT_REQ_MOV   = YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV,
    YMODEM_INTERNAL_XMODEM_RT_YIELD     = YMODEM_INTERNAL_WIN_CHK_RT_YIELD,
    YMODEM_INTERNAL_XMODEM_RT_EOT,
    YMODEM_INTERNAL_XMODEM_RT_CANCELLED,
    YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR,
} ymodem_internal_xmodem_fsm_rt_t;

static uint16_t update_crc(uint8_t *pchData, uint16_t hwCRC);
static ymodem_internal_win_checker_rt_t xmodem_checker(ymodem_internal_win_checker_item_t *ptItem, ymodem_queue_t *ptQueue);

ymodem_internal_xmodem_t *ymodem_internal_xmodem_init(ymodem_internal_xmodem_t *ptThis, ymodem_internal_xmodem_cfg_t *ptCFG) 
{
    assert(NULL != ptThis);
    assert(NULL != ptCFG);
    assert(NULL != ptCFG->evtReport.fnHandler);

    memset(ptThis, 0, sizeof(ymodem_internal_xmodem_t));

    this.tCFG = *ptCFG;
    this.uSize = 128;
    this.use_as_ymodem_internal_win_checker_item_t.fnHandler = &xmodem_checker;
    ymodem_internal_event_init(&(this.tTaskActive), false, false);
    return ptThis;
}

ymodem_internal_win_checker_item_t *ymodem_internal_xmodem_item(
    ymodem_internal_xmodem_t *ptThis)
{
    assert(NULL != ptThis);
    return &this.use_as_ymodem_internal_win_checker_item_t;
}

ymodem_internal_xmodem_t *ymodem_internal_xmodem_start_receive(ymodem_internal_xmodem_t *ptThis)
{
    assert(NULL != ptThis);
    this.bBusy = true;
    this.bComplete = false;
    return ptThis;
}

#define XMODEM_GET_FRAME_RESET_FSM() \
    do {this.chGetFrameState = START;} while(0)

static ymodem_internal_xmodem_fsm_rt_t xmodem_get_frame(ymodem_internal_xmodem_t *ptThis, ymodem_queue_t *ptFIFOIn)
{
    assert (NULL != ptThis);
    assert (NULL != ptFIFOIn);

    enum {
        START = 0,
        CHECK_STATUS,
        WAIT_HEAD,
        READ_SEQ1,
        READ_SEQ2,
        VALIDATE_SEQ,
        READ_DATA,
        IS_IN_RANGE,
        READ_VALIDATION1,
        READ_VALIDATION2,
        VALIDATE_CRC,
    };

    switch (this.chGetFrameState) {
        case START:
            ymodem_internal_queue_reset_peek(ptFIFOIn);
            this.uIndex = 0;
            this.hwCheckSum = 0;
            this.chGetFrameState++;
            /* fall through */

        case CHECK_STATUS:
            if ((false != this.bBusy) || this.bComplete) {
                this.chGetFrameState = WAIT_HEAD;
            } else {
                ymodem_queue_write_byte(this.tCFG.ptFIFOOut, CAN);
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_CANCELLED;
            }
            /* fall through */
        
        case WAIT_HEAD:
            if (false != ymodem_internal_queue_peek_byte(ptFIFOIn, &(this.pchFrame[0]))) {
                switch (this.pchFrame[0]) {
                    case EOT:
                        XMODEM_GET_FRAME_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_EOT;

                    case CAN:
                        XMODEM_GET_FRAME_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_CANCELLED;

                    case SOH128B:
                        this.uIndex++;
                        this.uSize = 128;
                        this.chGetFrameState = READ_SEQ1;
                        break;

                    case SOH1K:
                        this.uIndex++;
                        this.uSize = 1024;
                        this.chGetFrameState = READ_SEQ1;
                        break;

                    default:
                        XMODEM_GET_FRAME_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_REQ_MOV;
                }
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case READ_SEQ1:
            if (ymodem_internal_queue_peek_byte(ptFIFOIn, &(this.pchFrame[this.uIndex]))) {
                this.uIndex++;
                this.chGetFrameState = READ_SEQ2;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case READ_SEQ2:
            if (ymodem_internal_queue_peek_byte(ptFIFOIn, &(this.pchFrame[this.uIndex]))) {
                this.uIndex++;
                this.chGetFrameState = VALIDATE_SEQ;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case VALIDATE_SEQ:
            if (0xFF == this.pchFrame[1] + this.pchFrame[2]) {
                this.chGetFrameState = READ_DATA;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR;
            }
            /* fall through */

        case READ_DATA:
            LABEL_READ_DATA:
            if (ymodem_internal_queue_peek_byte (ptFIFOIn, &(this.pchFrame[this.uIndex]))) {
                this.hwCheckSum = update_crc(&(this.pchFrame[this.uIndex++]), this.hwCheckSum);
                this.chGetFrameState = IS_IN_RANGE;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case IS_IN_RANGE:
            if (this.uIndex < (this.uSize + 3)) {
                if (0 == (this.uIndex & 0x1F)) {
                    this.chGetFrameState = READ_DATA;
                    return YMODEM_INTERNAL_XMODEM_RT_YIELD;
                }
                this.chGetFrameState = READ_DATA;
                goto LABEL_READ_DATA;
            } else {
                this.chGetFrameState = READ_VALIDATION1;
            }
            /* fall through */

        case READ_VALIDATION1:
            if (ymodem_internal_queue_peek_byte(ptFIFOIn, &(this.pchFrame[this.uIndex]))) {
                this.uIndex++;
                this.chGetFrameState = READ_VALIDATION2;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case READ_VALIDATION2:
            if (ymodem_internal_queue_peek_byte(ptFIFOIn, &(this.pchFrame[this.uIndex]))) {
                this.uIndex++;
                this.chGetFrameState = VALIDATE_CRC;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
            }
            /* fall through */

        case VALIDATE_CRC:
            if (    (this.pchFrame[this.uSize + 3] == ((this.hwCheckSum >> 8) & 0xFF))
                &&  (this.pchFrame[this.uSize + 4]) == (this.hwCheckSum & 0xFF)) {
                    XMODEM_GET_FRAME_RESET_FSM();
                    return YMODEM_INTERNAL_XMODEM_RT_GET_FRAME;
            } else {
                XMODEM_GET_FRAME_RESET_FSM();
                return YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR;
            }
    }

    return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
}

#define XMODEM_PREPARE_RESET_FSM() \
    do {this.chPrepareState = START;} while(0)

static ymodem_internal_xmodem_fsm_rt_t xmodem_prepare(ymodem_internal_xmodem_t *ptThis, ymodem_queue_t *ptFIFOIn)
{
    assert (NULL != ptThis);

    enum {
        START = 0,
        GET_FRAME,
    };

    switch (this.chPrepareState) {
        case START:
            this.chSeq = 0;
            ymodem_internal_event_set(&(this.tTaskActive));
            this.bIsFirstFrame = true;
            this.chPrepareState = GET_FRAME;
            /* fall through */

        case GET_FRAME:
            do{
                ymodem_internal_xmodem_fsm_rt_t emRet = YMODEM_INTERNAL_XMODEM_RT_REQ_MOV;
                emRet = xmodem_get_frame(ptThis, ptFIFOIn);

                switch (emRet) {
                    case YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR:
                        XMODEM_PREPARE_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR;

                    case YMODEM_INTERNAL_XMODEM_RT_CANCELLED:
                        XMODEM_PREPARE_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_CANCELLED;

                    case YMODEM_INTERNAL_XMODEM_RT_GET_FRAME:
                        XMODEM_PREPARE_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_GET_FRAME;

                    case YMODEM_INTERNAL_XMODEM_RT_EOT:
                        XMODEM_PREPARE_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_EOT;

                    case YMODEM_INTERNAL_XMODEM_RT_REQ_MOV:
                        XMODEM_PREPARE_RESET_FSM();
                        return YMODEM_INTERNAL_XMODEM_RT_REQ_MOV;

                    case YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA:
                        return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;

                    case YMODEM_INTERNAL_XMODEM_RT_YIELD:
                        return YMODEM_INTERNAL_XMODEM_RT_YIELD;
                }

            }while(0);
    }
    
    return YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
}

#define XMODEM_TASK_RESET_FSM() \
    do {this.chTaskState = START;} while(0)

ymodem_fsm_rt_t ymodem_internal_xmodem_task(ymodem_internal_xmodem_t *ptThis)
{
    assert(NULL != ptThis);

    enum {
        START = 0,
        WAIT_ACTIVATION,
        SEND,
        DELAY,
    };

    switch (this.chTaskState) {
        case START:
            this.chTaskState++;
            /* fall through */

        case WAIT_ACTIVATION:
            if (ymodem_internal_event_wait(&(this.tTaskActive))) {
                this.chTaskState = SEND;
            } else {
                XMODEM_TASK_RESET_FSM();
                return ymodem_fsm_rt_cpl;
            }
            /* fall through */

        case SEND:
            if (ymodem_queue_write_byte(this.tCFG.ptFIFOOut, 'C')) {
                this.wDelayCnt = 0;
                this.chTaskState = DELAY;
            } else {
                return ymodem_fsm_rt_on_going;
            }
            /* fall through */

        case DELAY:
            if ((this.wDelayCnt++) < DELAY_COUNT) {
                return ymodem_fsm_rt_on_going;
            }

            if ((false != this.bBusy) &&
                (false != this.bIsFirstFrame)) {
                this.chTaskState = SEND;
                return ymodem_fsm_rt_on_going;
            }

            XMODEM_TASK_RESET_FSM();
            return ymodem_fsm_rt_cpl;
    }

    return ymodem_fsm_rt_on_going;
}

#define XMODEM_CHECKER_RESET_FSM() \
    do {this.chCheckerState = START;} while(0)

static ymodem_internal_win_checker_rt_t xmodem_checker(ymodem_internal_win_checker_item_t *ptItem, ymodem_queue_t *ptQueue)
{
    assert (NULL != ptItem);
    assert (NULL != ptQueue);

    ymodem_internal_xmodem_t *ptThis = (ymodem_internal_xmodem_t *)ptItem;

    enum {
        START = 0,
        CHECK_STATUS,
        PREPARE,
        ERR_HANDLE,
        GET_FRAME,
    };

    switch (this.chCheckerState) {
        case START:
            this.bErrCnt = false;
            this.chCheckerState++;
            /* fall through */

        case CHECK_STATUS:
            if (false != this.bBusy) {
                this.chCheckerState = PREPARE;
            } else {
                XMODEM_CHECKER_RESET_FSM();
                return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;
            }
            /* fall through */

        case PREPARE:
            do {
                ymodem_internal_xmodem_fsm_rt_t emRet = YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
                emRet = xmodem_prepare(ptThis, ptQueue);
                switch (emRet) {
                    case YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR:
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_ERROR, &this.pchFrame[3], this.uSize);
                        this.chCheckerState = ERR_HANDLE;
                        return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;
                        
                    case YMODEM_INTERNAL_XMODEM_RT_CANCELLED:
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_CANCELLED, &this.pchFrame[3], this.uSize);
                        this.bBusy = false;
                        XMODEM_CHECKER_RESET_FSM();
                        this.bIsFirstFrame = false;
                        return YMODEM_INTERNAL_WIN_CHK_RT_CPL;

                    case YMODEM_INTERNAL_XMODEM_RT_EOT:
                        ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK);
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_COMPLETE, NULL, 0u);
                        this.bBusy = false;
                        XMODEM_CHECKER_RESET_FSM();
                        this.bIsFirstFrame = false;
                        return YMODEM_INTERNAL_WIN_CHK_RT_CPL;

                    case YMODEM_INTERNAL_XMODEM_RT_GET_FRAME:
                        this.chCheckerState = GET_FRAME;
                        goto LABEL_HANDLE_VALID_FRAME;

                    case YMODEM_INTERNAL_XMODEM_RT_REQ_MOV:
                        return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;

                    case YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA:
                        return YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA;

                    case YMODEM_INTERNAL_XMODEM_RT_YIELD:
                        return YMODEM_INTERNAL_WIN_CHK_RT_YIELD;
                }
            } while (0);
            /* fall through */

        case ERR_HANDLE:
            if (false == this.bErrCnt) {
                ymodem_queue_write_byte(this.tCFG.ptFIFOOut, NACK);
                this.bErrCnt = true;
            }
            this.chCheckerState = GET_FRAME;
            /* fall through */

        case GET_FRAME:
            do {
                ymodem_internal_xmodem_fsm_rt_t emRet = YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA;
                emRet = xmodem_get_frame(ptThis, ptQueue);
                if (this.bComplete) {
                    if (YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA == emRet) {
                        return YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA;
                    }
                    if (YMODEM_INTERNAL_XMODEM_RT_YIELD == emRet) {
                        return YMODEM_INTERNAL_WIN_CHK_RT_YIELD;
                    }
                    if ((YMODEM_INTERNAL_XMODEM_RT_GET_FRAME == emRet)
                        && (0u == this.pchFrame[1])
                        && ('\0' == this.pchFrame[3])) {
                        /* A lost final ACK must not cause another COMPLETE report. */
                        if (ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK)) {
                            return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                        }
                        return YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA;
                    }
                    return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;
                }
                switch (emRet) {
                    case YMODEM_INTERNAL_XMODEM_RT_CHECKSUM_ERR:
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj,YMODEM_ERROR, this.pchFrame, this.uSize);
                        this.chCheckerState = ERR_HANDLE;
                        return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;

                    case YMODEM_INTERNAL_XMODEM_RT_CANCELLED:
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj,YMODEM_CANCELLED, this.pchFrame, this.uSize);
                        this.bBusy = false;
                        XMODEM_CHECKER_RESET_FSM();
                        return YMODEM_INTERNAL_WIN_CHK_RT_CPL;

                    case YMODEM_INTERNAL_XMODEM_RT_EOT:
                        if (this.bIsYmodem) {
                            /*
                             * The current sender uses a single EOT. Queue the
                             * complete simplified handshake here. If the sender
                             * is changed to standard double-EOT operation, this
                             * branch must wait for the second EOT before ACK/C.
                             */
                            ymodem_queue_write_byte(this.tCFG.ptFIFOOut, NACK);
                            ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK);
                            ymodem_queue_write_byte(this.tCFG.ptFIFOOut, 'C');

                            this.chSeq = 0u;
                            this.bErrCnt = false;
                            this.bIsFirstFrame = true;
                            this.chCheckerState = GET_FRAME;
                            return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                        }

                        ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK);
                        (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj,YMODEM_COMPLETE, this.pchFrame, this.uSize);
                        this.bBusy = false;
                        XMODEM_CHECKER_RESET_FSM();
                        return YMODEM_INTERNAL_WIN_CHK_RT_CPL;

                    case YMODEM_INTERNAL_XMODEM_RT_GET_FRAME:
                        LABEL_HANDLE_VALID_FRAME:
                        if (this.bIsFirstFrame) {
                            ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK);
                            this.bIsFirstFrame = false;
                            this.bErrCnt = false;
                            this.chCheckerState = GET_FRAME;

                            if (0u == this.pchFrame[1]) {
                                this.bIsYmodem = true;

                                if ('\0' == this.pchFrame[3]) {
                                    this.bComplete = true;
                                    (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_COMPLETE, NULL, 0);
                                    this.bBusy = false;
                                    this.chSeq = 0u;
                                } else {
                                    /* ACK was queued above; C requests data block 1. */
                                    ymodem_queue_write_byte(this.tCFG.ptFIFOOut, 'C');
                                    (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_START, &this.pchFrame[3], this.uSize);
                                    this.chSeq = 1u;
                                }

                                return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                            } else if (1u == this.pchFrame[1]) {
                                this.bIsYmodem = false;
                                (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_START, NULL, 0);
                                (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj, YMODEM_NEW_FRAME, &this.pchFrame[3], this.uSize);
                                this.chSeq = 2u;
                                return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                            } else {
                                this.bBusy = false;
                                XMODEM_CHECKER_RESET_FSM();
                                return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                            }
                        }

                        if (this.pchFrame[1] == this.chSeq) {
                            this.bErrCnt = false;
                            this.chCheckerState = GET_FRAME;
                            ymodem_queue_write_byte(this.tCFG.ptFIFOOut, ACK);
                            (*(this.tCFG.evtReport.fnHandler))(this.tCFG.evtReport.pObj,YMODEM_NEW_FRAME, &this.pchFrame[3], this.uSize);
                            this.chSeq++;
                            return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                        } else if (this.pchFrame[1] == (uint8_t)(this.chSeq - 1U)) {
                            ymodem_queue_write_byte (this.tCFG.ptFIFOOut, ACK);

                            /* Repeat ACK/C when the YMODEM block-0 reply was lost. */
                            if (    this.bIsYmodem
                                &&  (1u == this.chSeq)
                                &&  (0u == this.pchFrame[1])) {
                                ymodem_queue_write_byte(this.tCFG.ptFIFOOut, 'C');
                            }

                            this.bErrCnt = false;
                            this.chCheckerState = GET_FRAME;
                            return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                        } else {
                            this.bBusy = false;
                            XMODEM_CHECKER_RESET_FSM();
                            return YMODEM_INTERNAL_WIN_CHK_RT_CPL;
                        }
                        

                    case YMODEM_INTERNAL_XMODEM_RT_REQ_MOV:
                        return YMODEM_INTERNAL_WIN_CHK_RT_REQ_MOV;

                    case YMODEM_INTERNAL_XMODEM_RT_WAIT_DATA:
                        return YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA;

                    case YMODEM_INTERNAL_XMODEM_RT_YIELD:
                        return YMODEM_INTERNAL_WIN_CHK_RT_YIELD;
                }
            } while (0);
    }

    return YMODEM_INTERNAL_WIN_CHK_RT_WAIT_DATA;
}

static uint16_t update_crc(uint8_t *pchData, uint16_t hwCRC)
{
    uint8_t chBit;

    assert(NULL != pchData);

    hwCRC ^= (uint16_t)(*pchData) << 8;

    for (chBit = 0u; chBit < 8u; chBit++) {
        if (0u != (hwCRC & 0x8000u)) {
            hwCRC = (uint16_t)((hwCRC << 1) ^ 0x1021u);
        } else {
            hwCRC = (uint16_t)(hwCRC << 1);
        }
    }

    return hwCRC;
}
