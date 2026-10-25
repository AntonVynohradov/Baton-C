/**
 * @file gtest_baton_parser.cpp
 * @brief Unit tests of the incremental frame parser.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

using baton_test::Bytes;
using baton_test::feed_all;

namespace
{

const Bytes kPoll{0xA5U, 0x11U, 0x01U, 0x20U, 0x01U, 0x00U, 0x7CU, 0x44U};
const Bytes kAck{0xA5U, 0x12U, 0x11U, 0xC3U, 0x00U, 0x45U, 0x5BU};
const Bytes kData{0xA5U, 0x12U, 0x11U, 0xA0U, 0x04U, 0x10U, 0x02U, 0xABU, 0xCDU, 0xD8U, 0xBAU};

class Parser : public ::testing::Test
{
  protected:
    void SetUp() override { baton_parser_init(&parser); }

    baton_parser parser{};
};

}  // namespace

TEST_F(Parser, ReferencePollFrame)
{
    for (size_t i = 0U; i + 1U < kPoll.size(); ++i)
    {
        EXPECT_EQ(baton_parser_feed(&parser, kPoll[i]), 0);
    }
    EXPECT_EQ(baton_parser_feed(&parser, kPoll.back()), 1);
    EXPECT_EQ(parser.frame.dst, 0x11U);
    EXPECT_EQ(parser.frame.src, 0x01U);
    EXPECT_EQ(parser.frame.ctrl, 0x20U);
    EXPECT_EQ(parser.frame.length, 1U);
    EXPECT_EQ(parser.frame.payload[0], 0x00U);
    EXPECT_EQ(parser.state, BATON_PARSER_IDLE);
}

TEST_F(Parser, FrameWithoutPayload)
{
    EXPECT_EQ(feed_all(&parser, kAck), 1);
    EXPECT_EQ(parser.frame.length, 0U);
    EXPECT_EQ(parser.frame.ctrl, 0xC3U);
}

TEST_F(Parser, ReferenceDataFrame)
{
    EXPECT_EQ(feed_all(&parser, kData), 1);
    EXPECT_EQ(parser.frame.length, 4U);
    EXPECT_EQ(parser.frame.payload[3], 0xCDU);
}

TEST_F(Parser, BytesBeforeSyncAreIgnored)
{
    const Bytes stream = baton_test::cat(Bytes{0x00U, 0xFFU, 0x13U}, kPoll);
    EXPECT_EQ(feed_all(&parser, stream), 1);
}

TEST_F(Parser, BackToBackFrames)
{
    EXPECT_EQ(feed_all(&parser, kPoll), 1);
    EXPECT_EQ(feed_all(&parser, kAck), 1);
    EXPECT_EQ(parser.frame.dst, 0x12U);
}

TEST_F(Parser, CrcErrorInHighAndLowByte)
{
    Bytes bad = kPoll;
    bad[bad.size() - 2U] ^= 0x01U;
    EXPECT_EQ(feed_all(&parser, bad), BATON_PARSE_ERROR);
    bad = kPoll;
    bad.back() ^= 0x80U;
    EXPECT_EQ(feed_all(&parser, bad), BATON_PARSE_ERROR);
    /* the parser resynchronizes on the next frame */
    EXPECT_EQ(feed_all(&parser, kPoll), 1);
}

TEST_F(Parser, CorruptedPayloadIsRejected)
{
    Bytes bad = kData;
    bad[6] ^= 0x40U;
    EXPECT_EQ(feed_all(&parser, bad), BATON_PARSE_ERROR);
}

TEST_F(Parser, LengthAboveMaximumIsRejectedImmediately)
{
    const Bytes header{0xA5U, 0x11U, 0x01U, 0x20U};
    EXPECT_EQ(feed_all(&parser, header), 0);
    EXPECT_EQ(baton_parser_feed(&parser, static_cast<uint8_t>(BATON_MAX_PAYLOAD + 1U)),
              BATON_PARSE_ERROR);
    EXPECT_EQ(parser.state, BATON_PARSER_IDLE);
}

TEST_F(Parser, MaximumLengthIsAccepted)
{
    Bytes payload(BATON_MAX_PAYLOAD, 0x5AU);
    const Bytes f = baton_test::frame(0x11U, 0x01U, 0x20U, payload);
    ASSERT_EQ(f.size(), BATON_FRAME_MAX);
    EXPECT_EQ(feed_all(&parser, f), 1);
    EXPECT_EQ(parser.frame.length, BATON_MAX_PAYLOAD);
}

TEST_F(Parser, NullParser)
{
    EXPECT_EQ(baton_parser_feed(nullptr, 0xA5U), BATON_PARSE_ERROR);
    baton_parser_init(nullptr); /* ignored */
}

TEST_F(Parser, CorruptedStateRecovers)
{
    parser.state = 0x7FU;
    EXPECT_EQ(baton_parser_feed(&parser, 0xA5U), BATON_PARSE_ERROR);
    EXPECT_EQ(parser.state, BATON_PARSER_IDLE);
    EXPECT_EQ(feed_all(&parser, kPoll), 1);
}

TEST_F(Parser, CorruptedHeaderIndexRecovers)
{
    EXPECT_EQ(baton_parser_feed(&parser, 0xA5U), 0);
    parser.index = 9U;
    EXPECT_EQ(baton_parser_feed(&parser, 0x11U), BATON_PARSE_ERROR);
    EXPECT_EQ(parser.state, BATON_PARSER_IDLE);
}

TEST_F(Parser, CorruptedPayloadIndexRecovers)
{
    const Bytes start{0xA5U, 0x12U, 0x11U, 0xA0U, 0x04U, 0x10U};
    EXPECT_EQ(feed_all(&parser, start), 0);
    parser.index = 200U; /* beyond the declared length */
    EXPECT_EQ(baton_parser_feed(&parser, 0x02U), BATON_PARSE_ERROR);

    baton_parser_init(&parser);
    EXPECT_EQ(feed_all(&parser, start), 0);
    parser.frame.length = 255U; /* beyond the buffer */
    parser.index = 1U;
    EXPECT_EQ(baton_parser_feed(&parser, 0x02U), BATON_PARSE_ERROR);
    EXPECT_EQ(parser.state, BATON_PARSER_IDLE);
}
