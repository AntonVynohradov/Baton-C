/**
 * @file gtest_baton_node_init.cpp
 * @brief Unit tests of baton_node_default_config() and baton_node_init() validation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

using baton_test::Harness;

namespace
{

void noop(void* context)
{
    (void) context;
}

class NodeInit : public ::testing::Test
{
  protected:
    int init_with(const baton_node_config& c) { return baton_node_init(&h.node, &c, 0U); }

    Harness h{0x12U};
};

}  // namespace

TEST(NodeDefaults, DefaultConfiguration)
{
    baton_node_config c;
    baton_node_default_config(&c);
    EXPECT_EQ(c.role, BATON_ROLE_SLAVE);
    EXPECT_EQ(c.baudrate, 9600U);
    EXPECT_EQ(c.slot_time_ms, 250U);
    EXPECT_EQ(c.ack_timeout_ms, 20U);
    EXPECT_EQ(c.max_retries, 1U);
    EXPECT_EQ(c.tx_gap_ms, 2U);
    EXPECT_EQ(c.inter_byte_timeout_ms, 5U);
    EXPECT_EQ(c.cycle_period_ms, 0U);
    EXPECT_EQ(c.event_ttl_ms, 5000U);
    EXPECT_TRUE(c.rx_echo);
    EXPECT_EQ(c.write, nullptr);
    baton_node_default_config(nullptr); /* ignored */
}

TEST_F(NodeInit, ValidConfigurations)
{
    EXPECT_EQ(h.init(), BATON_OK);
    EXPECT_TRUE(h.node.initialized);
    EXPECT_EQ(h.node.node_count, 4U);
    EXPECT_EQ(h.node.self_index, 2U);
    EXPECT_EQ(h.node.others_mask, 0x0BU);

    Harness master(0x01U, 7U);
    const uint8_t slaves[7] = {2U, 3U, 4U, 5U, 6U, 7U, 8U};
    master.config.slave_addresses = slaves;
    EXPECT_EQ(master.init(), BATON_OK);
    EXPECT_EQ(master.node.others_mask, 0xFEU);
}

TEST_F(NodeInit, NullArguments)
{
    ASSERT_EQ(h.init(), BATON_OK);
    EXPECT_EQ(baton_node_init(&h.node, nullptr, 0U), BATON_ERR_PARAM);
    EXPECT_FALSE(h.node.initialized);
    EXPECT_EQ(baton_node_init(nullptr, &h.config, 0U), BATON_ERR_PARAM);
}

TEST_F(NodeInit, InvalidBasicParameters)
{
    baton_node_config c = h.config;
    c.write = nullptr;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.slave_addresses = nullptr;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.enter_critical = nullptr;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.exit_critical = nullptr;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.enter_critical = noop;
    c.exit_critical = noop;
    EXPECT_EQ(init_with(c), BATON_OK);
    c = h.config;
    c.slave_count = 0U;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.slave_count = BATON_MAX_NODES;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.baudrate = 0U;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.ack_timeout_ms = 0U;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.inter_byte_timeout_ms = 0U;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c = h.config;
    c.role = 2U;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
}

TEST_F(NodeInit, InvalidTopology)
{
    baton_node_config c = h.config;
    const uint8_t broadcast[3] = {0x11U, BATON_ADDR_BROADCAST, 0x12U};
    c.slave_addresses = broadcast;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);

    c = h.config;
    c.master_address = BATON_ADDR_BROADCAST;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);

    c = h.config;
    const uint8_t duplicate[3] = {0x11U, 0x12U, 0x11U};
    c.slave_addresses = duplicate;
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);

    c = h.config;
    c.own_address = 0x55U; /* not on the bus */
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);

    c = h.config;
    c.role = BATON_ROLE_MASTER; /* but the address is a slave */
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);

    c = h.config;
    c.own_address = 0x01U; /* the master address, but the role is slave */
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
}

TEST_F(NodeInit, InvalidTiming)
{
    baton_node_config c = h.config;
    c.ack_timeout_ms = 10U; /* ACK frame (8 ms) + gap (2 ms) */
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c.ack_timeout_ms = 11U;
    EXPECT_EQ(init_with(c), BATON_OK);

    c = h.config;
    c.slot_time_ms = 173U; /* minimum: 74 + 6 + 2 + 20 + 72 = 174 ms */
    EXPECT_EQ(init_with(c), BATON_ERR_PARAM);
    c.slot_time_ms = 174U;
    EXPECT_EQ(init_with(c), BATON_OK);
}

TEST_F(NodeInit, ReinitRestartsTheNode)
{
    ASSERT_EQ(h.init(), BATON_OK);
    const uint8_t d = 1U;
    ASSERT_EQ(baton_node_send(&h.node, 0x13U, BATON_MSG_EV_UNI, &d, 1U, BATON_PRIO_NORMAL),
              BATON_OK);
    ASSERT_EQ(h.init(), BATON_OK);
    baton_stats s{};
    baton_node_get_stats(&h.node, &s);
    EXPECT_EQ(s.queue_normal_used, 0U);
    EXPECT_TRUE(h.node.channel[3].tx_first);
}
