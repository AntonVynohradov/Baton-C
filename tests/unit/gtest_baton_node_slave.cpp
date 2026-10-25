/**
 * @file gtest_baton_node_slave.cpp
 * @brief Unit tests of the slave state machine: slot, DATA/ACK, retries, duplicates, TTL.
 *
 * The node under test is slave 0x12 (index 2); the frames of the master 0x01 and of the
 * other slaves 0x11 and 0x13 are injected.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

using baton_test::Bytes;
using baton_test::cat;
using baton_test::frame;
using baton_test::Harness;
using baton_test::tlv;

namespace
{

const uint8_t kSelf = 0x12U;
const uint8_t kMaster = 0x01U;
const uint8_t kPeer = 0x13U;  /* index 3 */
const uint8_t kOther = 0x11U; /* index 1 */

class Slave : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ASSERT_EQ(h.init(), BATON_OK);
        h.process(10U);
    }

    /** POLL from the master with ACKB @p ackb and optional messages. */
    void poll(uint8_t seq = 0U, bool first = true, const Bytes& content = Bytes{},
              uint8_t ackb = 0x00U, uint32_t advance_ms = 1U)
    {
        h.inject(
            frame(kSelf, kMaster, baton_ctrl(BATON_FT_POLL, first, seq), cat(Bytes{ackb}, content)),
            advance_ms);
    }

    /** DATA from @p src to the node under test. */
    void data(uint8_t src, uint8_t seq, bool first, const Bytes& content)
    {
        h.inject(frame(kSelf, src, baton_ctrl(BATON_FT_DATA, first, seq), content), 1U);
    }

    void ack(uint8_t src, uint8_t seq)
    {
        h.inject(frame(kSelf, src, baton_ctrl(BATON_FT_ACK, false, seq), Bytes{}), 1U);
    }

    /** Wait for the next frame, check its destination and type, and echo it. */
    Bytes expect_tx(uint8_t dst, uint8_t type)
    {
        EXPECT_TRUE(h.run_until_write());
        const Bytes f = h.last();
        EXPECT_GE(f.size(), BATON_FRAME_OVERHEAD);
        if (f.size() >= BATON_FRAME_OVERHEAD)
        {
            EXPECT_EQ(f[1], dst);
            EXPECT_EQ(f[2], kSelf);
            EXPECT_EQ(baton_ctrl_type(f[3]), type);
        }
        h.echo_last();
        return f;
    }

    /** Wait until the slave answers the master with a RESP. */
    Bytes expect_resp() { return expect_tx(kMaster, BATON_FT_RESP); }

    int send(uint8_t dst, uint8_t id, const Bytes& d, uint8_t prio = BATON_PRIO_NORMAL)
    {
        return baton_node_send(&h.node, dst, id, d.empty() ? nullptr : d.data(),
                               static_cast<uint8_t>(d.size()), prio);
    }

    baton_stats stats()
    {
        baton_stats s{};
        baton_node_get_stats(&h.node, &s);
        return s;
    }

    Harness h{kSelf};
};

Bytes payload_of(const Bytes& f)
{
    return Bytes(f.begin() + BATON_HEADER_SIZE, f.end() - BATON_CRC_SIZE);
}

}  // namespace

TEST_F(Slave, AnswersAPollWithAnEmptyResp)
{
    poll(5U);
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x85U, 0x00U})); /* ACKB valid | poll seq, NACK mask 0 */
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_RESP, true, 0U));
    EXPECT_EQ(h.node.link_state, 0U);
}

TEST_F(Slave, AckbOfAPollWithNothingOutstanding)
{
    poll(0U, true, Bytes{}, 0x80U); /* valid, matches tx_seq 0, but nothing is in flight */
    expect_resp();
    EXPECT_TRUE(h.node.channel[0].tx_first);
    EXPECT_EQ(h.node.channel[0].tx_seq, 0U);
}

TEST_F(Slave, PollWithoutAckbIsABadMessage)
{
    h.inject(frame(kSelf, kMaster, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{}), 1U);
    EXPECT_FALSE(h.run_until_write(20U));
    EXPECT_EQ(stats().bad_messages, 1U);
}

TEST_F(Slave, FramesForOthersOrFromUnknownNodesAreIgnored)
{
    h.inject(frame(kPeer, kMaster, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{0U}), 1U);
    h.inject(frame(kSelf, 0x55U, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{0U}), 1U);
    h.inject(frame(kSelf, kSelf, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{0U}), 1U);
    h.inject(frame(kSelf, kPeer, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{0U}), 1U);
    h.inject(frame(kSelf, kMaster, baton_ctrl(BATON_FT_RESP, true, 0U), Bytes{0U, 0U}), 1U);
    EXPECT_FALSE(h.run_until_write(20U));
    EXPECT_EQ(stats().rx_frames, 5U);
}

TEST_F(Slave, MessagesInAPollAreDelivered)
{
    poll(0U, true, cat(tlv(BATON_MSG_EV_UNI, Bytes{1U, 2U}), tlv(BATON_MSG_ST_SMALL, Bytes{7U})));
    ASSERT_EQ(h.messages.size(), 2U);
    EXPECT_EQ(h.messages[0].src, kMaster);
    EXPECT_EQ(h.messages[0].id, BATON_MSG_EV_UNI);
    EXPECT_EQ(h.messages[0].data, (Bytes{1U, 2U}));
    EXPECT_EQ(h.messages[1].id, BATON_MSG_ST_SMALL);
    expect_resp();
}

TEST_F(Slave, EventToTheMasterTravelsInTheRespAndIsReleasedByTheNextPoll)
{
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, Bytes{9U, 8U, 7U}), BATON_OK);
    h.process(1U); /* stamp */
    poll(0U);
    Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f), cat(Bytes{0x80U, 0x00U}, tlv(BATON_MSG_EV_UNI, Bytes{9U, 8U, 7U})));
    EXPECT_GT(stats().queue_normal_used, 0U);

    /* lost acknowledgement: the next POLL does not confirm, the content is sent again */
    poll(1U, false, Bytes{}, 0x00U);
    f = expect_resp();
    EXPECT_EQ(payload_of(f), cat(Bytes{0x81U, 0x00U}, tlv(BATON_MSG_EV_UNI, Bytes{9U, 8U, 7U})));
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_RESP, true, 0U)); /* same seq */

    /* wrong sequence number in ACKB: not confirmed either */
    poll(2U, false, Bytes{}, 0x81U);
    expect_resp();
    EXPECT_GT(stats().queue_normal_used, 0U);

    /* confirmed */
    poll(3U, false, Bytes{}, 0x80U);
    f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x83U, 0x00U}));
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_RESP, false, 1U));
    h.process(1U);
    EXPECT_EQ(stats().queue_normal_used, 0U);
}

TEST_F(Slave, EventToAnotherSlaveIsSentAsDataAndCommittedByAck)
{
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    EXPECT_EQ(payload_of(f), tlv(BATON_MSG_EV_UNI, Bytes{1U}));
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_DATA, true, 0U));
    EXPECT_EQ(h.node.link_state, 2U); /* waiting for ACK */

    ack(kOther, 0U); /* wrong peer */
    ack(kPeer, 1U);  /* wrong seq */
    EXPECT_EQ(h.node.link_state, 2U);
    ack(kPeer, 0U);
    f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x80U, 0x00U}));
    h.process(1U);
    EXPECT_EQ(stats().queue_normal_used, 0U);
    EXPECT_FALSE(h.node.channel[3].tx_first);

    ack(kPeer, 0U); /* late duplicate ACK while idle: ignored */
    EXPECT_EQ(h.node.link_state, 0U);
}

TEST_F(Slave, BroadcastEventGoesToEveryOtherNode)
{
    ASSERT_EQ(send(BATON_ADDR_BROADCAST, BATON_MSG_EV_BCAST, Bytes{4U}), BATON_OK);
    poll(0U);
    /* round robin starts after the slot counter advanced: peers 0x13 then 0x11 */
    Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    ack(kPeer, baton_ctrl_seq(f[3]));
    f = expect_tx(kOther, BATON_FT_DATA);
    ack(kOther, baton_ctrl_seq(f[3]));
    f = expect_resp();
    EXPECT_EQ(payload_of(f), cat(Bytes{0x80U, 0x00U}, tlv(BATON_MSG_EV_BCAST, Bytes{4U})));
    poll(1U, false, Bytes{}, 0x80U);
    expect_resp();
    h.process(1U);
    EXPECT_EQ(stats().queue_normal_used, 0U);
}

TEST_F(Slave, AckTimeoutRetriesThenReportsTheNodeInTheResp)
{
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    Bytes first = expect_tx(kPeer, BATON_FT_DATA);
    Bytes retry = expect_tx(kPeer, BATON_FT_DATA); /* after ack_timeout_ms */
    EXPECT_EQ(retry, first);
    Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x80U, 0x08U})); /* NACK bit of node index 3 */
    const baton_stats s = stats();
    EXPECT_EQ(s.ack_timeouts, 2U);
    EXPECT_EQ(s.retries, 1U);
    EXPECT_EQ(h.node.channel[3].fail_streak, 1U);
}

TEST_F(Slave, AckTimeoutIsExtendedWhileAFrameIsBeingReceived)
{
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    const uint32_t start = h.now;
    /* keep the parser busy: one byte of a long frame every 4 ms */
    const Bytes slow =
        frame(kOther, kPeer, baton_ctrl(BATON_FT_DATA, true, 0U), Bytes(BATON_MAX_PAYLOAD, 0x00U));
    size_t i = 0U;
    while ((h.now - start) < 32U)
    {
        h.inject(Bytes{slow[i]}, 4U);
        i++;
    }
    EXPECT_EQ(stats().ack_timeouts, 0U); /* more than 20 ms elapsed, but the bus is busy */
    while ((h.now - start) < 44U)
    {
        h.inject(Bytes{slow[i]}, 4U);
        i++;
    }
    EXPECT_EQ(stats().ack_timeouts, 1U); /* 2 x ack_timeout_ms */
}

TEST_F(Slave, UnreachablePeerIsServedLastAndWithoutRetries)
{
    h.node.channel[3].fail_streak = BATON_DEAD_STREAK;
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(kOther, BATON_MSG_EV_UNI, Bytes{2U}), BATON_OK);
    poll(0U);
    Bytes f = expect_tx(kOther, BATON_FT_DATA); /* healthy peer first */
    ack(kOther, 0U);
    expect_tx(kPeer, BATON_FT_DATA); /* dead peer in pass 1 */
    f = expect_resp();               /* no retry */
    EXPECT_EQ(payload_of(f), (Bytes{0x80U, 0x08U}));
    EXPECT_EQ(stats().retries, 0U);
    EXPECT_EQ(h.node.channel[3].fail_streak, BATON_DEAD_STREAK + 1U);
}

TEST_F(Slave, FailStreakSaturates)
{
    h.node.channel[3].fail_streak = 255U;
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    expect_resp();
    EXPECT_EQ(h.node.channel[3].fail_streak, 255U);
}

TEST_F(Slave, PeerThatFailedInThisSlotIsNotServedAgain)
{
    h.config.max_retries = 0U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    expect_resp(); /* the peer has content, but it is in the NACK mask of this slot */
}

TEST_F(Slave, ReceivesDataAcknowledgesAndFiltersDuplicates)
{
    data(kPeer, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{5U}));
    Bytes f = expect_tx(kPeer, BATON_FT_ACK);
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_ACK, false, 0U));
    ASSERT_EQ(h.messages.size(), 1U);

    data(kPeer, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{5U})); /* retransmission */
    expect_tx(kPeer, BATON_FT_ACK);
    EXPECT_EQ(h.messages.size(), 1U);

    data(kPeer, 1U, false, tlv(BATON_MSG_EV_UNI, Bytes{6U}));
    expect_tx(kPeer, BATON_FT_ACK);
    data(kPeer, 1U, false, tlv(BATON_MSG_EV_UNI, Bytes{6U})); /* retransmission */
    expect_tx(kPeer, BATON_FT_ACK);
    data(kPeer, 2U, false, tlv(BATON_MSG_EV_UNI, Bytes{7U}));
    expect_tx(kPeer, BATON_FT_ACK);
    EXPECT_EQ(h.messages.size(), 3U);
    EXPECT_EQ(stats().duplicates, 2U);

    /* the peer restarted: FIRST frames are new unless the last one was FIRST with that seq */
    data(kPeer, 2U, true, tlv(BATON_MSG_EV_UNI, Bytes{8U}));
    expect_tx(kPeer, BATON_FT_ACK);
    data(kPeer, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{9U}));
    expect_tx(kPeer, BATON_FT_ACK);
    EXPECT_EQ(h.messages.size(), 5U);
    EXPECT_EQ(stats().duplicates, 2U);
}

TEST_F(Slave, DataFromTheMasterOrWhileBusyIsIgnored)
{
    data(kMaster, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{5U}));
    EXPECT_FALSE(h.run_until_write(10U));

    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA); /* now waiting for an ACK */
    data(kOther, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{5U}));
    EXPECT_TRUE(h.messages.empty());
}

TEST_F(Slave, EmptyDataIsAcknowledgedWithoutSequencing)
{
    data(kPeer, 3U, false, Bytes{});
    expect_tx(kPeer, BATON_FT_ACK);
    EXPECT_FALSE(h.node.channel[3].rx_valid);
}

TEST_F(Slave, MalformedMessagesAreCounted)
{
    /* unknown id, too long, truncated record, single trailing byte */
    data(kPeer, 0U, true,
         cat(cat(tlv(0x77U, Bytes{1U}), tlv(BATON_MSG_ST_SMALL, Bytes{1U, 2U})),
             cat(tlv(BATON_MSG_EV_EMPTY, Bytes{}), Bytes{0x10U, 0x05U, 0x00U})));
    expect_tx(kPeer, BATON_FT_ACK);
    data(kPeer, 1U, false, Bytes{0x10U});
    expect_tx(kPeer, BATON_FT_ACK);
    ASSERT_EQ(h.messages.size(), 1U); /* only the empty event */
    EXPECT_EQ(h.messages[0].id, BATON_MSG_EV_EMPTY);
    EXPECT_EQ(stats().bad_messages, 4U);
}

TEST_F(Slave, StatesAreSentToEveryNodeUntilAcknowledged)
{
    const uint32_t value = 0x01020304U;
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, &value, 4U), BATON_OK);
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, nullptr, 0U), BATON_OK);
    poll(0U);
    Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    EXPECT_EQ(payload_of(f), cat(tlv(BATON_MSG_ST_COUNTER, Bytes{4U, 3U, 2U, 1U}),
                                 tlv(BATON_MSG_ST_SMALL, Bytes{})));
    ack(kPeer, 0U);
    f = expect_tx(kOther, BATON_FT_DATA);
    ack(kOther, 0U);
    f = expect_resp();
    EXPECT_EQ(payload_of(f),
              cat(Bytes{0x80U, 0x00U}, cat(tlv(BATON_MSG_ST_COUNTER, Bytes{4U, 3U, 2U, 1U}),
                                           tlv(BATON_MSG_ST_SMALL, Bytes{}))));
    poll(1U, false, Bytes{}, 0x80U);
    f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x81U, 0x00U}));
    EXPECT_EQ(h.node.state_dirty[0], 0U);
}

TEST_F(Slave, RetransmissionCarriesTheLatestStateValue)
{
    uint8_t v = 1U;
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, &v, 1U), BATON_OK);
    poll(0U);
    Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    v = 2U;
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, &v, 1U), BATON_OK);
    f = expect_tx(kPeer, BATON_FT_DATA); /* retry after the ACK timeout, same seq */
    EXPECT_EQ(payload_of(f), tlv(BATON_MSG_ST_SMALL, Bytes{2U}));
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_DATA, true, 0U));
}

/** Serve the slot until the RESP: acknowledge every DATA frame. Returns the RESP. */
static Bytes serve_slot(Harness& h)
{
    Bytes f;
    do
    {
        EXPECT_TRUE(h.run_until_write());
        f = h.last();
        h.echo_last();
        if (f[1] != kMaster)
        {
            h.inject(
                frame(kSelf, f[1], baton_ctrl(BATON_FT_ACK, false, baton_ctrl_seq(f[3])), Bytes{}),
                1U);
        }
    } while (f[1] != kMaster);
    return f;
}

TEST_F(Slave, StateThatGrewAndNoLongerFitsIsSentLater)
{
    /* events of 58 bytes (26 + 26 + 6) and an empty state (2) fill 60 of the 62 bytes */
    const Bytes big(24U, 0xEEU);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, big), BATON_OK);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, big), BATON_OK);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, Bytes{1U, 2U, 3U, 4U}), BATON_OK);
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, nullptr, 0U), BATON_OK);
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, nullptr, 0U), BATON_OK);
    poll(0U);
    Bytes f = serve_slot(h);
    EXPECT_EQ(payload_of(f).size(), 2U + 58U + 2U + 2U);
    /* ST_COUNTER grows to 4 bytes (6) and the RESP is not confirmed: it no longer fits,
       ST_SMALL behind it still does */
    const uint32_t value = 7U;
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, &value, 4U), BATON_OK);
    poll(1U, false, Bytes{}, 0x00U);
    f = serve_slot(h);
    EXPECT_EQ(payload_of(f).size(), 2U + 58U + 2U);
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_RESP, true, 0U)); /* same seq: a retransmission */
    /* confirmed: the state follows in the next RESP */
    poll(2U, false, Bytes{}, 0x80U);
    f = serve_slot(h);
    EXPECT_EQ(payload_of(f),
              cat(Bytes{0x82U, 0x00U}, tlv(BATON_MSG_ST_COUNTER, Bytes{7U, 0U, 0U, 0U})));
}

TEST_F(Slave, StateOnlyRetryThatNoLongerFitsIsAbandoned)
{
    const uint32_t value = 7U;
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, &value, 4U), BATON_OK);
    h.node.state_dirty[0] = 0x08U; /* only for 0x13 */
    poll(0U);
    Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    const uint8_t seq = baton_ctrl_seq(f[3]);
    /* the ACK timeout is seen when only 3 bytes of budget are left: a TLV of 6 does not fit */
    h.now = h.node.slot_start + 137U;
    f = expect_resp();
    EXPECT_EQ(h.node.channel[3].inflight_count, 0U);
    EXPECT_NE(h.node.channel[3].tx_seq, seq); /* abandoned: the next content uses a new seq */
    EXPECT_EQ(h.node.state_dirty[0], 0x08U);  /* sent again later */
}

TEST_F(Slave, PackingStopsAtTheMessageLimit)
{
    /* the HIGH queue alone fills the frame (16 x 16 bytes = 256): NORMAL is not reached */
    for (uint32_t i = 0U; i < BATON_MAX_MSGS_PER_FRAME; ++i)
    {
        ASSERT_EQ(send(kMaster, BATON_MSG_EV_EMPTY, Bytes{}, BATON_PRIO_HIGH), BATON_OK);
    }
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_EMPTY, Bytes{}), BATON_OK);
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, nullptr, 0U), BATON_OK);
    h.node.state_dirty[1] = 0U; /* only one of the two states is dirty */
    h.node.state_dirty[0] = h.node.others_mask;
    poll(0U);
    Bytes f;
    do
    {
        ASSERT_TRUE(h.run_until_write());
        f = h.last();
        h.echo_last();
        if (f[1] != kMaster)
        {
            ack(f[1], baton_ctrl_seq(f[3]));
        }
    } while (f[1] != kMaster);
    EXPECT_EQ(payload_of(f).size(), 2U + (BATON_MAX_MSGS_PER_FRAME * 2U));
}

TEST_F(Slave, PackingStopsAtTheMessageLimitInsideAQueue)
{
    for (uint32_t i = 0U; i <= BATON_MAX_MSGS_PER_FRAME; ++i)
    {
        ASSERT_EQ(send(kMaster, BATON_MSG_EV_EMPTY, Bytes{}), BATON_OK);
    }
    poll(0U);
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f).size(), 2U + (BATON_MAX_MSGS_PER_FRAME * 2U));
}

TEST_F(Slave, FifoOrderStopsAtTheFirstEventThatDoesNotFit)
{
    const Bytes big(24U, 0x11U);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, big), BATON_OK);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, big), BATON_OK);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, big), BATON_OK);       /* does not fit */
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_EMPTY, Bytes{}), BATON_OK); /* would fit, but FIFO */
    poll(0U);
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f).size(), 2U + 52U);
}

TEST_F(Slave, HighPriorityEventsArePackedFirst)
{
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(kMaster, BATON_MSG_EV_UNI, Bytes{2U}, BATON_PRIO_HIGH), BATON_OK);
    poll(0U);
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f), cat(Bytes{0x80U, 0x00U}, cat(tlv(BATON_MSG_EV_UNI, Bytes{2U}),
                                                          tlv(BATON_MSG_EV_UNI, Bytes{1U}))));
}

TEST_F(Slave, RecordStillBeingWrittenIsNotPacked)
{
    uint16_t offset = 0U;
    ASSERT_TRUE(baton_queue_alloc(&h.node.queue[1], baton_queue_record_size(1U), &offset));
    baton_queue_set_flags(&h.node.queue[1], offset, 0U); /* producer interrupted */
    poll(0U);
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f), (Bytes{0x80U, 0x00U}));
    h.process(20U); /* the TTL scan and the reclaim stop at it as well */
    EXPECT_TRUE(h.drops.empty());
    EXPECT_GT(stats().queue_normal_used, 0U);
}

TEST_F(Slave, NoDataLateInTheSlot)
{
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(kOther, BATON_MSG_EV_UNI, Bytes{2U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    /* the ACK arrives when the rest of the slot is reserved for the RESP */
    h.now = h.node.slot_start + 149U;
    ack(kPeer, 0U);
    expect_resp();
    EXPECT_EQ(h.node.channel[1].inflight_count, 0U); /* nothing was sent to 0x11 */
}

TEST_F(Slave, RetryThatNoLongerFitsTheSlotGoesToTheResp)
{
    const Bytes big(24U, 0x22U);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, big), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, big), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA); /* 52 bytes */
    /* the ACK timeout is detected late: only a few bytes of budget are left */
    h.now += 125U;
    expect_resp();
    EXPECT_EQ(stats().retries, 1U);
    EXPECT_EQ(h.node.channel[3].inflight_count, 2U);
}

TEST_F(Slave, HighBaudRateCapsTheDataPayload)
{
    h.config.baudrate = 115200U;
    ASSERT_EQ(h.init(), BATON_OK);
    for (int i = 0; i < 4; ++i)
    {
        ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes(24U, 1U)), BATON_OK);
    }
    poll(0U);
    const Bytes f = expect_tx(kPeer, BATON_FT_DATA);
    EXPECT_EQ(payload_of(f).size(), 52U); /* 2 of 4 fit into BATON_MAX_PAYLOAD */
}

TEST_F(Slave, ATinyBudgetIsNotEnoughForAnyMessage)
{
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(kOther, BATON_MSG_EV_EMPTY, Bytes{}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    /* 1 ms of budget left: less than the frame overhead */
    h.now = h.node.slot_start + 146U;
    ack(kPeer, 0U);
    expect_resp();
    EXPECT_EQ(h.node.channel[1].inflight_count, 0U);
}

TEST_F(Slave, PollCancelsAPendingTransmission)
{
    poll(0U);
    poll(1U, true, Bytes{}, 0x00U, 0U); /* second POLL before the RESP could start */
    const Bytes f = expect_resp();
    EXPECT_EQ(payload_of(f)[0], 0x81U);
    EXPECT_EQ(h.writes.frames.size(), 1U);
}

TEST_F(Slave, ExpiredEventIsDroppedAndReported)
{
    h.config.event_ttl_ms = 100U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(BATON_ADDR_BROADCAST, BATON_MSG_EV_BCAST, Bytes{2U}), BATON_OK);
    ASSERT_EQ(send(kOther, BATON_MSG_EV_UNI, Bytes{3U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA); /* both events to 0x13 in flight */
    for (int i = 0; i < 120; ++i)
    {
        h.process(1U);
    }
    ASSERT_EQ(h.drops.size(), 3U);
    EXPECT_EQ(h.drops[0].id, BATON_MSG_EV_UNI);
    EXPECT_EQ(h.drops[0].dst, kPeer);
    EXPECT_EQ(h.drops[0].missed, 0x08U);
    EXPECT_EQ(h.drops[1].dst, BATON_ADDR_BROADCAST);
    EXPECT_EQ(h.drops[1].missed, 0x0BU); /* master, 0x11, 0x13 */
    EXPECT_EQ(h.node.channel[3].inflight_count, 0U);
    EXPECT_EQ(stats().events_dropped, 3U);
    EXPECT_EQ(stats().queue_normal_used, 0U);
}

TEST_F(Slave, EventExpiresWhileTheOtherOneInTheFrameStays)
{
    h.config.event_ttl_ms = 100U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    h.process(60U);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{2U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    for (int i = 0; i < 50; ++i)
    {
        h.process(1U);
    }
    ASSERT_EQ(h.drops.size(), 1U);
    EXPECT_EQ(h.node.channel[3].inflight_count, 1U);
}

TEST_F(Slave, AcknowledgedEventBehindAPendingOneIsNotExpired)
{
    h.config.event_ttl_ms = 100U;
    ASSERT_EQ(h.init(), BATON_OK);
    h.node.channel[3].fail_streak = BATON_DEAD_STREAK; /* 0x13 served last */
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    ASSERT_EQ(send(kOther, BATON_MSG_EV_UNI, Bytes{2U}), BATON_OK);
    poll(0U);
    expect_tx(kOther, BATON_FT_DATA);
    ack(kOther, 0U); /* second record acknowledged, first one still pending */
    for (int i = 0; i < 120; ++i)
    {
        h.process(1U);
    }
    ASSERT_EQ(h.drops.size(), 1U);
    EXPECT_EQ(h.drops[0].dst, kPeer);
}

TEST_F(Slave, ContentExpiredDuringTheAckWaitIsNotReported)
{
    h.config.event_ttl_ms = 15U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    const Bytes f = expect_resp(); /* expired before the ACK timeout */
    EXPECT_EQ(payload_of(f), (Bytes{0x80U, 0x00U}));
    EXPECT_EQ(h.node.channel[3].fail_streak, 0U);
}

TEST_F(Slave, AckForContentThatExpiredIsIgnored)
{
    h.config.event_ttl_ms = 12U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    poll(0U);
    expect_tx(kPeer, BATON_FT_DATA);
    for (int i = 0; i < 17; ++i) /* TTL scan every 10 ms; ACK timeout after 20 ms */
    {
        h.process(1U);
    }
    ASSERT_EQ(h.drops.size(), 1U);
    ack(kPeer, 0U); /* the content is gone and the channel moved on to seq 1 */
    ack(kPeer, 1U); /* a stray ACK with the new seq: nothing is outstanding */
    EXPECT_EQ(h.node.link_state, 2U);
    EXPECT_EQ(h.node.channel[3].tx_seq, 1U);
}

TEST_F(Slave, NoTtlKeepsEventsForever)
{
    h.config.event_ttl_ms = 0U;
    ASSERT_EQ(h.init(), BATON_OK);
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    for (int i = 0; i < 100; ++i)
    {
        h.process(100U);
    }
    EXPECT_TRUE(h.drops.empty());
}

TEST_F(Slave, TxDoneWithoutAnOutstandingTransmissionIsIgnored)
{
    h.config.rx_echo = false;
    ASSERT_EQ(h.init(), BATON_OK);
    poll(0U);
    ASSERT_TRUE(h.run_until_write());
    h.node.link_state = 0U; /* white box: the state machine no longer waits for it */
    baton_node_tx_done_isr(&h.node);
    h.process(1U);
    EXPECT_EQ(h.node.link_state, 0U);
}

TEST_F(Slave, WorksWithoutCallbacksAndCriticalSection)
{
    h.config.on_message = nullptr;
    h.config.on_drop = nullptr;
    h.config.enter_critical = nullptr;
    h.config.exit_critical = nullptr;
    h.config.event_ttl_ms = 50U;
    ASSERT_EQ(h.init(), BATON_OK);
    h.critical_calls = 0;
    ASSERT_EQ(send(kPeer, BATON_MSG_EV_UNI, Bytes{1U}), BATON_OK);
    data(kPeer, 0U, true, tlv(BATON_MSG_EV_UNI, Bytes{5U}));
    expect_tx(kPeer, BATON_FT_ACK);
    for (int i = 0; i < 60; ++i)
    {
        h.process(1U);
    }
    EXPECT_TRUE(h.messages.empty());
    EXPECT_TRUE(h.drops.empty());
    EXPECT_EQ(stats().events_dropped, 1U);
    EXPECT_EQ(h.critical_calls, 0);
}
