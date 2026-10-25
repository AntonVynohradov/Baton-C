/**
 * @file baton_test_support.cpp
 * @brief Shared helpers of the Baton unit tests.
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

#include "baton_test_support.h"

namespace baton_test
{

const uint8_t Harness::kSlaves[3] = {0x11U, 0x12U, 0x13U};

Bytes frame(uint8_t dst, uint8_t src, uint8_t ctrl, const Bytes& payload)
{
    Bytes out(BATON_FRAME_MAX);
    size_t length = 0U;
    const int result = baton_encode(dst, src, ctrl, payload.empty() ? nullptr : payload.data(),
                                    payload.size(), out.data(), out.size(), &length);
    if (result != 0)
    {
        out.clear();
        return out;
    }
    out.resize(length);
    return out;
}

Bytes tlv(uint8_t id, const Bytes& data)
{
    Bytes out{id, static_cast<uint8_t>(data.size())};
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

Bytes cat(const Bytes& a, const Bytes& b)
{
    Bytes out(a);
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

int feed_all(baton_parser* parser, const Bytes& bytes)
{
    int result = 0;
    for (uint8_t b : bytes)
    {
        result = baton_parser_feed(parser, b);
    }
    return result;
}

int capture_write(void* context, const uint8_t* data, size_t length)
{
    WriteLog* log = static_cast<WriteLog*>(context);
    log->frames.emplace_back(data, data + length);
    return log->result;
}

namespace
{

Harness* self(void* context)
{
    return static_cast<Harness*>(context);
}

int harness_write(void* context, const uint8_t* data, size_t length)
{
    return capture_write(&self(context)->writes, data, length);
}

void on_message(void* context, uint8_t src, uint8_t id, const uint8_t* data, uint8_t length)
{
    self(context)->messages.push_back(Message{src, id, Bytes(data, data + length)});
}

void on_drop(void* context, uint8_t id, uint8_t dst, uint32_t missed)
{
    self(context)->drops.push_back(Drop{id, dst, missed});
}

void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack)
{
    self(context)->polls.push_back(Poll{slave, responded, nack});
}

void enter(void* context)
{
    self(context)->critical_depth++;
    self(context)->critical_calls++;
}

void leave(void* context)
{
    self(context)->critical_depth--;
}

}  // namespace

Harness::Harness(uint8_t own, uint8_t slave_count)
{
    baton_node_default_config(&config);
    config.role = (own == kMaster) ? BATON_ROLE_MASTER : BATON_ROLE_SLAVE;
    config.own_address = own;
    config.master_address = kMaster;
    config.slave_addresses = kSlaves;
    config.slave_count = slave_count;
    config.write = harness_write;
    config.enter_critical = enter;
    config.exit_critical = leave;
    config.on_message = on_message;
    config.on_drop = on_drop;
    config.on_poll_done = on_poll_done;
    config.context = this;
}

int Harness::init()
{
    return baton_node_init(&node, &config, now);
}

void Harness::process(uint32_t advance_ms)
{
    now += advance_ms;
    baton_node_process(&node, now);
}

void Harness::rx(const Bytes& bytes)
{
    for (uint8_t b : bytes)
    {
        baton_node_rx_isr(&node, b);
    }
}

void Harness::inject(const Bytes& bytes, uint32_t advance_ms)
{
    now += advance_ms;
    rx(bytes);
    baton_node_process(&node, now);
}

void Harness::echo_last(uint32_t advance_ms)
{
    inject(last(), advance_ms);
}

Bytes Harness::last() const
{
    return writes.frames.empty() ? Bytes{} : writes.frames.back();
}

bool Harness::run_until_write(uint32_t limit_ms)
{
    const size_t before = writes.frames.size();
    for (uint32_t t = 0U; t <= limit_ms; ++t)
    {
        process(t == 0U ? 0U : 1U);
        if (writes.frames.size() != before)
        {
            return true;
        }
    }
    return false;
}

Bytes Harness::last_payload() const
{
    const Bytes f = last();
    if (f.size() < BATON_FRAME_OVERHEAD)
    {
        return Bytes{};
    }
    return Bytes(f.begin() + BATON_HEADER_SIZE, f.end() - BATON_CRC_SIZE);
}

uint8_t Harness::last_ctrl() const
{
    const Bytes f = last();
    return (f.size() > 3U) ? f[3] : 0U;
}

}  // namespace baton_test
