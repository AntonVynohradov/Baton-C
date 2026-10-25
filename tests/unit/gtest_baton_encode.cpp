/**
 * @file gtest_baton_encode.cpp
 * @brief Unit tests of the frame encoder.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <cstring>

#include "baton_test_support.h"

using baton_test::Bytes;

TEST(Encode, ReferenceFrames)
{
    EXPECT_EQ(baton_test::frame(0x11U, 0x01U, 0x20U, Bytes{0x00U}),
              (Bytes{0xA5U, 0x11U, 0x01U, 0x20U, 0x01U, 0x00U, 0x7CU, 0x44U}));
    EXPECT_EQ(baton_test::frame(0x12U, 0x11U, 0xC3U, Bytes{}),
              (Bytes{0xA5U, 0x12U, 0x11U, 0xC3U, 0x00U, 0x45U, 0x5BU}));
    EXPECT_EQ(baton_test::frame(0x01U, 0x11U, 0x60U, Bytes{0x80U, 0x00U}),
              (Bytes{0xA5U, 0x01U, 0x11U, 0x60U, 0x02U, 0x80U, 0x00U, 0xC9U, 0x91U}));
    EXPECT_EQ(baton_test::frame(0x12U, 0x11U, 0xA0U, Bytes{0x10U, 0x02U, 0xABU, 0xCDU}),
              (Bytes{0xA5U, 0x12U, 0x11U, 0xA0U, 0x04U, 0x10U, 0x02U, 0xABU, 0xCDU, 0xD8U, 0xBAU}));
}

TEST(Encode, CapacityExactFitAndOneByteShort)
{
    const uint8_t payload[3] = {1U, 2U, 3U};
    uint8_t out[10];
    size_t length = 0U;
    EXPECT_EQ(baton_encode(0x11U, 0x01U, 0x20U, payload, 3U, out, 10U, &length), 0);
    EXPECT_EQ(length, 10U);
    EXPECT_EQ(baton_encode(0x11U, 0x01U, 0x20U, payload, 3U, out, 9U, &length), BATON_PARSE_ERROR);
}

TEST(Encode, InvalidArguments)
{
    uint8_t out[BATON_FRAME_MAX + 1U];
    uint8_t payload[BATON_MAX_PAYLOAD + 1U] = {0U};
    size_t length = 0U;
    EXPECT_EQ(baton_encode(1U, 2U, 3U, payload, 1U, nullptr, sizeof(out), &length),
              BATON_PARSE_ERROR);
    EXPECT_EQ(baton_encode(1U, 2U, 3U, payload, 1U, out, sizeof(out), nullptr), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_encode(1U, 2U, 3U, nullptr, 1U, out, sizeof(out), &length), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_encode(1U, 2U, 3U, payload, BATON_MAX_PAYLOAD + 1U, out, sizeof(out), &length),
              BATON_PARSE_ERROR);
    EXPECT_EQ(baton_encode(1U, 2U, 3U, nullptr, 0U, out, sizeof(out), &length), 0);
    EXPECT_EQ(length, BATON_FRAME_OVERHEAD);
}

TEST(Encode, InPlacePayloadIsNotCopied)
{
    uint8_t buffer[BATON_FRAME_MAX];
    size_t length = 0U;
    buffer[BATON_HEADER_SIZE] = 0x10U;
    buffer[BATON_HEADER_SIZE + 1U] = 0x02U;
    buffer[BATON_HEADER_SIZE + 2U] = 0xABU;
    buffer[BATON_HEADER_SIZE + 3U] = 0xCDU;
    EXPECT_EQ(baton_encode(0x12U, 0x11U, 0xA0U, &buffer[BATON_HEADER_SIZE], 4U, buffer,
                           sizeof(buffer), &length),
              0);
    EXPECT_EQ(Bytes(buffer, buffer + length),
              (Bytes{0xA5U, 0x12U, 0x11U, 0xA0U, 0x04U, 0x10U, 0x02U, 0xABU, 0xCDU, 0xD8U, 0xBAU}));
}

TEST(Encode, EncodeParseRoundTripAllLengths)
{
    for (size_t n = 0U; n <= BATON_MAX_PAYLOAD; ++n)
    {
        Bytes payload(n);
        for (size_t i = 0U; i < n; ++i)
        {
            payload[i] = static_cast<uint8_t>(i * 37U + n);
        }
        const Bytes f = baton_test::frame(0x13U, 0x12U, 0x81U, payload);
        ASSERT_EQ(f.size(), n + BATON_FRAME_OVERHEAD);
        baton_parser parser;
        baton_parser_init(&parser);
        ASSERT_EQ(baton_test::feed_all(&parser, f), 1);
        EXPECT_EQ(parser.frame.length, n);
        EXPECT_EQ(std::memcmp(parser.frame.payload, payload.data(), n), 0);
    }
}
