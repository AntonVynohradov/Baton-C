/**
 * @file gtest_baton_link.cpp
 * @brief Unit tests of the half-duplex link: ring, parser timeout, transmitter, echo.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

using baton_test::Bytes;
using baton_test::WriteLog;

namespace
{

const Bytes kPoll{0xA5U, 0x11U, 0x01U, 0x20U, 0x01U, 0x00U, 0x7CU, 0x44U};

class Link : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        config.write = baton_test::capture_write;
        config.context = &log;
        config.baudrate = 9600U;
        config.tx_gap_ms = 2U;
        config.inter_byte_timeout_ms = 5U;
        config.rx_echo = true;
        ASSERT_EQ(baton_link_init(&link, &config, now), 0);
    }

    void rx(const Bytes& bytes)
    {
        for (uint8_t b : bytes)
        {
            baton_link_rx_isr(&link, b);
        }
    }

    int receive() { return baton_link_receive(&link, now); }

    baton_link link{};
    baton_link_config config{};
    WriteLog log;
    uint32_t now = 100U;
};

}  // namespace

TEST_F(Link, InitRejectsInvalidConfiguration)
{
    baton_link_config bad = config;
    EXPECT_EQ(baton_link_init(nullptr, &config, now), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_link_init(&link, nullptr, now), BATON_PARSE_ERROR);
    bad.write = nullptr;
    EXPECT_EQ(baton_link_init(&link, &bad, now), BATON_PARSE_ERROR);
    bad = config;
    bad.baudrate = 0U;
    EXPECT_EQ(baton_link_init(&link, &bad, now), BATON_PARSE_ERROR);
    bad = config;
    bad.inter_byte_timeout_ms = 0U;
    EXPECT_EQ(baton_link_init(&link, &bad, now), BATON_PARSE_ERROR);
}

TEST_F(Link, NullLinkIsIgnored)
{
    baton_link_rx_isr(nullptr, 0U);
    baton_link_tx_done_isr(nullptr);
    baton_link_cancel(nullptr);
    EXPECT_EQ(baton_link_receive(nullptr, now), BATON_LINK_NONE);
    EXPECT_EQ(baton_link_transmit(nullptr, now), BATON_LINK_NONE);
    EXPECT_EQ(baton_link_frame(nullptr), nullptr);
    EXPECT_FALSE(baton_link_rx_busy(nullptr, now));
    EXPECT_EQ(baton_link_frame_ms(nullptr, 10U), 0U);
}

TEST_F(Link, ReceivesAFrame)
{
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    const baton_frame* frame = baton_link_frame(&link);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->dst, 0x11U);
    EXPECT_EQ(receive(), BATON_LINK_NONE);
    EXPECT_EQ(link.counters.rx_frames, 1U);
}

TEST_F(Link, TwoFramesInTheRingAreReportedOneByOne)
{
    rx(kPoll);
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    EXPECT_EQ(receive(), BATON_LINK_NONE);
}

TEST_F(Link, CrcErrorsAreCounted)
{
    Bytes bad = kPoll;
    bad.back() ^= 0x01U;
    rx(bad);
    EXPECT_EQ(receive(), BATON_LINK_NONE);
    EXPECT_EQ(link.counters.crc_errors, 1U);
}

TEST_F(Link, RingOverflowDropsBytes)
{
    for (uint32_t i = 0U; i < BATON_RX_BUF_SIZE + 5U; ++i)
    {
        baton_link_rx_isr(&link, 0x00U);
    }
    EXPECT_EQ(link.rx_overflows, 6U); /* one slot always stays free */
}

TEST_F(Link, InterByteTimeoutAbortsAFrame)
{
    rx(Bytes(kPoll.begin(), kPoll.begin() + 4));
    EXPECT_EQ(receive(), BATON_LINK_NONE);
    EXPECT_TRUE(baton_link_rx_busy(&link, now));
    EXPECT_TRUE(baton_link_rx_busy(&link, now + 5U));
    EXPECT_FALSE(baton_link_rx_busy(&link, now + 6U));
    now += 6U;
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    EXPECT_EQ(link.counters.rx_timeouts, 1U);
}

TEST_F(Link, SilenceBetweenFramesIsNoTimeout)
{
    now += 100U;
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    EXPECT_EQ(link.counters.rx_timeouts, 0U);
}

TEST_F(Link, SendRejectsInvalidArguments)
{
    EXPECT_EQ(baton_link_send(nullptr, kPoll.data(), kPoll.size()), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_link_send(&link, nullptr, kPoll.size()), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_link_send(&link, kPoll.data(), 0U), BATON_PARSE_ERROR);
    EXPECT_EQ(baton_link_send(&link, kPoll.data(), BATON_FRAME_MAX + 1U), BATON_PARSE_ERROR);
}

TEST_F(Link, TransmitWaitsForTheGapAndCompletesOnEcho)
{
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now + 1U), BATON_LINK_NONE); /* gap since init */
    EXPECT_TRUE(log.frames.empty());
    now += 2U;
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    ASSERT_EQ(log.frames.size(), 1U);
    EXPECT_EQ(log.frames[0], kPoll);
    EXPECT_EQ(link.counters.tx_frames, 1U);

    rx(Bytes(kPoll.begin(), kPoll.end() - 1));
    EXPECT_EQ(receive(), BATON_LINK_NONE);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE); /* still transmitting */
    rx(Bytes{kPoll.back()});
    EXPECT_EQ(receive(), BATON_LINK_TX_DONE);
    EXPECT_EQ(link.counters.collisions, 0U);
    EXPECT_EQ(link.counters.rx_frames, 0U);                      /* own echo is not parsed */
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE); /* idle */
}

TEST_F(Link, EchoMismatchIsACollision)
{
    now += 10U;
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    Bytes echo = kPoll;
    echo[2] ^= 0x01U;
    rx(echo);
    EXPECT_EQ(receive(), BATON_LINK_TX_DONE);
    EXPECT_EQ(link.counters.collisions, 1U);
}

TEST_F(Link, GapAfterReceivedBytesAndAfterOwnTransmission)
{
    now += 10U;
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now + 1U), BATON_LINK_NONE);
    EXPECT_TRUE(log.frames.empty());
    EXPECT_EQ(baton_link_transmit(&link, now + 2U), BATON_LINK_NONE);
    EXPECT_EQ(log.frames.size(), 1U);
}

TEST_F(Link, GapAfterTheOwnTransmissionAlone)
{
    now += 10U;
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_TX_DONE); /* own echo: last_rx_ms stays old */
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now + 1U), BATON_LINK_NONE);
    EXPECT_EQ(log.frames.size(), 1U);
    EXPECT_EQ(baton_link_transmit(&link, now + 2U), BATON_LINK_NONE);
    EXPECT_EQ(log.frames.size(), 2U);
}

TEST_F(Link, NoTransmissionWhileAFrameIsBeingReceived)
{
    now += 10U;
    rx(Bytes(kPoll.begin(), kPoll.begin() + 3));
    EXPECT_EQ(receive(), BATON_LINK_NONE);
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    now += 3U;
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    EXPECT_TRUE(log.frames.empty());
    now += 3U; /* inter-byte timeout expired */
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    EXPECT_EQ(log.frames.size(), 1U);
}

TEST_F(Link, WriteFailureEndsTheTransmission)
{
    now += 10U;
    log.result = -5;
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_TX_DONE);
    EXPECT_EQ(link.counters.tx_timeouts, 1U);
}

TEST_F(Link, MissingEchoTimesOut)
{
    now += 10U;
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    const uint32_t limit = baton_link_frame_ms(&link, kPoll.size()) + 2U + 5U;
    EXPECT_EQ(baton_link_transmit(&link, now + limit), BATON_LINK_NONE);
    EXPECT_EQ(baton_link_transmit(&link, now + limit + 1U), BATON_LINK_TX_DONE);
    EXPECT_EQ(link.counters.tx_timeouts, 1U);
}

TEST_F(Link, TxDoneInterruptWithoutEcho)
{
    config.rx_echo = false;
    ASSERT_EQ(baton_link_init(&link, &config, now), 0);
    now += 10U;
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_NONE);
    baton_link_tx_done_isr(&link);
    EXPECT_EQ(baton_link_transmit(&link, now), BATON_LINK_TX_DONE);

    /* without echo, bytes received while transmitting go to the parser */
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now + 2U), BATON_LINK_NONE);
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_FRAME);
}

TEST_F(Link, CancelDropsOnlyAPendingRequest)
{
    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    baton_link_cancel(&link);
    EXPECT_EQ(baton_link_transmit(&link, now + 10U), BATON_LINK_NONE);
    EXPECT_TRUE(log.frames.empty());

    ASSERT_EQ(baton_link_send(&link, kPoll.data(), kPoll.size()), 0);
    EXPECT_EQ(baton_link_transmit(&link, now + 10U), BATON_LINK_NONE);
    baton_link_cancel(&link); /* already transmitting: no effect */
    rx(kPoll);
    EXPECT_EQ(receive(), BATON_LINK_TX_DONE);
}

TEST_F(Link, FrameDuration)
{
    EXPECT_EQ(baton_link_frame_ms(&link, 0U), 0U);
    EXPECT_EQ(baton_link_frame_ms(&link, 1U), 2U);   /* 1.04 ms rounded up */
    EXPECT_EQ(baton_link_frame_ms(&link, 71U), 74U); /* 73.96 ms rounded up */
    EXPECT_EQ(baton_link_frame_ms(&link, 96U), 100U);
}
