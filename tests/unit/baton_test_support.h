/**
 * @file baton_test_support.h
 * @brief Shared helpers of the Baton unit tests: mocks, frame builders, node harness.
 *
 * Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Author: Anton Vynohradov
 * Email: avynohradov@systemfromscratch.com
 *
 * @copyright Copyright (c) 2026 Anton Vynohradov
 * @license Apache License 2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BATON_TEST_SUPPORT_H
#define BATON_TEST_SUPPORT_H

#include <cstdint>
#include <vector>

extern "C"
{
#include "baton.h"
}

namespace baton_test
{

using Bytes = std::vector<uint8_t>;

/** Complete frame built with baton_encode(). */
Bytes frame(uint8_t dst, uint8_t src, uint8_t ctrl, const Bytes& payload);

/** TLV record: id, length, data. */
Bytes tlv(uint8_t id, const Bytes& data);

/** Concatenate byte vectors. */
Bytes cat(const Bytes& a, const Bytes& b);

/** Feed every byte to the parser; returns the result of the last call. */
int feed_all(baton_parser* parser, const Bytes& bytes);

/** Writes recorded by capture_write(). */
struct WriteLog
{
    std::vector<Bytes> frames;
    int result = 0;
};

/** baton_write_fn that records the frame into a WriteLog (context) and returns its result. */
int capture_write(void* context, const uint8_t* data, size_t length);

struct Message
{
    uint8_t src;
    uint8_t id;
    Bytes data;
};

struct Drop
{
    uint8_t id;
    uint8_t dst;
    uint32_t missed;
};

struct Poll
{
    uint8_t slave;
    bool responded;
    uint32_t nack;
};

/**
 * One node under test with recorded writes and callbacks.
 *
 * Bus used by the tests: master 0x01, slaves 0x11, 0x12, 0x13 (node indexes 1..3).
 */
struct Harness
{
    baton_node node{};
    baton_node_config config{};
    WriteLog writes;
    std::vector<Message> messages;
    std::vector<Drop> drops;
    std::vector<Poll> polls;
    int critical_depth = 0;
    int critical_calls = 0;
    uint32_t now = 1000U;

    static const uint8_t kMaster = 0x01U;
    static const uint8_t kSlaves[3];

    /** Default configuration of the node with address @p own (callbacks and critical hooks set). */
    explicit Harness(uint8_t own, uint8_t slave_count = 3U);

    /** baton_node_init() with the current configuration at the current time. */
    int init();

    /** Advance the time and call baton_node_process(). */
    void process(uint32_t advance_ms = 0U);

    /** Feed bytes into the receive ring (no processing). */
    void rx(const Bytes& bytes);

    /** Feed bytes and process. */
    void inject(const Bytes& bytes, uint32_t advance_ms = 0U);

    /** Echo the last written frame (single-wire UART) and process: the transmission completes. */
    void echo_last(uint32_t advance_ms = 0U);

    /** Last frame passed to the write function (empty when none). */
    Bytes last() const;

    /** Process until a new frame is written or @p limit_ms elapsed; returns true when written. */
    bool run_until_write(uint32_t limit_ms = 1000U);

    /** Payload of the last written frame. */
    Bytes last_payload() const;

    /** Control byte of the last written frame. */
    uint8_t last_ctrl() const;
};

}  // namespace baton_test

#endif /* BATON_TEST_SUPPORT_H */
