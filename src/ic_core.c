/**
 * @file  ic_core.c
 * @brief Intercom protocol engine: parser, transmitter, link layer
 *        (master and slave state machines), message packing, deduplication.
 *
 * Everything here runs from ic_process() except ic_rx_byte_isr(),
 * ic_tx_done_isr(), and ic_send()/ic_publish() which may run in an ISR when
 * critical section hooks are configured.
 */
#include <string.h>
#include "ic_internal.h"

/* ======================================================================== */
/* Message descriptor tables (generated from IntercomConfig.h)              */
/* ======================================================================== */
#define IC_X_STATE_DESC_(name, id, maxLen) \
    { (uint8_t)(id), (uint8_t)(maxLen), (uint16_t)offsetof(ic_state_data_t, name) },
#define IC_X_EVENT_DESC_(name, id, maxLen) \
    { (uint8_t)(id), (uint8_t)(maxLen), 0u },

static const ic_msg_desc_t s_stateDesc[IC_STATE_COUNT + 1] = {
    INTERCOM_STATE_LIST(IC_X_STATE_DESC_)
    { 0u, 0u, 0u }
};
static const ic_msg_desc_t s_eventDesc[IC_EVENT_COUNT + 1] = {
    INTERCOM_EVENT_LIST(IC_X_EVENT_DESC_)
    { 0u, 0u, 0u }
};

/* Compile-time checks of every message definition. */
#define IC_X_CHECK_(name, id, maxLen)                                          \
    typedef char ic_check_len_##name[((maxLen) >= 1 && (maxLen) <= (int)IC_MAX_MSG_LEN) ? 1 : -1]; \
    typedef char ic_check_id_##name[((id) >= 0 && (id) <= 255) ? 1 : -1];
INTERCOM_STATE_LIST(IC_X_CHECK_)
INTERCOM_EVENT_LIST(IC_X_CHECK_)

#define IC_HOUSEKEEP_PERIOD_MS 10u
#define IC_RX_MASK             ((uint16_t)(INTERCOM_RX_BUF_SIZE - 1u))

/* ======================================================================== */
/* Small helpers                                                             */
/* ======================================================================== */
static uint32_t now_ms(const ic_t *ic)
{
    return ic->cfg.getTimeMs(ic->cfg.ctx);
}

static void lock(const ic_t *ic)
{
    if (ic->cfg.enterCritical != NULL)
    {
        ic->cfg.enterCritical(ic->cfg.ctx);
    }
}

static void unlock(const ic_t *ic)
{
    if (ic->cfg.exitCritical != NULL)
    {
        ic->cfg.exitCritical(ic->cfg.ctx);
    }
}

static ic_mask_t node_bit(uint8_t idx)
{
    return (ic_mask_t)1u << idx;
}

/** Duration of a frame of `bytes` bytes on the wire (8N1 = 10 bits/byte), rounded up. */
static uint32_t frame_ms(const ic_t *ic, uint32_t bytes)
{
    return (bytes * 10000u + ic->cfg.baudrate - 1u) / ic->cfg.baudrate;
}

static int node_index(const ic_t *ic, uint8_t addr)
{
    for (uint8_t i = 0; i < ic->nodeCount; i++)
    {
        if (ic->nodeAddr[i] == addr)
        {
            return (int)i;
        }
    }
    return -1;
}

static const ic_msg_desc_t *find_desc(const ic_msg_desc_t *tab, int count, uint8_t id, int *idx)
{
    for (int i = 0; i < count; i++)
    {
        if (tab[i].id == id)
        {
            if (idx != NULL)
            {
                *idx = i;
            }
            return &tab[i];
        }
    }
    return NULL;
}

static int msg_max_len(uint8_t id)
{
    const ic_msg_desc_t *d = find_desc(s_stateDesc, IC_STATE_COUNT, id, NULL);
    if (d == NULL)
    {
        d = find_desc(s_eventDesc, IC_EVENT_COUNT, id, NULL);
    }
    return (d != NULL) ? (int)d->maxLen : -1;
}

static const ic_msg_desc_t *desc_at(int k)
{
    return (k < IC_STATE_COUNT) ? &s_stateDesc[k] : &s_eventDesc[k - IC_STATE_COUNT];
}

static uint16_t frame_finish(uint8_t *f, uint8_t dst, uint8_t src, uint8_t ctrl, uint8_t len)
{
    f[0] = IC_SYNC;
    f[1] = dst;
    f[2] = src;
    f[3] = ctrl;
    f[4] = len;
    uint16_t crc = ic_crc16(&f[1], (uint16_t)(4u + len));
    f[IC_HDR_LEN + len]      = (uint8_t)(crc >> 8);
    f[IC_HDR_LEN + len + 1u] = (uint8_t)crc;
    return (uint16_t)(IC_OVERHEAD + len);
}

/** Shortest slot for which a slave can still send one maximum size message. */
static uint32_t min_slot_ms(const ic_t *ic)
{
    const IntercomCfg *c = &ic->cfg;
    return frame_ms(ic, IC_FRAME_MAX) + 3u * c->txGapMs + 2u + c->ackTimeoutMs +
           frame_ms(ic, IC_OVERHEAD + IC_TLV_HDR + IC_MAX_MSG_LEN);
}

/* ======================================================================== */
/* Configuration                                                             */
/* ======================================================================== */
void ic_default_cfg(IntercomCfg *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->role               = INTERCOM_ROLE_SLAVE;
    cfg->baudrate           = 9600u;
    cfg->slotTimeMs         = 250u;
    cfg->ackTimeoutMs       = 20u;
    cfg->maxRetries         = 1u;
    cfg->txGapMs            = 2u;
    cfg->interByteTimeoutMs = 5u;
    cfg->cyclePeriodMs      = 0u;
    cfg->eventTtlMs         = 5000u;
    cfg->rxEcho             = true;
}

Intercom_Status ic_init(ic_t *ic, const IntercomCfg *cfg)
{
    if ((ic == NULL) || (cfg == NULL))
    {
        return INTERCOM_ERR_PARAM;
    }
    ic->initialized = false;

    if ((cfg->getTimeMs == NULL) || (cfg->uartStartTx == NULL) || (cfg->slaveAddrs == NULL) ||
        ((cfg->enterCritical == NULL) != (cfg->exitCritical == NULL)) ||
        (cfg->slaveCount == 0u) || (cfg->slaveCount > (INTERCOM_MAX_NODES - 1)) ||
        (cfg->baudrate == 0u) || (cfg->ackTimeoutMs == 0u) || (cfg->interByteTimeoutMs == 0u))
    {
        return INTERCOM_ERR_PARAM;
    }

    memset(ic, 0, sizeof(*ic));
    ic->cfg = *cfg;

    /* Topology: index 0 = master, 1..n = slaves in polling order. */
    ic->nodeCount   = (uint8_t)(cfg->slaveCount + 1u);
    ic->nodeAddr[0] = cfg->masterAddr;
    for (uint8_t i = 0; i < cfg->slaveCount; i++)
    {
        ic->nodeAddr[i + 1u] = cfg->slaveAddrs[i];
    }
    for (uint8_t i = 0; i < ic->nodeCount; i++)
    {
        if (ic->nodeAddr[i] == INTERCOM_ADDR_BROADCAST)
        {
            return INTERCOM_ERR_PARAM;
        }
        for (uint8_t j = (uint8_t)(i + 1u); j < ic->nodeCount; j++)
        {
            if (ic->nodeAddr[i] == ic->nodeAddr[j])
            {
                return INTERCOM_ERR_PARAM;
            }
        }
    }

    int self = node_index(ic, cfg->ownAddr);
    if ((self < 0) ||
        ((cfg->role == INTERCOM_ROLE_MASTER) && (self != 0)) ||
        ((cfg->role == INTERCOM_ROLE_SLAVE) && (self == 0)) ||
        ((cfg->role != INTERCOM_ROLE_MASTER) && (cfg->role != INTERCOM_ROLE_SLAVE)))
    {
        return INTERCOM_ERR_PARAM;
    }
    ic->selfIdx = (uint8_t)self;
    ic_mask_t all = (ic->nodeCount >= 32u) ? 0xFFFFFFFFu : (node_bit(ic->nodeCount) - 1u);
    ic->othersMask = all & ~node_bit(ic->selfIdx);

    /* Message ids must be unique across both lists. */
    for (int i = 0; i < IC_STATE_COUNT + IC_EVENT_COUNT; i++)
    {
        for (int j = i + 1; j < IC_STATE_COUNT + IC_EVENT_COUNT; j++)
        {
            if (desc_at(i)->id == desc_at(j)->id)
            {
                return INTERCOM_ERR_PARAM;
            }
        }
    }

    /* Timing must allow at least one maximum size exchange per slot. */
    if ((cfg->ackTimeoutMs <= frame_ms(ic, IC_OVERHEAD) + cfg->txGapMs) ||
        (cfg->slotTimeMs < min_slot_ms(ic)))
    {
        return INTERCOM_ERR_PARAM;
    }

    ic_q_init(&ic->q[IC_REF_HIGH], ic->qMemHigh, (uint16_t)sizeof(ic->qMemHigh));
    ic_q_init(&ic->q[IC_REF_NORMAL], ic->qMemNormal, (uint16_t)sizeof(ic->qMemNormal));
    for (int i = 0; i < IC_EVENT_COUNT; i++)
    {
        uint16_t need = ic_q_rec_size(s_eventDesc[i].maxLen);
        if ((need >= ic->q[0].size) || (need >= ic->q[1].size))
        {
            return INTERCOM_ERR_PARAM;
        }
    }

    for (uint8_t i = 0; i < INTERCOM_MAX_NODES; i++)
    {
        ic->chan[i].txFirst = 1u;
    }

    uint32_t t = now_ms(ic);
    ic->lastRxMs        = t;
    ic->lastTxEndMs     = t;
    ic->pLastMs         = t;
    ic->lastHousekeepMs = t;
    ic->mCur            = cfg->slaveCount;               /* first Process starts a cycle */
    ic->cycleStart      = t - cfg->cyclePeriodMs;
    ic->pState          = IC_P_SYNC;
    ic->txState         = IC_TX_IDLE;
    ic->lState          = IC_L_IDLE;
    ic->initialized     = true;
    return INTERCOM_OK;
}

/* ======================================================================== */
/* ISR entry points                                                          */
/* ======================================================================== */
void ic_rx_byte_isr(ic_t *ic, uint8_t byte)
{
    uint16_t h = ic->rxHead;
    uint16_t n = (uint16_t)((h + 1u) & IC_RX_MASK);
    if (n == ic->rxTail)
    {
        ic->rxOverflows++;
        return;
    }
    ic->rxBuf[h] = byte;
    ic->rxHead   = n;
}

void ic_tx_done_isr(ic_t *ic)
{
    ic->txDoneFlag = 1u;
}

/* ======================================================================== */
/* Channels: stop-and-wait bookkeeping                                       */
/* ======================================================================== */
static ic_rec_t *ref_rec(const ic_t *ic, uint16_t ref)
{
    return ic_q_rec(&ic->q[IC_REF_KIND(ref)], (uint16_t)(IC_REF_VAL(ref) << 2));
}

static void chan_remove_at(ic_chan_t *c, uint8_t i)
{
    for (uint8_t k = i; (uint8_t)(k + 1u) < c->nInflight; k++)
    {
        c->inflight[k] = c->inflight[k + 1u];
    }
    c->nInflight--;
}

/** Content of the outstanding frame was abandoned: next content uses a new seq. */
static void chan_abandon(ic_chan_t *c)
{
    c->nInflight = 0u;
    c->txSeq     = (uint8_t)((c->txSeq + 1u) & 0x0Fu);
}

/** Acknowledged: release all messages of the outstanding frame for this peer. */
static void chan_commit(ic_t *ic, uint8_t peer)
{
    ic_chan_t *c   = &ic->chan[peer];
    ic_mask_t  bit = node_bit(peer);
    for (uint8_t i = 0; i < c->nInflight; i++)
    {
        uint16_t ref = c->inflight[i];
        if (IC_REF_KIND(ref) != IC_REF_STATE)
        {
            ic_rec_t *rec = ref_rec(ic, ref);
            rec->pending  &= ~bit;
            rec->inflight &= ~bit;
        }
    }
    c->nInflight = 0u;
    c->txSeq     = (uint8_t)((c->txSeq + 1u) & 0x0Fu);
    c->txFirst   = 0u;
}

static void chan_drop_ref(ic_t *ic, uint8_t peer, uint16_t ref)
{
    ic_chan_t *c = &ic->chan[peer];
    for (uint8_t i = 0; i < c->nInflight; i++)
    {
        if (c->inflight[i] == ref)
        {
            chan_remove_at(c, i);
            if (c->nInflight == 0u)
            {
                chan_abandon(c);
            }
            return;
        }
    }
}

/* ======================================================================== */
/* Packing messages into a frame payload (TLV: id, len, data)               */
/* ======================================================================== */
static uint16_t emit(uint8_t *out, uint16_t n, uint8_t id, const uint8_t *data, uint8_t len)
{
    out[n]      = id;
    out[n + 1u] = len;
    if (len > 0u)
    {
        memcpy(&out[n + 2u], data, len);
    }
    return (uint16_t)(n + IC_TLV_HDR + len);
}

/** Copy the current value of state s if it fits below `limit`. Optionally clears dirty bits. */
static bool try_emit_state(ic_t *ic, uint8_t *out, uint16_t *n, uint16_t limit, int s, ic_mask_t clearBit)
{
    bool ok = false;
    lock(ic);
    uint8_t len = ic->stLen[s];
    if ((uint32_t)*n + IC_TLV_HDR + len <= limit)
    {
        *n = emit(out, *n, s_stateDesc[s].id, (const uint8_t *)&ic->stData + s_stateDesc[s].offset, len);
        ic->stDirty[s] &= ~clearBit;
        ok = true;
    }
    unlock(ic);
    return ok;
}

/**
 * Build the message part of a frame for `peer`.
 *  - If a previous frame to this peer is unacknowledged, rebuild the same
 *    content (same seq) so the receiver can filter duplicates.
 *  - Otherwise take HIGH events, NORMAL events (FIFO each), then dirty states.
 * @return bytes written, 0 if nothing to send, -1 if the retransmission does
 *         not fit into maxLen.
 */
static int pack(ic_t *ic, uint8_t peer, uint8_t *out, uint16_t maxLen)
{
    ic_chan_t      *c   = &ic->chan[peer];
    const ic_mask_t bit = node_bit(peer);
    uint16_t        n   = 0u;

    if (c->nInflight > 0u)
    {
        uint16_t evBytes = 0u;
        for (uint8_t i = 0; i < c->nInflight; i++)
        {
            if (IC_REF_KIND(c->inflight[i]) != IC_REF_STATE)
            {
                evBytes = (uint16_t)(evBytes + IC_TLV_HDR + ref_rec(ic, c->inflight[i])->len);
            }
        }
        if (evBytes > maxLen)
        {
            return -1;
        }
        uint8_t i = 0u;
        while (i < c->nInflight)
        {
            uint16_t ref = c->inflight[i];
            if (IC_REF_KIND(ref) == IC_REF_STATE)
            {
                int s = (int)IC_REF_VAL(ref);
                if (!try_emit_state(ic, out, &n, (uint16_t)(maxLen - evBytes), s, 0u))
                {
                    /* state grew and no longer fits: send it in a later frame */
                    chan_remove_at(c, i);
                    lock(ic);
                    ic->stDirty[s] |= bit;
                    unlock(ic);
                    continue;
                }
            }
            else
            {
                ic_rec_t *rec = ref_rec(ic, ref);
                n       = emit(out, n, rec->msgId, ic_q_data(rec), rec->len);
                evBytes = (uint16_t)(evBytes - (IC_TLV_HDR + rec->len));
            }
            i++;
        }
        if (c->nInflight > 0u)
        {
            return (int)n;
        }
        chan_abandon(c);
        n = 0u;
    }

    for (uint8_t qi = 0; (qi < 2u) && (c->nInflight < INTERCOM_MAX_MSGS_PER_FRAME); qi++)
    {
        ic_queue_t *q = &ic->q[qi];
        lock(ic);
        uint16_t head = q->head;
        uint16_t tail = q->tail;
        unlock(ic);
        for (uint16_t off = ic_q_first(q, head, tail); off != tail; off = ic_q_next(q, off, tail))
        {
            ic_rec_t *rec = ic_q_rec(q, off);
            if ((rec->flags & IC_REC_VALID) == 0u)
            {
                break;
            }
            if (((rec->pending & bit) == 0u) || ((rec->inflight & bit) != 0u))
            {
                continue;
            }
            if (((uint32_t)n + IC_TLV_HDR + rec->len > maxLen) ||
                (c->nInflight >= INTERCOM_MAX_MSGS_PER_FRAME))
            {
                break;   /* keep FIFO order: never skip ahead within a queue */
            }
            n = emit(out, n, rec->msgId, ic_q_data(rec), rec->len);
            rec->inflight |= bit;
            c->inflight[c->nInflight++] = IC_REF(qi, off >> 2);
        }
    }

    for (int s = 0; (s < IC_STATE_COUNT) && (c->nInflight < INTERCOM_MAX_MSGS_PER_FRAME); s++)
    {
        if (((ic->stDirty[s] & bit) != 0u) && try_emit_state(ic, out, &n, maxLen, s, bit))
        {
            c->inflight[c->nInflight++] = IC_REF(IC_REF_STATE, (uint16_t)s);
        }
    }
    return (int)n;
}

/* ======================================================================== */
/* Receiving messages                                                        */
/* ======================================================================== */
static void deliver(ic_t *ic, uint8_t srcIdx, const uint8_t *p, uint16_t n)
{
    uint8_t src = ic->nodeAddr[srcIdx];
    while (n > 0u)
    {
        if (n < IC_TLV_HDR)
        {
            ic->stats.badMessages++;
            return;
        }
        uint8_t id  = p[0];
        uint8_t len = p[1];
        if ((uint16_t)(IC_TLV_HDR + len) > n)
        {
            ic->stats.badMessages++;
            return;
        }
        int maxLen = msg_max_len(id);
        if ((maxLen < 0) || ((int)len > maxLen))
        {
            ic->stats.badMessages++;
        }
        else if (ic->cfg.onMessage != NULL)
        {
            ic->cfg.onMessage(src, id, &p[IC_TLV_HDR], len, ic->cfg.ctx);
        }
        p += IC_TLV_HDR + len;
        n  = (uint16_t)(n - (IC_TLV_HDR + len));
    }
}

/** Deliver message content unless it is a retransmission already delivered. */
static void rx_content(ic_t *ic, uint8_t srcIdx, uint8_t ctrl, const uint8_t *p, uint16_t n)
{
    if (n == 0u)
    {
        return;   /* frames without content do not take part in sequencing */
    }
    ic_chan_t *c     = &ic->chan[srcIdx];
    uint8_t    seq   = IC_CTRL_SEQ(ctrl);
    bool       first = IC_CTRL_FIRST(ctrl);
    bool       dup;

    if (c->rxValid == 0u)
    {
        dup = false;
    }
    else if (first)
    {
        dup = (c->rxLastFirst != 0u) && (seq == c->rxSeq);
    }
    else
    {
        dup = (seq == c->rxSeq);
    }

    if (dup)
    {
        ic->stats.duplicates++;
        return;
    }
    c->rxValid     = 1u;
    c->rxSeq       = seq;
    c->rxLastFirst = first ? 1u : 0u;
    deliver(ic, srcIdx, p, n);
}

/* ======================================================================== */
/* Transmitter                                                               */
/* ======================================================================== */
static void link_on_tx_done(ic_t *ic, uint32_t t);

static bool parser_busy(const ic_t *ic, uint32_t t)
{
    return (ic->pState != IC_P_SYNC) && ((uint32_t)(t - ic->pLastMs) <= ic->cfg.interByteTimeoutMs);
}

static void tx_request(ic_t *ic, const uint8_t *buf, uint16_t len)
{
    ic->txPtr   = buf;
    ic->txLen   = len;
    ic->txState = IC_TX_WAIT_GAP;
}

static void tx_finish(ic_t *ic, uint32_t t)
{
    ic->txState     = IC_TX_IDLE;
    ic->lastTxEndMs = t;
    if (ic->echoErr)
    {
        ic->stats.collisions++;
    }
    link_on_tx_done(ic, t);
}

static void tx_service(ic_t *ic, uint32_t t)
{
    if (ic->txState == IC_TX_WAIT_GAP)
    {
        if (parser_busy(ic, t) ||
            ((uint32_t)(t - ic->lastRxMs) < ic->cfg.txGapMs) ||
            ((uint32_t)(t - ic->lastTxEndMs) < ic->cfg.txGapMs))
        {
            return;
        }
        ic->txState    = IC_TX_BUSY;
        ic->txDoneFlag = 0u;
        ic->echoIdx    = 0u;
        ic->echoErr    = false;
        ic->txStartMs  = t;
        ic->stats.txFrames++;
        if (!ic->cfg.uartStartTx(ic->txPtr, ic->txLen, ic->cfg.ctx))
        {
            ic->stats.txTimeouts++;
            tx_finish(ic, t);
        }
    }
    else if (ic->txState == IC_TX_BUSY)
    {
        if (!ic->cfg.rxEcho && (ic->txDoneFlag != 0u))
        {
            tx_finish(ic, t);
        }
        else if ((uint32_t)(t - ic->txStartMs) > frame_ms(ic, ic->txLen) + ic->cfg.txGapMs + 5u)
        {
            ic->stats.txTimeouts++;
            tx_finish(ic, t);
        }
        else
        {
            /* still transmitting */
        }
    }
    else
    {
        /* idle */
    }
}

/* ======================================================================== */
/* Slave                                                                     */
/* ======================================================================== */
/** Payload bytes a DATA frame may still carry in this slot (keeps RESP time reserved). */
static int32_t slave_budget(const ic_t *ic, uint32_t t)
{
    const IntercomCfg *c = &ic->cfg;
    int32_t elapsed = (int32_t)(t - ic->slotStart);
    int32_t reserve = (int32_t)(frame_ms(ic, IC_FRAME_MAX) + 2u * c->txGapMs + 2u);
    int32_t availMs = (int32_t)c->slotTimeMs - elapsed - reserve -
                      (int32_t)c->txGapMs - (int32_t)c->ackTimeoutMs;
    if (availMs <= 0)
    {
        return 0;
    }
    int32_t bytes = (int32_t)(((uint32_t)availMs * c->baudrate) / 10000u) - (int32_t)IC_OVERHEAD;
    if (bytes > (int32_t)INTERCOM_MAX_PAYLOAD)
    {
        bytes = (int32_t)INTERCOM_MAX_PAYLOAD;
    }
    return (bytes < 0) ? 0 : bytes;
}

/** @return 1 frame queued, 0 nothing for this peer, -1 no time left / does not fit. */
static int slave_tx_data(ic_t *ic, uint8_t peer, uint32_t t)
{
    int32_t budget = slave_budget(ic, t);
    if (budget < (int32_t)IC_TLV_HDR)
    {
        return -1;
    }
    int r = pack(ic, peer, &ic->txFrame[IC_HDR_LEN], (uint16_t)budget);
    if (r <= 0)
    {
        return r;
    }
    ic_chan_t *c   = &ic->chan[peer];
    uint16_t   len = frame_finish(ic->txFrame, ic->nodeAddr[peer], ic->cfg.ownAddr,
                                  IC_CTRL(IC_FT_DATA, c->txFirst, c->txSeq), (uint8_t)r);
    tx_request(ic, ic->txFrame, len);
    ic->lState = IC_S_DATA_TX;
    return 1;
}

static void slave_send_resp(ic_t *ic)
{
    uint8_t *pl = &ic->txFrame[IC_HDR_LEN];
    pl[0] = (uint8_t)(IC_ACKB_VALID | (ic->pollSeq & 0x0Fu));
    for (uint8_t i = 0; i < IC_MASK_BYTES; i++)
    {
        pl[1u + i] = (uint8_t)(ic->slotNack >> (8u * i));
    }
    int r = pack(ic, 0u, &pl[IC_RESP_PREFIX], (uint16_t)(INTERCOM_MAX_PAYLOAD - IC_RESP_PREFIX));
    if (r < 0)
    {
        r = 0;
    }
    ic_chan_t *c   = &ic->chan[0];
    uint16_t   len = frame_finish(ic->txFrame, ic->nodeAddr[0], ic->cfg.ownAddr,
                                  IC_CTRL(IC_FT_RESP, c->txFirst, c->txSeq),
                                  (uint8_t)(IC_RESP_PREFIX + (uint16_t)r));
    tx_request(ic, ic->txFrame, len);
    ic->lState = IC_S_RESP_TX;
}

/**
 * Continue the slot: DATA to the next peer that has something, else RESP.
 * Pass 0 serves peers that answered recently, pass 1 the ones that failed
 * before, so an unreachable node cannot eat the slot of the healthy ones.
 */
static void slave_slot_step(ic_t *ic, uint32_t t)
{
    const uint8_t nSlaves = (uint8_t)(ic->nodeCount - 1u);
    while (ic->slotPass < 2u)
    {
        while (ic->slotK < nSlaves)
        {
            /* round robin over slaves so no peer is always served last */
            uint8_t p       = (uint8_t)(1u + (uint8_t)((ic->slotRr + ic->slotK) % nSlaves));
            bool    healthy = (ic->chan[p].failStreak < IC_DEAD_STREAK);
            ic->slotPeer    = p;
            if ((p != ic->selfIdx) && (healthy == (ic->slotPass == 0u)) &&
                ((ic->slotNack & node_bit(p)) == 0u))
            {
                if (slave_tx_data(ic, p, t) > 0)
                {
                    return;
                }
            }
            ic->slotK++;
            ic->retries = 0u;
        }
        ic->slotPass++;
        ic->slotK = 0u;
    }
    slave_send_resp(ic);
}

static void slave_on_poll(ic_t *ic, uint8_t ctrl, const uint8_t *p, uint16_t n, uint32_t t)
{
    if (n < IC_POLL_PREFIX)
    {
        ic->stats.badMessages++;
        return;
    }
    ic_chan_t *m = &ic->chan[0];
    if (((p[0] & IC_ACKB_VALID) != 0u) && ((p[0] & 0x0Fu) == m->txSeq) && (m->nInflight > 0u))
    {
        chan_commit(ic, 0u);   /* master confirmed our previous RESP */
    }
    rx_content(ic, 0u, ctrl, &p[IC_POLL_PREFIX], (uint16_t)(n - IC_POLL_PREFIX));

    /* The master is the authority: a POLL always starts a fresh slot. */
    if (ic->txState == IC_TX_WAIT_GAP)
    {
        ic->txState = IC_TX_IDLE;
    }
    ic->pollSeq   = IC_CTRL_SEQ(ctrl);
    ic->slotStart = t;
    ic->slotK     = 0u;
    ic->slotRr++;
    ic->slotPass  = 0u;
    ic->retries   = 0u;
    ic->slotNack  = 0u;
    slave_slot_step(ic, t);
}

static void slave_on_data(ic_t *ic, uint8_t srcIdx, uint8_t ctrl, const uint8_t *p, uint16_t n)
{
    if ((ic->lState != IC_L_IDLE) || (srcIdx == 0u))
    {
        return;
    }
    rx_content(ic, srcIdx, ctrl, p, n);
    uint16_t len = frame_finish(ic->ackFrame, ic->nodeAddr[srcIdx], ic->cfg.ownAddr,
                                IC_CTRL(IC_FT_ACK, 0u, IC_CTRL_SEQ(ctrl)), 0u);
    tx_request(ic, ic->ackFrame, len);
    ic->lState = IC_S_ACK_TX;
}

static void slave_on_ack(ic_t *ic, uint8_t srcIdx, uint8_t ctrl, uint32_t t)
{
    if ((ic->lState != IC_S_WAIT_ACK) || (srcIdx != ic->slotPeer) ||
        (IC_CTRL_SEQ(ctrl) != ic->chan[srcIdx].txSeq) || (ic->chan[srcIdx].nInflight == 0u))
    {
        return;
    }
    chan_commit(ic, srcIdx);
    ic->chan[srcIdx].failStreak = 0u;
    ic->retries = 0u;
    ic->lState  = IC_L_IDLE;
    slave_slot_step(ic, t);   /* same peer first: it may have more */
}

static void slave_ack_timeout(ic_t *ic, uint32_t t)
{
    uint8_t p = ic->slotPeer;
    ic->stats.ackTimeouts++;
    ic->lState = IC_L_IDLE;
    uint8_t maxRetries = (ic->chan[p].failStreak >= IC_DEAD_STREAK) ? 0u : ic->cfg.maxRetries;
    if (ic->retries < maxRetries)
    {
        ic->retries++;
        ic->stats.retries++;
        if (slave_tx_data(ic, p, t) > 0)
        {
            return;
        }
    }
    if (ic->chan[p].nInflight > 0u)
    {
        ic->slotNack |= node_bit(p);   /* reported to the master in RESP */
        if (ic->chan[p].failStreak < 255u)
        {
            ic->chan[p].failStreak++;
        }
    }
    ic->slotK++;
    ic->retries = 0u;
    slave_slot_step(ic, t);
}

/* ======================================================================== */
/* Master                                                                    */
/* ======================================================================== */
static void master_send_poll(ic_t *ic)
{
    uint8_t    si = (uint8_t)(ic->mCur + 1u);
    ic_chan_t *c  = &ic->chan[si];
    uint8_t   *pl = &ic->txFrame[IC_HDR_LEN];

    pl[0] = (uint8_t)((c->respValid ? IC_ACKB_VALID : 0u) | (c->respSeq & 0x0Fu));
    int r = pack(ic, si, &pl[IC_POLL_PREFIX], (uint16_t)(INTERCOM_MAX_PAYLOAD - IC_POLL_PREFIX));
    if (r < 0)
    {
        r = 0;
    }
    uint16_t len = frame_finish(ic->txFrame, ic->nodeAddr[si], ic->cfg.ownAddr,
                                IC_CTRL(IC_FT_POLL, c->txFirst, c->txSeq),
                                (uint8_t)(IC_POLL_PREFIX + (uint16_t)r));
    tx_request(ic, ic->txFrame, len);
    ic->lState = IC_M_POLL_TX;
}

static void master_poll_done(ic_t *ic, bool responded, ic_mask_t nackNodes)
{
    uint8_t    si = (uint8_t)(ic->mCur + 1u);
    ic_chan_t *c  = &ic->chan[si];
    if (responded)
    {
        c->failStreak = 0u;
    }
    else if (c->failStreak < 255u)
    {
        c->failStreak++;
    }
    ic->mCur++;
    ic->lState = IC_L_IDLE;
    if (ic->cfg.onPollDone != NULL)
    {
        ic->cfg.onPollDone(ic->nodeAddr[si], responded, (uint32_t)nackNodes, ic->cfg.ctx);
    }
}

static void master_on_resp(ic_t *ic, uint8_t srcIdx, uint8_t ctrl, const uint8_t *p, uint16_t n)
{
    if ((ic->lState != IC_M_WAIT_RESP) || (srcIdx != (uint8_t)(ic->mCur + 1u)))
    {
        return;
    }
    if (n < IC_RESP_PREFIX)
    {
        ic->stats.badMessages++;
        return;
    }
    ic_chan_t *c = &ic->chan[srcIdx];
    if (((p[0] & IC_ACKB_VALID) != 0u) && ((p[0] & 0x0Fu) == c->txSeq) && (c->nInflight > 0u))
    {
        chan_commit(ic, srcIdx);   /* slave confirmed our POLL content */
    }
    ic_mask_t nack = 0u;
    for (uint8_t i = 0; i < IC_MASK_BYTES; i++)
    {
        nack |= (ic_mask_t)p[1u + i] << (8u * i);
    }
    if (n > IC_RESP_PREFIX)
    {
        /* Acknowledge only RESP frames with content: an empty RESP shares the
           seq of the next content RESP and must not confirm it in advance. */
        c->respSeq   = IC_CTRL_SEQ(ctrl);
        c->respValid = 1u;
    }
    rx_content(ic, srcIdx, ctrl, &p[IC_RESP_PREFIX], (uint16_t)(n - IC_RESP_PREFIX));
    master_poll_done(ic, true, nack);
}

static void master_service(ic_t *ic, uint32_t t)
{
    if (ic->lState == IC_M_WAIT_RESP)
    {
        uint32_t el = (uint32_t)(t - ic->lTimer);
        uint32_t to = (uint32_t)ic->cfg.slotTimeMs + ic->cfg.txGapMs + 10u;
        if (((el >= to) && !parser_busy(ic, t)) || (el >= 2u * to))
        {
            ic->stats.respTimeouts++;
            master_poll_done(ic, false, 0u);
        }
    }
    if (ic->lState != IC_L_IDLE)
    {
        return;
    }
    for (;;)
    {
        if (ic->mCur >= ic->cfg.slaveCount)
        {
            if ((ic->cfg.cyclePeriodMs != 0u) && ((uint32_t)(t - ic->cycleStart) < ic->cfg.cyclePeriodMs))
            {
                return;
            }
            ic->mCur       = 0u;
            ic->cycleStart = t;
            ic->cycleCount++;
        }
        /* an unreachable slave is polled only every IC_DEAD_POLL_DIV-th cycle */
        if ((ic->chan[ic->mCur + 1u].failStreak < IC_DEAD_STREAK) ||
            ((ic->cycleCount % IC_DEAD_POLL_DIV) == 0u))
        {
            break;
        }
        ic->mCur++;   /* bounded: at most IC_DEAD_POLL_DIV cycles are skipped */
    }
    master_send_poll(ic);
}

/* ======================================================================== */
/* Link layer dispatch                                                       */
/* ======================================================================== */
static void link_on_tx_done(ic_t *ic, uint32_t t)
{
    switch (ic->lState)
    {
        case IC_S_DATA_TX:
            ic->lState = IC_S_WAIT_ACK;
            ic->lTimer = t;
            break;
        case IC_M_POLL_TX:
            ic->lState = IC_M_WAIT_RESP;
            ic->lTimer = t;
            break;
        case IC_S_RESP_TX:
        case IC_S_ACK_TX:
            ic->lState = IC_L_IDLE;
            break;
        default:
            break;
    }
}

static void link_service(ic_t *ic, uint32_t t)
{
    if (ic->cfg.role == INTERCOM_ROLE_MASTER)
    {
        master_service(ic, t);
    }
    else if (ic->lState == IC_S_WAIT_ACK)
    {
        uint32_t el = (uint32_t)(t - ic->lTimer);
        if (((el >= ic->cfg.ackTimeoutMs) && !parser_busy(ic, t)) || (el >= 2u * ic->cfg.ackTimeoutMs))
        {
            slave_ack_timeout(ic, t);
        }
    }
    else
    {
        /* slave idle or transmitting: driven by received frames and tx done */
    }
}

static void handle_frame(ic_t *ic, uint32_t t)
{
    const uint8_t *f    = ic->pFrame;
    uint8_t        dst  = f[0];
    uint8_t        ctrl = f[2];
    uint16_t       n    = f[3];
    const uint8_t *p    = &f[4];

    if (dst != ic->cfg.ownAddr)
    {
        return;
    }
    int si = node_index(ic, f[1]);
    if ((si < 0) || (si == (int)ic->selfIdx))
    {
        return;
    }
    bool master = (ic->cfg.role == INTERCOM_ROLE_MASTER);
    switch (IC_CTRL_TYPE(ctrl))
    {
        case IC_FT_POLL:
            if (!master && (si == 0))
            {
                slave_on_poll(ic, ctrl, p, n, t);
            }
            break;
        case IC_FT_RESP:
            if (master)
            {
                master_on_resp(ic, (uint8_t)si, ctrl, p, n);
            }
            break;
        case IC_FT_DATA:
            if (!master)
            {
                slave_on_data(ic, (uint8_t)si, ctrl, p, n);
            }
            break;
        default: /* IC_FT_ACK */
            if (!master)
            {
                slave_on_ack(ic, (uint8_t)si, ctrl, t);
            }
            break;
    }
}

/* ======================================================================== */
/* Receiver                                                                  */
/* ======================================================================== */
static void parser_byte(ic_t *ic, uint8_t b, uint32_t t)
{
    if ((ic->pState != IC_P_SYNC) && ((uint32_t)(t - ic->pLastMs) > ic->cfg.interByteTimeoutMs))
    {
        ic->stats.rxTimeouts++;
        ic->pState = IC_P_SYNC;
    }
    ic->pLastMs = t;

    switch (ic->pState)
    {
        case IC_P_SYNC:
            if (b == IC_SYNC)
            {
                ic->pIdx   = 0u;
                ic->pState = IC_P_HDR;
            }
            break;

        case IC_P_HDR:
            ic->pFrame[ic->pIdx++] = b;
            if (ic->pIdx == 4u)
            {
                if (b > INTERCOM_MAX_PAYLOAD)
                {
                    ic->stats.crcErrors++;
                    ic->pState = IC_P_SYNC;
                }
                else
                {
                    ic->pState = (b == 0u) ? IC_P_CRC : IC_P_PAYLOAD;
                }
            }
            break;

        case IC_P_PAYLOAD:
            ic->pFrame[ic->pIdx++] = b;
            if (ic->pIdx == (uint16_t)(4u + ic->pFrame[3]))
            {
                ic->pState = IC_P_CRC;
            }
            break;

        default: /* IC_P_CRC */
            ic->pFrame[ic->pIdx++] = b;
            if (ic->pIdx == (uint16_t)(6u + ic->pFrame[3]))
            {
                uint16_t n  = (uint16_t)(4u + ic->pFrame[3]);
                uint16_t rx = (uint16_t)(((uint16_t)ic->pFrame[n] << 8) | ic->pFrame[n + 1u]);
                ic->pState  = IC_P_SYNC;
                if (ic_crc16(ic->pFrame, n) == rx)
                {
                    ic->stats.rxFrames++;
                    handle_frame(ic, t);
                }
                else
                {
                    ic->stats.crcErrors++;
                }
            }
            break;
    }
}

static void rx_drain(ic_t *ic, uint32_t t)
{
    uint16_t tail = ic->rxTail;
    while (tail != ic->rxHead)
    {
        uint8_t b = ic->rxBuf[tail];
        tail       = (uint16_t)((tail + 1u) & IC_RX_MASK);
        ic->rxTail = tail;

        if ((ic->txState == IC_TX_BUSY) && ic->cfg.rxEcho && (ic->echoIdx < ic->txLen))
        {
            if (b != ic->txPtr[ic->echoIdx])
            {
                ic->echoErr = true;
            }
            ic->echoIdx++;
            if (ic->echoIdx >= ic->txLen)
            {
                tx_finish(ic, t);
            }
            continue;
        }
        ic->lastRxMs = t;
        parser_byte(ic, b, t);
    }
}

/* ======================================================================== */
/* Housekeeping: release acknowledged events, expire old ones               */
/* ======================================================================== */
static void q_reclaim(ic_t *ic, ic_queue_t *q)
{
    lock(ic);
    uint16_t h0   = q->head;
    uint16_t tail = q->tail;
    unlock(ic);

    uint16_t off = ic_q_first(q, h0, tail);
    while (off != tail)
    {
        const ic_rec_t *rec = ic_q_rec(q, off);
        if (((rec->flags & IC_REC_VALID) == 0u) || (rec->pending != 0u) || (rec->inflight != 0u))
        {
            break;
        }
        off = ic_q_next(q, off, tail);
    }
    if (off == h0)
    {
        return;
    }
    lock(ic);
    if (q->head == h0)   /* producer may have reset an empty queue meanwhile */
    {
        q->head = off;
        if (q->head == q->tail)
        {
            q->head = 0u;
            q->tail = 0u;
        }
    }
    unlock(ic);
}

static void expire_events(ic_t *ic, uint32_t t)
{
    if (ic->cfg.eventTtlMs == 0u)
    {
        return;
    }
    for (uint8_t qi = 0; qi < 2u; qi++)
    {
        ic_queue_t *q = &ic->q[qi];
        lock(ic);
        uint16_t head = q->head;
        uint16_t tail = q->tail;
        unlock(ic);
        for (uint16_t off = ic_q_first(q, head, tail); off != tail; off = ic_q_next(q, off, tail))
        {
            ic_rec_t *rec = ic_q_rec(q, off);
            if ((rec->flags & IC_REC_VALID) == 0u)
            {
                break;
            }
            if ((rec->pending == 0u) || ((uint32_t)(t - rec->stamp) < ic->cfg.eventTtlMs))
            {
                continue;
            }
            uint16_t  ref    = IC_REF(qi, off >> 2);
            ic_mask_t missed = rec->pending;
            for (uint8_t p = 0; p < ic->nodeCount; p++)
            {
                if ((rec->inflight & node_bit(p)) != 0u)
                {
                    chan_drop_ref(ic, p, ref);
                }
            }
            rec->pending  = 0u;
            rec->inflight = 0u;
            ic->stats.eventsDropped++;
            if (ic->cfg.onDrop != NULL)
            {
                ic->cfg.onDrop(rec->msgId, rec->dst, (uint32_t)missed, ic->cfg.ctx);
            }
        }
    }
}

static void housekeeping(ic_t *ic, uint32_t t)
{
    if ((uint32_t)(t - ic->lastHousekeepMs) >= IC_HOUSEKEEP_PERIOD_MS)
    {
        ic->lastHousekeepMs = t;
        expire_events(ic, t);
    }
    q_reclaim(ic, &ic->q[IC_REF_HIGH]);
    q_reclaim(ic, &ic->q[IC_REF_NORMAL]);
}

/* ======================================================================== */
/* Public (instance) API                                                     */
/* ======================================================================== */
void ic_process(ic_t *ic)
{
    if ((ic == NULL) || !ic->initialized)
    {
        return;
    }
    uint32_t t = now_ms(ic);
    rx_drain(ic, t);
    tx_service(ic, t);
    link_service(ic, t);
    tx_service(ic, t);   /* start a frame requested just now, if the gap allows */
    housekeeping(ic, t);
}

Intercom_Status ic_send(ic_t *ic, uint8_t dst, uint8_t msgId, const void *data,
                        uint8_t len, Intercom_Prio prio)
{
    if ((ic == NULL) || !ic->initialized)
    {
        return INTERCOM_ERR_NOT_INIT;
    }
    if ((len > 0u) && (data == NULL))
    {
        return INTERCOM_ERR_PARAM;
    }
    const ic_msg_desc_t *d = find_desc(s_eventDesc, IC_EVENT_COUNT, msgId, NULL);
    if (d == NULL)
    {
        return INTERCOM_ERR_MSG_ID;
    }
    if (len > d->maxLen)
    {
        return INTERCOM_ERR_LEN;
    }

    ic_mask_t mask;
    if (dst == INTERCOM_ADDR_BROADCAST)
    {
        mask = ic->othersMask;
    }
    else
    {
        int i = node_index(ic, dst);
        if ((i < 0) || (i == (int)ic->selfIdx))
        {
            return INTERCOM_ERR_ADDR;
        }
        mask = node_bit((uint8_t)i);
    }

    ic_queue_t *q = &ic->q[(prio == INTERCOM_PRIO_HIGH) ? IC_REF_HIGH : IC_REF_NORMAL];
    uint16_t    off = 0u;
    ic_rec_t   *rec = NULL;

    lock(ic);
    if (ic_q_alloc(q, ic_q_rec_size(len), &off))
    {
        rec        = ic_q_rec(q, off);
        rec->flags = 0u;   /* not visible to the consumer yet */
    }
    unlock(ic);

    if (rec == NULL)
    {
        ic->stats.queueFull++;
        return INTERCOM_ERR_FULL;
    }

    rec->pending  = mask;
    rec->inflight = 0u;
    rec->stamp    = now_ms(ic);
    rec->msgId    = msgId;
    rec->len      = len;
    rec->dst      = dst;
    if (len > 0u)
    {
        memcpy(ic_q_data(rec), data, len);
    }

    lock(ic);
    rec->flags = IC_REC_VALID;
    unlock(ic);
    return INTERCOM_OK;
}

Intercom_Status ic_publish(ic_t *ic, uint8_t msgId, const void *data, uint8_t len)
{
    if ((ic == NULL) || !ic->initialized)
    {
        return INTERCOM_ERR_NOT_INIT;
    }
    if ((len > 0u) && (data == NULL))
    {
        return INTERCOM_ERR_PARAM;
    }
    int idx = 0;
    const ic_msg_desc_t *d = find_desc(s_stateDesc, IC_STATE_COUNT, msgId, &idx);
    if (d == NULL)
    {
        return INTERCOM_ERR_MSG_ID;
    }
    if (len > d->maxLen)
    {
        return INTERCOM_ERR_LEN;
    }
    lock(ic);
    if (len > 0u)
    {
        memcpy((uint8_t *)&ic->stData + d->offset, data, len);
    }
    ic->stLen[idx]   = len;
    ic->stDirty[idx] = ic->othersMask;
    unlock(ic);
    return INTERCOM_OK;
}

void ic_get_stats(const ic_t *ic, Intercom_Stats *out)
{
    if ((ic == NULL) || (out == NULL))
    {
        return;
    }
    *out                 = ic->stats;
    out->rxOverflows     = ic->rxOverflows;
    out->queueHighUsed   = ic_q_used(&ic->q[IC_REF_HIGH]);
    out->queueNormalUsed = ic_q_used(&ic->q[IC_REF_NORMAL]);
}
