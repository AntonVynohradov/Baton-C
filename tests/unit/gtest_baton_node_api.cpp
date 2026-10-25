/**
 * @file gtest_baton_node_api.cpp
 * @brief Unit tests of baton_node_send(), baton_node_publish(), the interrupt entry points
 *        and baton_node_get_stats().
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <cstring>

#include "baton_test_support.h"

using baton_test::Bytes;
using baton_test::Harness;

namespace
{

class NodeApi : public ::testing::Test
{
  protected:
    void SetUp() override { ASSERT_EQ(h.init(), BATON_OK); }

    Harness h{0x12U};
    uint8_t data[32]{};
};

}  // namespace

TEST(NodeApiUninitialized, CallsBeforeInit)
{
    baton_node node;
    std::memset(&node, 0, sizeof(node));
    const uint8_t d = 0U;
    EXPECT_EQ(baton_node_send(&node, 0x01U, BATON_MSG_EV_UNI, &d, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_NOT_INIT);
    EXPECT_EQ(baton_node_publish(&node, BATON_MSG_ST_SMALL, &d, 1U), BATON_ERR_NOT_INIT);
    EXPECT_EQ(baton_node_send(nullptr, 0x01U, BATON_MSG_EV_UNI, &d, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_NOT_INIT);
    EXPECT_EQ(baton_node_publish(nullptr, BATON_MSG_ST_SMALL, &d, 1U), BATON_ERR_NOT_INIT);
    baton_node_process(&node, 0U);
    baton_node_process(nullptr, 0U);
    baton_node_rx_isr(nullptr, 0U);
    baton_node_tx_done_isr(nullptr);
    baton_stats s{};
    baton_node_get_stats(nullptr, &s);
    baton_node_get_stats(&node, nullptr);
}

TEST_F(NodeApi, SendValidatesItsArguments)
{
    EXPECT_EQ(baton_node_send(&h.node, 0x01U, BATON_MSG_EV_UNI, nullptr, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_PARAM);
    EXPECT_EQ(baton_node_send(&h.node, 0x01U, 0x99U, data, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_MSG_ID);
    EXPECT_EQ(baton_node_send(&h.node, 0x01U, BATON_MSG_ST_COUNTER, data, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_MSG_ID);
    EXPECT_EQ(baton_node_send(&h.node, 0x01U, BATON_MSG_EV_UNI, data, 25U, BATON_PRIO_NORMAL),
              BATON_ERR_LEN);
    EXPECT_EQ(baton_node_send(&h.node, 0x12U, BATON_MSG_EV_UNI, data, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_ADDR);
    EXPECT_EQ(baton_node_send(&h.node, 0x77U, BATON_MSG_EV_UNI, data, 1U, BATON_PRIO_NORMAL),
              BATON_ERR_ADDR);
    EXPECT_EQ(baton_node_send(&h.node, 0x01U, BATON_MSG_EV_EMPTY, nullptr, 0U, BATON_PRIO_NORMAL),
              BATON_OK);
    EXPECT_EQ(baton_node_send(&h.node, BATON_ADDR_BROADCAST, BATON_MSG_EV_BCAST, data, 12U,
                              BATON_PRIO_HIGH),
              BATON_OK);
}

TEST_F(NodeApi, SendCopiesTheDataAndUsesTheCriticalSection)
{
    data[0] = 0x42U;
    const int calls = h.critical_calls;
    ASSERT_EQ(baton_node_send(&h.node, 0x13U, BATON_MSG_EV_UNI, data, 3U, BATON_PRIO_NORMAL),
              BATON_OK);
    EXPECT_GT(h.critical_calls, calls);
    EXPECT_EQ(h.critical_depth, 0);
    data[0] = 0U;
    baton_record r{};
    baton_queue_read(&h.node.queue[1], 0U, &r);
    EXPECT_EQ(r.pending, 0x08U);
    EXPECT_EQ(r.inflight, 0U);
    EXPECT_EQ(r.stamp, h.now);
    EXPECT_EQ(r.msg_id, BATON_MSG_EV_UNI);
    EXPECT_EQ(r.length, 3U);
    EXPECT_EQ(r.dst, 0x13U);
    EXPECT_EQ(r.flags, BATON_REC_VALID);
    EXPECT_EQ(baton_queue_data(&h.node.queue[1], 0U)[0], 0x42U);
}

TEST_F(NodeApi, QueueFullIsReportedAndCounted)
{
    int sent = 0;
    while (baton_node_send(&h.node, 0x01U, BATON_MSG_EV_UNI, data, 24U, BATON_PRIO_HIGH)
           == BATON_OK)
    {
        sent++;
    }
    EXPECT_EQ(sent, 6); /* 256 bytes, 40 bytes per event */
    baton_stats s{};
    baton_node_get_stats(&h.node, &s);
    EXPECT_EQ(s.queue_full, 1U);
    EXPECT_EQ(s.queue_high_used, 240U);
    EXPECT_EQ(s.queue_normal_used, 0U);
}

TEST_F(NodeApi, PublishValidatesItsArguments)
{
    EXPECT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, nullptr, 1U), BATON_ERR_PARAM);
    EXPECT_EQ(baton_node_publish(&h.node, BATON_MSG_EV_UNI, data, 1U), BATON_ERR_MSG_ID);
    EXPECT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, data, 5U), BATON_ERR_LEN);
    EXPECT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, data, 4U), BATON_OK);
    EXPECT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, nullptr, 0U), BATON_OK);
}

TEST_F(NodeApi, PublishStoresTheLatestValue)
{
    const uint8_t a[4] = {1U, 2U, 3U, 4U};
    const uint8_t b[1] = {9U};
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_COUNTER, a, 4U), BATON_OK);
    ASSERT_EQ(baton_node_publish(&h.node, BATON_MSG_ST_SMALL, b, 1U), BATON_OK);
    EXPECT_EQ(h.node.state_length[0], 4U);
    EXPECT_EQ(h.node.state_length[1], 1U);
    EXPECT_EQ(h.node.state_data[0], 1U);
    EXPECT_EQ(h.node.state_data[4], 9U); /* ST_SMALL follows the 4 bytes of ST_COUNTER */
    EXPECT_EQ(h.node.state_dirty[0], h.node.others_mask);
}

TEST_F(NodeApi, StatsCombineLinkAndNodeCounters)
{
    const Bytes bad{0xA5U, 0x12U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U};
    h.inject(bad, 1U);
    for (uint32_t i = 0U; i < BATON_RX_BUF_SIZE; ++i)
    {
        baton_node_rx_isr(&h.node, 0x00U);
    }
    baton_stats s{};
    baton_node_get_stats(&h.node, &s);
    EXPECT_EQ(s.crc_errors, 1U);
    EXPECT_EQ(s.rx_overflows, 1U);
    EXPECT_EQ(s.rx_frames, 0U);
}
