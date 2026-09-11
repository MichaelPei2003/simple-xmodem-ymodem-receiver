#ifndef YMODEM_HELPER_QUEUE_H
#define YMODEM_HELPER_QUEUE_H

#include "../app_cfg.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ymodem_queue_t {
    uint8_t *pchBuffer;
    uint16_t hwSize;
    uint16_t hwLength;
    uint16_t hwHead;
    uint16_t hwTail;
    uint16_t hwPeek;
    uint16_t hwPeekLength;
} ymodem_queue_t;

ymodem_queue_t *ymodem_queue_init(
    ymodem_queue_t *ptThis,
    void *pBuffer,
    uint16_t hwSize);
bool        ymodem_queue_write_byte                 (ymodem_queue_t *ptThis, uint8_t chByte);
bool        ymodem_queue_read_byte                  (ymodem_queue_t *ptThis, uint8_t *pchByte);
uint16_t    ymodem_queue_length                     (ymodem_queue_t *ptThis);
bool        ymodem_internal_queue_peek_byte         (ymodem_queue_t *ptThis, uint8_t *pchByte);
void        ymodem_internal_queue_drop_all_peeked   (ymodem_queue_t *ptThis);
void        ymodem_internal_queue_reset_peek        (ymodem_queue_t *ptThis);

#endif /* YMODEM_HELPER_QUEUE_H */
