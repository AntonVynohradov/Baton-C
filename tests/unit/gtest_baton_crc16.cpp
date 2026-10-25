/**
 * @file gtest_baton_crc16.cpp
 * @brief Unit tests of the CRC-16/CCITT-FALSE implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

using baton_test::Bytes;

TEST(Crc16, CheckValueOfTheCatalogue)
{
    const Bytes check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(baton_crc16(check.data(), check.size()), 0x29B1U);
}

TEST(Crc16, EmptyInputAndNullReturnTheInitialValue)
{
    const uint8_t byte = 0x00U;
    EXPECT_EQ(baton_crc16(&byte, 0U), BATON_CRC_INIT);
    EXPECT_EQ(baton_crc16(nullptr, 10U), BATON_CRC_INIT);
}

TEST(Crc16, UpdateMatchesTheBufferFunction)
{
    const Bytes data{0x11U, 0x01U, 0x20U, 0x01U, 0x00U};
    uint16_t crc = BATON_CRC_INIT;
    for (uint8_t b : data)
    {
        crc = baton_crc16_update(crc, b);
    }
    EXPECT_EQ(crc, baton_crc16(data.data(), data.size()));
    EXPECT_EQ(crc, 0x7C44U); /* reference POLL frame, see docs/protocol.md */
}
