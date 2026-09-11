#include "ymodem_helper.h"
#include "./__common.h"

#undef this
#define this (*ptThis)

static void ymodem_helper_report(
    void *pObj,
    ymodem_report_t emReport,
    uint8_t *pchData,
    size_t uSize)
{
    ymodem_t *ptThis = (ymodem_t *)pObj;

    assert(NULL != ptThis);
    if (NULL != this.tCFG.fnHandler) {
        this.tCFG.fnHandler(this.tCFG.pObj, emReport, pchData, uSize);
    }
}

ymodem_t *ymodem_helper_init(
    ymodem_t *ptThis,
    ymodem_helper_cfg_t *ptCFG)
{
    ymodem_internal_win_checker_cfg_t tWindowCheckerCFG = {0};
    ymodem_internal_xmodem_cfg_t tXmodemCFG = {0};

    assert(NULL != ptThis);
    assert(NULL != ptCFG);
    assert(NULL != ptCFG->ptIn);
    assert(NULL != ptCFG->ptOut);

    this.tCFG = *ptCFG;
    tWindowCheckerCFG.ptFIFO = this.tCFG.ptIn;
    tXmodemCFG.ptFIFOOut = this.tCFG.ptOut;
    tXmodemCFG.evtReport.fnHandler = &ymodem_helper_report;
    tXmodemCFG.evtReport.pObj = ptThis;

    (void)ymodem_internal_win_checker_init(
        &this.tWindowChecker, &tWindowCheckerCFG);
    (void)ymodem_internal_xmodem_init(&this.tXmodem, &tXmodemCFG);
    (void)ymodem_internal_win_checker_register(
        &this.tWindowChecker,
        ymodem_internal_xmodem_item(&this.tXmodem));
    (void)ymodem_internal_xmodem_start_receive(&this.tXmodem);

    return ptThis;
}

ymodem_fsm_rt_t ymodem_helper_task(ymodem_t *ptThis)
{
    assert(NULL != ptThis);

    (void)ymodem_internal_win_checker(&this.tWindowChecker);
    return ymodem_internal_xmodem_task(&this.tXmodem);
}
