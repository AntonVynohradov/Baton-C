/**
 * @file  ic_queue.c
 * @brief Variable size record ring buffer.
 *
 * Layout rules
 *  - Records are 4-byte aligned: ic_rec_t header + data padded to 4.
 *  - A record is never split at the end of the buffer. If it does not fit at
 *    the end, a SKIP marker is written there (when there is room for a header)
 *    and the record goes to offset 0. A reader that reaches an offset with no
 *    room for a header or with a SKIP marker continues at 0.
 *  - head == tail means empty; the producer resets both to 0 when empty, so
 *    the "wrapped" state never starts from a stale head.
 *  - Records are released only from the head, after every node acknowledged
 *    them (pending == 0) and no unacknowledged frame refers to them
 *    (inflight == 0). This keeps queue offsets stored in channels valid.
 *
 * ic_q_alloc() must be called with the critical section held.
 */
#include <string.h>
#include "ic_internal.h"

static uint16_t align4(uint16_t v)
{
    return (uint16_t)((v + 3u) & ~3u);
}

void ic_q_init(ic_queue_t *q, uint32_t *mem, uint16_t sizeBytes)
{
    q->mem  = mem;
    q->size = (uint16_t)(sizeBytes & ~3u);
    q->head = 0;
    q->tail = 0;
}

uint16_t ic_q_rec_size(uint8_t len)
{
    return (uint16_t)(IC_REC_HDR + align4(len));
}

ic_rec_t *ic_q_rec(const ic_queue_t *q, uint16_t off)
{
    return (ic_rec_t *)(void *)((uint8_t *)q->mem + off);
}

uint8_t *ic_q_data(ic_rec_t *rec)
{
    return (uint8_t *)(rec + 1);
}

static uint16_t q_norm(const ic_queue_t *q, uint16_t off, uint16_t tail)
{
    if (off != tail)
    {
        if (((uint32_t)off + IC_REC_HDR > q->size) || ((ic_q_rec(q, off)->flags & IC_REC_SKIP) != 0u))
        {
            return 0u;
        }
    }
    return off;
}

uint16_t ic_q_first(const ic_queue_t *q, uint16_t head, uint16_t tail)
{
    return q_norm(q, head, tail);
}

uint16_t ic_q_next(const ic_queue_t *q, uint16_t off, uint16_t tail)
{
    const ic_rec_t *rec = ic_q_rec(q, off);
    return q_norm(q, (uint16_t)(off + ic_q_rec_size(rec->len)), tail);
}

bool ic_q_alloc(ic_queue_t *q, uint16_t need, uint16_t *off)
{
    if (q->head == q->tail)
    {
        q->head = 0;
        q->tail = 0;
    }

    uint16_t h = q->head;
    uint16_t t = q->tail;

    if (t >= h)
    {
        if ((uint32_t)q->size - t >= need)
        {
            *off    = t;
            q->tail = (uint16_t)(t + need);
            return true;
        }
        if (h > need)  /* wrap; strict so that tail never catches head */
        {
            if ((uint32_t)q->size - t >= IC_REC_HDR)
            {
                ic_rec_t *skip = ic_q_rec(q, t);
                memset(skip, 0, sizeof(*skip));
                skip->flags = IC_REC_SKIP;
            }
            *off    = 0;
            q->tail = need;
            return true;
        }
        return false;
    }

    if ((uint16_t)(h - t) > need)
    {
        *off    = t;
        q->tail = (uint16_t)(t + need);
        return true;
    }
    return false;
}

uint16_t ic_q_used(const ic_queue_t *q)
{
    uint16_t h = q->head;
    uint16_t t = q->tail;
    return (t >= h) ? (uint16_t)(t - h) : (uint16_t)(q->size - h + t);
}
