#include "queue.h"
#include "./__common.h"

#undef this
#define this (*ptThis)

ymodem_queue_t *ymodem_queue_init(
    ymodem_queue_t *ptThis,
    void *pBuffer,
    uint16_t hwSize)
{
    assert(NULL != ptThis);
    assert(NULL != pBuffer);
    assert(0u != hwSize);

    memset(ptThis, 0, sizeof(*ptThis));
    this.pchBuffer = (uint8_t *)pBuffer;
    this.hwSize = hwSize;

    return ptThis;
}

bool ymodem_queue_write_byte(ymodem_queue_t *ptThis, uint8_t chByte)
{
    assert(NULL != ptThis);
    assert(NULL != this.pchBuffer);

    if ((0u != this.hwLength) && (this.hwTail == this.hwHead)) {
        return false;
    }

    this.pchBuffer[this.hwTail] = chByte;
    this.hwLength++;
    this.hwPeekLength++;
    this.hwTail++;
    if (this.hwTail >= this.hwSize) {
        this.hwTail = 0u;
    }

    return true;
}

bool ymodem_queue_read_byte(ymodem_queue_t *ptThis, uint8_t *pchByte)
{
    assert(NULL != ptThis);
    assert(NULL != this.pchBuffer);

    if (0u == this.hwLength) {
        return false;
    }

    if (NULL != pchByte) {
        *pchByte = this.pchBuffer[this.hwHead];
    }

    this.hwLength--;
    this.hwHead++;
    if (this.hwHead >= this.hwSize) {
        this.hwHead = 0u;
    }

    ymodem_internal_queue_reset_peek(ptThis);
    return true;
}

uint16_t ymodem_queue_length(ymodem_queue_t *ptThis)
{
    assert(NULL != ptThis);
    return this.hwLength;
}

bool ymodem_internal_queue_peek_byte(ymodem_queue_t *ptThis, uint8_t *pchByte)
{
    assert(NULL != ptThis);
    assert(NULL != this.pchBuffer);

    if (0u == this.hwPeekLength) {
        return false;
    }

    if (NULL != pchByte) {
        *pchByte = this.pchBuffer[this.hwPeek];
    }

    this.hwPeekLength--;
    this.hwPeek++;
    if (this.hwPeek >= this.hwSize) {
        this.hwPeek = 0u;
    }

    return true;
}

void ymodem_internal_queue_drop_all_peeked(ymodem_queue_t *ptThis)
{
    assert(NULL != ptThis);
    this.hwHead = this.hwPeek;
    this.hwLength = this.hwPeekLength;
}

void ymodem_internal_queue_reset_peek(ymodem_queue_t *ptThis)
{
    assert(NULL != ptThis);
    this.hwPeek = this.hwHead;
    this.hwPeekLength = this.hwLength;
}
