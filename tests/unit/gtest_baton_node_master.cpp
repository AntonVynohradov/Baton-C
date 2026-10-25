/**
 * @file gtest_baton_node_master.cpp
 * @brief Unit tests of the master state machine: poll schedule, RESP handling, timeouts.
 *
 * The node under test is the master 0x01; the RESP frames of the slaves 0x11, 0x12 and
 * 0x13 (indexes 1..3) are injected.
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

const uint8_t kMaster = 0x01U;

class Master : public ::testing::Test
{
  protected:
    void SetUp() override { ASSERT_EQ(h.init(), BATON_OK); }

    /** Wait for the next POLL, check its destination, echo it and return it. */
    Bytes expect_poll(uint8_t slave, uint32_t limit_ms = 1000U)
    {
        EXPECT_TRUE(h.run_until_write(limit_ms));
        const Bytes f = h.last();
        EXPECT_GE(f.size(), BATON_FRAME_OVERHEAD + 1U);
        if (f.size() >= BATON_FRAME_OVERHEAD)
        {
            EXPECT_EQ(f[1], slave);
            EXPECT_EQ(f[2], kMaster);
            EXPECT_EQ(baton_ctrl_type(f[3]), BATON_FT_POLL);
        }
        h.echo_last();
        return f;
    }

    /** RESP from @p slave: ACKB, NACK mask, messages. */
    void resp(uint8_t slave, uint8_t seq, bool first, uint8_t ackb, uint8_t nack = 0U,
              const Bytes& content = Bytes{})
    {
        h.inject(frame(kMaster, slave, baton_ctrl(BATON_FT_RESP, first, seq),
                       cat(Bytes{ackb, nack}, content)),
                 1U);
    }

    baton_stats stats()
    {
        baton_stats s{};
        baton_node_get_stats(&h.node, &s);
        return s;
    }

    Harness h{kMaster};
};

Bytes payload_of(const Bytes& f)
{
    return Bytes(f.begin() + BATON_HEADER_SIZE, f.end() - BATON_CRC_SIZE);
}

}  // namespace

TEST_F(Master, PollsTheSlavesInOrder)
{
    Bytes f = expect_poll(0x11U);
    EXPECT_EQ(payload_of(f), (Bytes{0x00U})); /* no RESP acknowledged yet */
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_POLL, true, 0U));
    resp(0x11U, 0U, true, 0x80U);
    expect_poll(0x12U);
    resp(0x12U, 0U, true, 0x80U, 0x08U);
    expect_poll(0x13U);
    resp(0x13U, 0U, true, 0x80U);
    expect_poll(0x11U); /* next cycle */
    ASSERT_EQ(h.polls.size(), 3U);
    EXPECT_EQ(h.polls[0].slave, 0x11U);
    EXPECT_TRUE(h.polls[0].responded);
    EXPECT_EQ(h.polls[1].nack, 0x08U);
    EXPECT_EQ(h.node.cycle_count, 2U);
}

TEST_F(Master, RespWithContentIsDeliveredAndAcknowledgedInTheNextPoll)
{
    expect_poll(0x11U);
    resp(0x11U, 3U, true, 0x80U, 0U, tlv(BATON_MSG_EV_UNI, Bytes{1U, 2U}));
    ASSERT_EQ(h.messages.size(), 1U);
    EXPECT_EQ(h.messages[0].src, 0x11U);
    expect_poll(0x12U);
    resp(0x12U, 0U, true, 0x80U);
    expect_poll(0x13U);
    resp(0x13U, 0U, true, 0x80U);
    Bytes f = expect_poll(0x11U);
    EXPECT_EQ(payload_of(f), (Bytes{0x83U})); /* ACKB valid | seq 3 */

    /* an empty RESP does not change the acknowledged sequence number */
    resp(0x11U, 4U, false, 0x80U);
    expect_poll(0x12U);
    resp(0x12U, 0U, true, 0x80U);
    expect_poll(0x13U);
    resp(0x13U, 0U, true, 0x80U);
    f = expect_poll(0x11U);
    EXPECT_EQ(payload_of(f), (Bytes{0x83U}));
}

TEST_F(Master, EventToASlaveTravelsInThePollAndIsReleasedByTheResp)
{
    const uint8_t d[2] = {7U, 8U};
    ASSERT_EQ(baton_node_send(&h.node, 0x11U, BATON_MSG_EV_UNI, d, 2U, BATON_PRIO_NORMAL),
              BATON_OK);
    Bytes f = expect_poll(0x11U);
    EXPECT_EQ(payload_of(f), cat(Bytes{0x00U}, tlv(BATON_MSG_EV_UNI, Bytes{7U, 8U})));
    resp(0x11U, 0U, true, 0x81U); /* wrong seq: not confirmed */
    expect_poll(0x12U);
    resp(0x12U, 0U, true, 0x80U);
    expect_poll(0x13U);
    resp(0x13U, 0U, true, 0x80U);
    f = expect_poll(0x11U); /* retransmission, same seq */
    EXPECT_EQ(f[3], baton_ctrl(BATON_FT_POLL, true, 0U));
    EXPECT_EQ(payload_of(f), cat(Bytes{0x00U}, tlv(BATON_MSG_EV_UNI, Bytes{7U, 8U})));
    resp(0x11U, 0U, true, 0x00U); /* ACKB not valid: not confirmed */
    expect_poll(0x12U);
    resp(0x12U, 0U, true, 0x80U);
    expect_poll(0x13U);
    resp(0x13U, 0U, true, 0x80U);
    expect_poll(0x11U);
    resp(0x11U, 0U, true, 0x80U); /* confirmed */
    h.process(1U);
    EXPECT_EQ(stats().queue_normal_used, 0U);
    EXPECT_FALSE(h.node.channel[1].tx_first);
}

TEST_F(Master, EmptyRespWhileNothingIsOutstanding)
{
    expect_poll(0x11U);
    resp(0x11U, 0U, true, 0x80U); /* ACKB matches seq 0, but nothing is in flight */
    EXPECT_TRUE(h.node.channel[1].tx_first);
}

TEST_F(Master, ShortRespIsABadMessage)
{
    expect_poll(0x11U);
    h.inject(frame(kMaster, 0x11U, baton_ctrl(BATON_FT_RESP, true, 0U), Bytes{0x80U}), 1U);
    EXPECT_EQ(stats().bad_messages, 1U);
    EXPECT_EQ(h.node.link_state, 6U); /* still waiting */
}

TEST_F(Master, RespFromTheWrongSlaveOrOutOfTurnIsIgnored)
{
    resp(0x11U, 0U, true, 0x80U); /* not waiting yet */
    expect_poll(0x11U);
    resp(0x12U, 0U, true, 0x80U); /* not the polled slave */
    EXPECT_TRUE(h.polls.empty());
    /* other frame types are not for the master */
    h.inject(frame(kMaster, 0x11U, baton_ctrl(BATON_FT_POLL, true, 0U), Bytes{0U}), 1U);
    h.inject(frame(kMaster, 0x11U, baton_ctrl(BATON_FT_DATA, true, 0U), tlv(0x10U, Bytes{1U})), 1U);
    h.inject(frame(kMaster, 0x11U, baton_ctrl(BATON_FT_ACK, false, 0U), Bytes{}), 1U);
    EXPECT_TRUE(h.messages.empty());
    EXPECT_TRUE(h.polls.empty());
}

TEST_F(Master, MissingRespTimesOut)
{
    expect_poll(0x11U);
    const uint32_t start = h.now;
    expect_poll(0x12U, 400U);
    ASSERT_EQ(h.polls.size(), 1U);
    EXPECT_FALSE(h.polls[0].responded);
    EXPECT_EQ(h.now - start, 250U + 2U + 10U); /* slot + gap + margin; the bus is silent */
    EXPECT_EQ(stats().resp_timeouts, 1U);
    EXPECT_EQ(h.node.channel[1].fail_streak, 1U);
}

TEST_F(Master, RespTimeoutIsExtendedWhileAFrameIsBeingReceived)
{
    h.config.inter_byte_timeout_ms = 20U;
    ASSERT_EQ(h.init(), BATON_OK);
    expect_poll(0x11U);
    const uint32_t start = h.now;
    /* a slow maximum-size frame: one byte every 10 ms keeps the parser busy for 710 ms */
    const Bytes slow =
        frame(0x13U, 0x12U, baton_ctrl(BATON_FT_DATA, true, 0U), Bytes(BATON_MAX_PAYLOAD, 0x00U));
    size_t i = 0U;
    while ((h.now - start) < 400U)
    {
        h.inject(Bytes{slow[i]}, 10U);
        i++;
    }
    EXPECT_EQ(stats().resp_timeouts, 0U); /* slot + gap + margin = 262 ms have passed */
    while ((h.now - start) < 530U)
    {
        h.inject(Bytes{slow[i]}, 10U);
        i++;
    }
    EXPECT_EQ(stats().resp_timeouts, 1U); /* 2 x 262 ms even though the bus is busy */
}

TEST_F(Master, UnreachableSlaveIsPolledOnlyEveryFourthCycle)
{
    h.node.channel[2].fail_streak = BATON_DEAD_STREAK;
    std::vector<uint8_t> order;
    for (int i = 0; i < 12; ++i)
    {
        ASSERT_TRUE(h.run_until_write());
        const Bytes f = h.last();
        h.echo_last();
        order.push_back(f[1]);
        resp(f[1], 0U, true, 0x80U);
    }
    /* cycles 1..3 skip the unreachable slave, cycle 4 polls it; a RESP makes it healthy */
    const std::vector<uint8_t> expected{0x11U, 0x13U, 0x11U, 0x13U, 0x11U, 0x13U,
                                        0x11U, 0x12U, 0x13U, 0x11U, 0x12U, 0x13U};
    EXPECT_EQ(order, expected);
}

TEST_F(Master, FailStreakSaturates)
{
    h.node.channel[1].fail_streak = 255U;
    h.node.cycle_count = 3U; /* the next cycle (4) polls dead slaves */
    expect_poll(0x11U);
    expect_poll(0x12U, 400U);
    EXPECT_EQ(h.node.channel[1].fail_streak, 255U);
}

TEST_F(Master, CyclePeriodSpacesTheCycles)
{
    h.config.cycle_period_ms = 1000U;
    h.config.slave_count = 1U;
    ASSERT_EQ(h.init(), BATON_OK);
    expect_poll(0x11U);
    const uint32_t start = h.now;
    resp(0x11U, 0U, true, 0x80U);
    expect_poll(0x11U, 1200U);
    EXPECT_GE(h.now - start + 3U, 1000U);
}

TEST_F(Master, WriteFailureCountsAsATransmissionAndTheSlotTimesOut)
{
    h.writes.result = -1;
    ASSERT_TRUE(h.run_until_write());
    EXPECT_EQ(stats().tx_timeouts, 1U);
    EXPECT_EQ(h.node.link_state, 6U); /* waiting for the RESP of a POLL that was not sent */
    /* after the timeout the next POLL starts in the same call (the bus is silent) and fails */
    ASSERT_TRUE(h.run_until_write(400U));
    EXPECT_EQ(h.last()[1], 0x12U);
    EXPECT_EQ(stats().tx_timeouts, 2U);
    ASSERT_EQ(h.polls.size(), 1U);
    EXPECT_FALSE(h.polls[0].responded);
}

TEST_F(Master, WorksWithoutPollCallback)
{
    h.config.on_poll_done = nullptr;
    ASSERT_EQ(h.init(), BATON_OK);
    expect_poll(0x11U);
    resp(0x11U, 0U, true, 0x80U);
    expect_poll(0x12U);
    EXPECT_TRUE(h.polls.empty());
}
