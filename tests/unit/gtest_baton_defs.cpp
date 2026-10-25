/**
 * @file gtest_baton_defs.cpp
 * @brief Unit tests of the control-byte and byte-order helpers and the version macros.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <string>

#include "baton_test_support.h"

TEST(Defs, ControlByteRoundTrip)
{
    for (uint8_t type = 0U; type < 4U; ++type)
    {
        for (uint8_t seq = 0U; seq < 16U; ++seq)
        {
            for (int first = 0; first < 2; ++first)
            {
                const uint8_t ctrl = baton_ctrl(type, first != 0, seq);
                EXPECT_EQ(baton_ctrl_type(ctrl), type);
                EXPECT_EQ(baton_ctrl_seq(ctrl), seq);
                EXPECT_EQ(baton_ctrl_first(ctrl), first != 0);
                EXPECT_EQ(ctrl & 0x10U, 0U); /* reserved bit */
            }
        }
    }
}

TEST(Defs, ControlByteValues)
{
    EXPECT_EQ(baton_ctrl(BATON_FT_POLL, true, 0U), 0x20U);
    EXPECT_EQ(baton_ctrl(BATON_FT_ACK, false, 3U), 0xC3U);
    EXPECT_EQ(baton_ctrl(BATON_FT_DATA, true, 0x1FU), 0xAFU); /* seq is masked */
}

TEST(Defs, ByteOrderHelpers)
{
    uint8_t buffer[4] = {0U, 0U, 0U, 0U};
    baton_put_u16be(buffer, 0x7C44U);
    EXPECT_EQ(buffer[0], 0x7CU);
    EXPECT_EQ(buffer[1], 0x44U);
    EXPECT_EQ(baton_get_u16be(buffer), 0x7C44U);

    baton_put_u32le(buffer, 0x12345678U);
    EXPECT_EQ(buffer[0], 0x78U);
    EXPECT_EQ(buffer[3], 0x12U);
    EXPECT_EQ(baton_get_u32le(buffer), 0x12345678U);
}

TEST(Defs, VersionStringMatchesTheNumbers)
{
    const std::string expected = std::to_string(BATON_VERSION_MAJOR) + "."
                                 + std::to_string(BATON_VERSION_MINOR) + "."
                                 + std::to_string(BATON_VERSION_PATCH);
    EXPECT_EQ(expected, std::string(BATON_VERSION));
}

TEST(Defs, DerivedSizes)
{
    EXPECT_EQ(BATON_FRAME_OVERHEAD, 7U);
    EXPECT_EQ(BATON_FRAME_MAX, 7U + BATON_MAX_PAYLOAD);
    EXPECT_EQ(BATON_MASK_BYTES, 1U);
    EXPECT_EQ(BATON_MAX_MSG_LEN, BATON_MAX_PAYLOAD - 4U);
    EXPECT_EQ(BATON_STATE_COUNT, 2U);
    EXPECT_EQ(BATON_EVENT_COUNT, 3U);
    EXPECT_EQ(BATON_STATE_DATA_SIZE, 1U + 4U + 1U);
    EXPECT_EQ(BATON_MSG_EV_UNI, 0x10);
    EXPECT_EQ(BATON_MSG_EV_UNI_MAX_LEN, 24);
}
