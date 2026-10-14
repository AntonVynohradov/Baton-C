/**
 * @file  sim_test.c
 * @brief Host simulation of a single-wire UART bus with several Intercom nodes.
 *
 * The bus model sends bytes at the real 9600 bit/s byte time, delivers every
 * byte to every powered node (including the sender = echo), detects
 * overlapping transmissions (collisions) and can inject bit errors.
 *
 * Checks per scenario:
 *   - events delivered exactly once and in order per (src, dst, priority)
 *   - all events delivered after the traffic stops (unless dropped by TTL)
 *   - state values never go backwards
 *   - no bus collisions (the protocol must be collision free)
 *   - slot duration stays within the configured budget
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ic_internal.h"

#define MAXN       8
#define BYTE_US    (10u * 1000000u / 9600u)
#define STEP_US    50u

typedef struct
{
    ic_t     ic;
    int      idx;
    uint8_t  addr;
    int      alive;
    uint32_t procPhaseUs;
    /* UART TX model */
    uint8_t  tx[IC_FRAME_MAX];
    uint16_t txLen, txPos;
    uint64_t txNextUs;
    int      txActive;
} node_t;

typedef struct
{
    const char *name;
    int      nodes;            /* master + slaves */
    int      deadIdx;          /* -1 = all alive */
    double   ber;              /* probability of a corrupted byte */
    uint32_t genMs;            /* traffic generation time */
    uint32_t drainMs;
    uint32_t meanGapMs;        /* mean time between events per node */
    uint32_t ttlMs;
    uint8_t  retries;
    uint16_t cycleMs;
    int      expectDrops;      /* 1 = drops allowed/expected */
    int      noEcho;           /* 1 = RS-485 style: no echo, TX-done interrupt */
} scenario_t;

static node_t   g_n[MAXN];
static uint64_t g_us;
static int      g_nn;
static uint8_t  g_slaveAddrs[MAXN];
static uint32_t g_rng = 0x12345678u;

/* verification state */
static uint16_t g_uniSent[MAXN][MAXN][2], g_uniExp[MAXN][MAXN][2];
static uint16_t g_bcSent[MAXN][2], g_bcExp[MAXN][MAXN][2];
static uint32_t g_stPub[MAXN], g_stLast[MAXN][MAXN];
static int      g_errors, g_dups, g_gaps, g_drops, g_liveDrops, g_full;
static uint64_t g_pollStartUs;
static long     g_collisionSteps, g_delivered, g_stateUpdates;
static uint32_t g_lastPollMs, g_maxSlotMs, g_polls, g_pollFail;

static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static void fail(const char *msg, int a, int b, int c)
{
    if (g_errors < 10)
    {
        printf("    ERROR: %s (%d %d %d) at %llu ms\n", msg, a, b, c, (unsigned long long)(g_us / 1000u));
    }
    g_errors++;
}

/* ---- hooks ---- */
static uint32_t hk_time(void *ctx)
{
    (void)ctx;
    return (uint32_t)(g_us / 1000u);
}

static bool hk_tx(const uint8_t *d, uint16_t len, void *ctx)
{
    node_t *n = (node_t *)ctx;
    if (n->txActive)
    {
        fail("uartStartTx while busy", n->idx, 0, 0);
    }
    memcpy(n->tx, d, len);
    if (n->idx == 0)
    {
        g_pollStartUs = g_us;   /* master only sends POLL frames */
    }
    n->txLen    = len;
    n->txPos    = 0;
    n->txNextUs = g_us + BYTE_US;
    n->txActive = 1;
    return true;
}

static int idx_of(uint8_t addr)
{
    for (int i = 0; i < g_nn; i++)
    {
        if (g_n[i].addr == addr)
        {
            return i;
        }
    }
    return -1;
}

static uint8_t pattern(uint16_t seq, int i)
{
    return (uint8_t)(seq * 7u + (unsigned)i * 13u);
}

static void hk_msg(uint8_t src, uint8_t id, const uint8_t *d, uint8_t len, void *ctx)
{
    node_t *r = (node_t *)ctx;
    int     s = idx_of(src);
    g_delivered++;
    if (s < 0)
    {
        fail("unknown src", src, 0, 0);
        return;
    }
    if (id == INTERCOM_MSG_ST_COUNTER)
    {
        uint32_t v;
        memcpy(&v, d, 4);
        if (len != 4 || v < g_stLast[r->idx][s])
        {
            fail("state went backwards", r->idx, s, (int)v);
        }
        g_stLast[r->idx][s] = v;
        g_stateUpdates++;
        return;
    }
    if (id == INTERCOM_MSG_ST_SMALL || id == INTERCOM_MSG_EV_EMPTY)
    {
        return;
    }
    if (len < 4 || d[0] != s)
    {
        fail("bad event header", r->idx, s, len);
        return;
    }
    int      prio = d[1] & 1;
    uint16_t seq  = (uint16_t)(d[2] | (d[3] << 8));
    for (int i = 4; i < len; i++)
    {
        if (d[i] != pattern(seq, i))
        {
            fail("payload corrupted", r->idx, s, seq);
            return;
        }
    }
    uint16_t *exp = (id == INTERCOM_MSG_EV_UNI) ? &g_uniExp[r->idx][s][prio] : &g_bcExp[r->idx][s][prio];
    if (seq < *exp)
    {
        g_dups++;
        fail("duplicate / reordered", r->idx, s, seq);
        return;
    }
    if (seq > *exp)
    {
        g_gaps += seq - *exp;
    }
    *exp = (uint16_t)(seq + 1u);
}

static void hk_drop(uint8_t id, uint8_t dst, uint32_t missed, void *ctx)
{
    (void)id;
    (void)dst;
    (void)ctx;
    g_drops++;
    for (int i = 0; i < g_nn; i++)
    {
        if ((missed & (1u << i)) && g_n[i].alive)
        {
            g_liveDrops++;   /* node index == node mask bit in this test */
        }
    }
}

static void hk_poll(uint8_t slave, bool ok, uint32_t nack, void *ctx)
{
    (void)slave;
    (void)nack;
    (void)ctx;
    uint32_t t    = (uint32_t)(g_us / 1000u);
    uint32_t slot = (uint32_t)((g_us - g_pollStartUs) / 1000u);
    if (slot > g_maxSlotMs)
    {
        g_maxSlotMs = slot;
    }
    g_lastPollMs = t;
    g_polls++;
    if (!ok)
    {
        g_pollFail++;
    }
}

/* ---- bus ---- */
static void bus_step(const scenario_t *sc)
{
    int active = 0;
    for (int i = 0; i < g_nn; i++)
    {
        active += g_n[i].txActive;
    }
    if (active > 1)
    {
        g_collisionSteps++;
    }
    for (int i = 0; i < g_nn; i++)
    {
        node_t *n = &g_n[i];
        if (!n->txActive || g_us < n->txNextUs)
        {
            continue;
        }
        uint8_t b = n->tx[n->txPos];
        if (sc->ber > 0.0 && (double)(rnd() % 1000000u) < sc->ber * 1e6)
        {
            b ^= (uint8_t)(1u << (rnd() % 8u));
        }
        for (int k = 0; k < g_nn; k++)
        {
            if (g_n[k].alive && !(sc->noEcho && k == i))
            {
                ic_rx_byte_isr(&g_n[k].ic, b);
            }
        }
        n->txPos++;
        if (n->txPos >= n->txLen)
        {
            n->txActive = 0;
            if (sc->noEcho)
            {
                ic_tx_done_isr(&n->ic);
            }
        }
        else
        {
            n->txNextUs += BYTE_US;
        }
    }
}

static void gen_traffic(int s, const scenario_t *sc)
{
    node_t *n = &g_n[s];
    /* events */
    if (rnd() % (sc->meanGapMs * 1000u / STEP_US) == 0u)
    {
        int     prio = (int)(rnd() % 4u == 0u);
        uint8_t buf[24];
        if (rnd() % 5u == 0u)
        {
            uint16_t seq = g_bcSent[s][prio];
            uint8_t  len = (uint8_t)(4u + rnd() % 9u);
            buf[0] = (uint8_t)s; buf[1] = (uint8_t)prio; buf[2] = (uint8_t)seq; buf[3] = (uint8_t)(seq >> 8);
            for (int i = 4; i < len; i++) buf[i] = pattern(seq, i);
            Intercom_Status st = ic_send(&n->ic, INTERCOM_ADDR_BROADCAST, INTERCOM_MSG_EV_BCAST, buf, len,
                                         prio ? INTERCOM_PRIO_HIGH : INTERCOM_PRIO_NORMAL);
            if (st == INTERCOM_OK) g_bcSent[s][prio]++;
            else if (st == INTERCOM_ERR_FULL) g_full++;
            else fail("send bcast", s, st, 0);
        }
        else
        {
            int d;
            do { d = (int)(rnd() % (uint32_t)g_nn); } while (d == s);
            uint16_t seq = g_uniSent[s][d][prio];
            uint8_t  len = (uint8_t)(4u + rnd() % 21u);
            buf[0] = (uint8_t)s; buf[1] = (uint8_t)prio; buf[2] = (uint8_t)seq; buf[3] = (uint8_t)(seq >> 8);
            for (int i = 4; i < len; i++) buf[i] = pattern(seq, i);
            Intercom_Status st = ic_send(&n->ic, g_n[d].addr, INTERCOM_MSG_EV_UNI, buf, len,
                                         prio ? INTERCOM_PRIO_HIGH : INTERCOM_PRIO_NORMAL);
            if (st == INTERCOM_OK) g_uniSent[s][d][prio]++;
            else if (st == INTERCOM_ERR_FULL) g_full++;
            else fail("send uni", s, st, 0);
        }
    }
    /* state every ~100 ms */
    if (rnd() % (100000u / STEP_US) == 0u)
    {
        g_stPub[s]++;
        if (ic_publish(&n->ic, INTERCOM_MSG_ST_COUNTER, &g_stPub[s], 4) != INTERCOM_OK)
        {
            fail("publish", s, 0, 0);
        }
    }
}

static IntercomCfg make_cfg(int i, const scenario_t *sc)
{
    IntercomCfg c;
    ic_default_cfg(&c);
    c.role          = (i == 0) ? INTERCOM_ROLE_MASTER : INTERCOM_ROLE_SLAVE;
    c.ownAddr       = g_n[i].addr;
    c.masterAddr    = g_n[0].addr;
    c.slaveAddrs    = g_slaveAddrs;
    c.slaveCount    = (uint8_t)(g_nn - 1);
    c.eventTtlMs    = sc->ttlMs;
    c.maxRetries    = sc->retries;
    c.rxEcho        = !sc->noEcho;
    c.cyclePeriodMs = sc->cycleMs;
    c.getTimeMs     = hk_time;
    c.uartStartTx   = hk_tx;
    c.onMessage     = hk_msg;
    c.onDrop        = hk_drop;
    c.onPollDone    = (i == 0) ? hk_poll : NULL;
    c.ctx           = &g_n[i];
    return c;
}

static int run(const scenario_t *sc)
{
    memset(g_n, 0, sizeof(g_n));
    memset(g_uniSent, 0, sizeof(g_uniSent)); memset(g_uniExp, 0, sizeof(g_uniExp));
    memset(g_bcSent, 0, sizeof(g_bcSent));   memset(g_bcExp, 0, sizeof(g_bcExp));
    memset(g_stPub, 0, sizeof(g_stPub));     memset(g_stLast, 0, sizeof(g_stLast));
    g_errors = g_dups = g_gaps = g_drops = g_liveDrops = g_full = 0;
    g_collisionSteps = g_delivered = g_stateUpdates = 0;
    g_lastPollMs = g_maxSlotMs = g_polls = g_pollFail = 0;
    g_us = 1000000u;
    g_nn = sc->nodes;

    for (int i = 0; i < g_nn; i++)
    {
        g_n[i].idx         = i;
        g_n[i].addr        = (uint8_t)(i == 0 ? 0x01 : 0x10 + i);
        g_n[i].alive       = (i != sc->deadIdx);
        g_n[i].procPhaseUs = (uint32_t)(rnd() % 500u);
        if (i > 0) g_slaveAddrs[i - 1] = g_n[i].addr;
    }
    for (int i = 0; i < g_nn; i++)
    {
        IntercomCfg c = make_cfg(i, sc);
        if (ic_init(&g_n[i].ic, &c) != INTERCOM_OK)
        {
            printf("    init failed for node %d\n", i);
            return 1;
        }
    }

    uint64_t genEnd = g_us + (uint64_t)sc->genMs * 1000u;
    uint64_t end    = genEnd + (uint64_t)sc->drainMs * 1000u;
    while (g_us < end)
    {
        bus_step(sc);
        for (int i = 0; i < g_nn; i++)
        {
            if (!g_n[i].alive) continue;
            if (g_us < genEnd) gen_traffic(i, sc);
            if (((g_us + g_n[i].procPhaseUs) % 500u) < STEP_US) ic_process(&g_n[i].ic);
        }
        g_us += STEP_US;
    }

    /* completeness */
    long missing = 0, sent = 0;
    for (int s = 0; s < g_nn; s++)
    {
        if (!g_n[s].alive) continue;
        for (int p = 0; p < 2; p++)
        {
            for (int d = 0; d < g_nn; d++)
            {
                if (d == s || !g_n[d].alive) continue;
                sent    += g_uniSent[s][d][p] + g_bcSent[s][p];
                missing += (g_uniSent[s][d][p] - g_uniExp[d][s][p]) + (g_bcSent[s][p] - g_bcExp[d][s][p]);
            }
        }
    }
    /* states: final value must arrive */
    int stateStale = 0;
    for (int s = 0; s < g_nn; s++)
        for (int d = 0; d < g_nn; d++)
            if (s != d && g_n[s].alive && g_n[d].alive && g_stLast[d][s] != g_stPub[s]) stateStale++;

    Intercom_Stats st, sum;
    memset(&sum, 0, sizeof(sum));
    for (int i = 0; i < g_nn; i++)
    {
        ic_get_stats(&g_n[i].ic, &st);
        sum.crcErrors += st.crcErrors; sum.ackTimeouts += st.ackTimeouts; sum.retries += st.retries;
        sum.duplicates += st.duplicates; sum.badMessages += st.badMessages; sum.respTimeouts += st.respTimeouts;
        sum.txFrames += st.txFrames; sum.rxOverflows += st.rxOverflows; sum.txTimeouts += st.txTimeouts;
    }

    uint32_t slotLimit = 250u + 75u + 20u; /* slot + max POLL frame + gaps/process latency */
    printf("    sent %ld deliveries, delivered %ld msgs (%ld state), dups %d, gaps %d, missing %ld, drops %d (live nodes missed %d)\n",
           sent, g_delivered, g_stateUpdates, g_dups, g_gaps, missing, g_drops, g_liveDrops);
    printf("    queueFull %d, collisions %ld, crcErr %u, ackTO %u, retries %u, dupFiltered %u, respTO %u, badMsg %u, rxOvf %u, txTO %u\n",
           g_full, g_collisionSteps, sum.crcErrors, sum.ackTimeouts, sum.retries, sum.duplicates,
           sum.respTimeouts, sum.badMessages, sum.rxOverflows, sum.txTimeouts);
    printf("    polls %u (failed %u), max slot %u ms (limit %u), stale states %d\n",
           g_polls, g_pollFail, g_maxSlotMs, slotLimit, stateStale);

    int bad = g_errors;
    if (g_collisionSteps) { printf("    FAIL: bus collisions\n"); bad++; }
    if (g_maxSlotMs > slotLimit) { printf("    FAIL: slot too long\n"); bad++; }
    if (sum.badMessages || sum.rxOverflows || sum.txTimeouts) { printf("    FAIL: unexpected error counters\n"); bad++; }
    if (stateStale) { printf("    FAIL: final state not delivered\n"); bad++; }
    if (!sc->expectDrops)
    {
        if (g_drops || g_gaps || missing) { printf("    FAIL: messages lost\n"); bad++; }
    }
    else
    {
        /* no silent loss: every message a live node did not get was reported by
           onDrop (a report may also cover a message received whose ACK was lost) */
        if (g_gaps + missing > g_liveDrops) { printf("    FAIL: unreported loss to live nodes\n"); bad++; }
        if (!g_drops) { printf("    FAIL: expected TTL drops for dead node\n"); bad++; }
    }
    return bad;
}

/* ---- API / parameter tests ---- */
static void hk_crit_noop(void *ctx) { (void)ctx; }

static int api_tests(void)
{
    int bad = 0;
    scenario_t sc = { "api", 3, -1, 0, 0, 0, 100, 5000, 1, 0, 0, 0 };
    g_nn = 3;
    for (int i = 0; i < 3; i++) { g_n[i].idx = i; g_n[i].addr = (uint8_t)(i == 0 ? 1 : 0x10 + i); }
    g_slaveAddrs[0] = 0x11; g_slaveAddrs[1] = 0x12;
    ic_t *ic = &g_n[1].ic;
    IntercomCfg c = make_cfg(1, &sc);

#define EXPECT(cond) do { if (!(cond)) { printf("    FAIL line %d: %s\n", __LINE__, #cond); bad++; } } while (0)
    EXPECT(ic_send(ic, 0x01, INTERCOM_MSG_EV_UNI, "x", 1, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_NOT_INIT);
    IntercomCfg b = c; b.getTimeMs = NULL;         EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    b = c; b.ownAddr = 0x55;                       EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    b = c; b.role = INTERCOM_ROLE_MASTER;          EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    b = c; b.slotTimeMs = 100;                     EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    b = c; b.ackTimeoutMs = 5;                     EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    b = c; b.enterCritical = hk_crit_noop; EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    uint8_t dupAddrs[2] = { 0x11, 0x11 };
    b = c; b.slaveAddrs = dupAddrs;                EXPECT(ic_init(ic, &b) == INTERCOM_ERR_PARAM);
    EXPECT(ic_init(ic, &c) == INTERCOM_OK);

    uint8_t big[30] = { 0 };
    EXPECT(ic_send(ic, 0x01, 0x99, big, 1, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_MSG_ID);
    EXPECT(ic_send(ic, 0x01, INTERCOM_MSG_ST_COUNTER, big, 1, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_MSG_ID);
    EXPECT(ic_send(ic, 0x01, INTERCOM_MSG_EV_UNI, big, 25, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_LEN);
    EXPECT(ic_send(ic, 0x11, INTERCOM_MSG_EV_UNI, big, 1, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_ADDR);
    EXPECT(ic_send(ic, 0x77, INTERCOM_MSG_EV_UNI, big, 1, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_ADDR);
    EXPECT(ic_send(ic, 0x01, INTERCOM_MSG_EV_UNI, NULL, 3, INTERCOM_PRIO_NORMAL) == INTERCOM_ERR_PARAM);
    EXPECT(ic_send(ic, 0x01, INTERCOM_MSG_EV_EMPTY, NULL, 0, INTERCOM_PRIO_NORMAL) == INTERCOM_OK);
    EXPECT(ic_publish(ic, INTERCOM_MSG_EV_UNI, big, 1) == INTERCOM_ERR_MSG_ID);
    EXPECT(ic_publish(ic, INTERCOM_MSG_ST_COUNTER, big, 5) == INTERCOM_ERR_LEN);
    EXPECT(ic_publish(ic, INTERCOM_MSG_ST_COUNTER, big, 4) == INTERCOM_OK);

    /* fill the HIGH queue (256 B, 40 B per 24 B event) until full */
    int n = 0;
    while (ic_send(ic, 0x01, INTERCOM_MSG_EV_UNI, big, 24, INTERCOM_PRIO_HIGH) == INTERCOM_OK) n++;
    EXPECT(n == 6);
    Intercom_Stats st;
    ic_get_stats(ic, &st);
    EXPECT(st.queueFull == 1 && st.queueHighUsed == 240);
    return bad;
}

/* ---- queue fuzz: random alloc / FIFO release, check no overlap ---- */
static int queue_fuzz(void)
{
    static uint32_t mem[64];
    ic_queue_t q;
    ic_q_init(&q, mem, sizeof(mem));
    uint16_t offs[64], sizes[64];
    int head = 0, cnt = 0, bad = 0;
    for (int it = 0; it < 200000; it++)
    {
        if ((rnd() & 1u) && cnt < 64)
        {
            uint8_t  len  = (uint8_t)(rnd() % 40u);
            uint16_t need = ic_q_rec_size(len), off;
            if (ic_q_alloc(&q, need, &off))
            {
                if (off + need > q.size) { bad++; break; }
                for (int k = 0; k < cnt; k++)
                {
                    int j = (head + k) % 64;
                    if (off < offs[j] + sizes[j] && offs[j] < off + need) { bad++; }
                }
                ic_rec_t *r = ic_q_rec(&q, off);
                memset(r, 0, sizeof(*r));
                r->len = len; r->flags = IC_REC_VALID;
                int j = (head + cnt) % 64;
                offs[j] = off; sizes[j] = need; cnt++;
            }
        }
        else if (cnt > 0)
        {
            /* consumer walks from head like q_reclaim */
            uint16_t first = ic_q_first(&q, q.head, q.tail);
            if (first != offs[head]) { bad++; break; }
            q.head = ic_q_next(&q, first, q.tail);
            if (q.head == q.tail) { q.head = 0; q.tail = 0; }
            head = (head + 1) % 64; cnt--;
        }
    }
    if (bad) printf("    FAIL: queue fuzz (%d)\n", bad);
    return bad;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        g_rng = (uint32_t)strtoul(argv[1], NULL, 0) | 1u;
    }
    static const scenario_t sc[] = {
        /* name,                        nodes dead  ber     genMs  drain  gap  ttl    rt cyc drops echo */
        { "clean bus, 1M+3S",            4,   -1,  0.0,    60000, 15000, 150, 30000, 1, 0, 0, 0 },
        { "noisy bus (0.3% bytes)",      4,   -1,  0.003,  60000, 20000, 150, 60000, 1, 0, 0, 0 },
        { "7 nodes, noise, cycle 1s",    7,   -1,  0.001,  60000, 30000, 400, 60000, 2, 1000, 0, 0 },
        { "overload (queue full)",       4,   -1,  0.0,    20000, 60000,  15, 120000, 1, 0, 0, 0 },
        { "dead slave, TTL 2 s",         4,    3,  0.0,    30000, 15000, 150, 2000,  1, 0, 1, 0 },
        { "dead slave + noise",          5,    2,  0.002,  30000, 20000, 200, 3000,  1, 0, 1, 0 },
        { "no echo (RS-485), noise",     4,   -1,  0.002,  40000, 20000, 150, 60000, 1, 0, 0, 1 },
        { "very noisy bus (1% bytes)",   4,   -1,  0.01,   40000, 60000, 300, 120000, 2, 0, 0, 0 },
    };
    int total = 0;
    printf("api tests\n");
    total += api_tests();
    printf("queue fuzz\n");
    total += queue_fuzz();
    for (size_t i = 0; i < sizeof(sc) / sizeof(sc[0]); i++)
    {
        printf("scenario: %s\n", sc[i].name);
        int r = run(&sc[i]);
        printf("    -> %s\n", r ? "FAIL" : "ok");
        total += r;
    }
    printf("\n%s (%d problems)\n", total ? "TESTS FAILED" : "ALL TESTS PASSED", total);
    return total ? 1 : 0;
}
