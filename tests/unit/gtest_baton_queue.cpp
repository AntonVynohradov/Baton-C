/**
 * @file gtest_baton_queue.cpp
 * @brief Unit tests of the event queue (variable-size record ring).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "baton_test_support.h"

namespace
{

class Queue : public ::testing::Test
{
  protected:
    void SetUp() override { baton_queue_init(&queue, memory, sizeof(memory)); }

    uint16_t push(uint8_t length)
    {
        uint16_t offset = 0xFFFFU;
        if (baton_queue_alloc(&queue, baton_queue_record_size(length), &offset))
        {
            baton_record record{};
            record.length = length;
            record.flags = BATON_REC_VALID;
            baton_queue_write(&queue, offset, &record);
        }
        return offset;
    }

    void pop()
    {
        const uint16_t first = baton_queue_first(&queue, queue.head, queue.tail);
        queue.head = baton_queue_next(&queue, first, queue.tail);
    }

    uint8_t memory[130]{};
    baton_queue queue{};
};

}  // namespace

TEST_F(Queue, InitRoundsTheSizeDownToFour)
{
    EXPECT_EQ(queue.size, 128U);
    EXPECT_EQ(baton_queue_used(&queue), 0U);
}

TEST_F(Queue, RecordSize)
{
    EXPECT_EQ(baton_queue_record_size(0U), 16U);
    EXPECT_EQ(baton_queue_record_size(1U), 20U);
    EXPECT_EQ(baton_queue_record_size(4U), 20U);
    EXPECT_EQ(baton_queue_record_size(24U), 40U);
}

TEST_F(Queue, HeaderRoundTrip)
{
    const uint16_t offset = push(5U);
    baton_record in{0x11223344U, 0x55667788U, 0x99AABBCCU, 0x10U, 5U, 0x12U, BATON_REC_VALID};
    baton_queue_write(&queue, offset, &in);
    baton_record out{};
    baton_queue_read(&queue, offset, &out);
    EXPECT_EQ(out.pending, in.pending);
    EXPECT_EQ(out.inflight, in.inflight);
    EXPECT_EQ(out.stamp, in.stamp);
    EXPECT_EQ(out.msg_id, in.msg_id);
    EXPECT_EQ(out.length, in.length);
    EXPECT_EQ(out.dst, in.dst);
    EXPECT_EQ(out.flags, in.flags);

    baton_queue_set_masks(&queue, offset, 1U, 2U);
    baton_queue_set_flags(&queue, offset, 0U);
    baton_queue_read(&queue, offset, &out);
    EXPECT_EQ(out.pending, 1U);
    EXPECT_EQ(out.inflight, 2U);
    EXPECT_EQ(out.flags, 0U);
    EXPECT_EQ(out.stamp, in.stamp);
    EXPECT_EQ(baton_queue_data(&queue, offset), &memory[offset + BATON_REC_HEADER_SIZE]);
}

TEST_F(Queue, FillsAndRejectsWhenFull)
{
    EXPECT_EQ(push(24U), 0U);
    EXPECT_EQ(push(24U), 40U);
    EXPECT_EQ(push(24U), 80U);
    EXPECT_EQ(push(24U), 0xFFFFU); /* 8 bytes left at the end, head at 0 */
    EXPECT_EQ(baton_queue_used(&queue), 120U);
}

TEST_F(Queue, WrapsWithSkipMarker)
{
    push(24U); /* 0..39   */
    push(24U); /* 40..79  */
    push(8U);  /* 80..103 */
    pop();
    pop();                    /* head 80 */
    EXPECT_EQ(push(24U), 0U); /* 24 left at the end: skip marker at 104, wrap */
    EXPECT_EQ(baton_queue_used(&queue), static_cast<uint16_t>((128U - 80U) + 40U));
    /* a reader walks 80 -> (skip at 104) -> 0 */
    uint16_t offset = baton_queue_first(&queue, queue.head, queue.tail);
    EXPECT_EQ(offset, 80U);
    offset = baton_queue_next(&queue, offset, queue.tail);
    EXPECT_EQ(offset, 0U);
    offset = baton_queue_next(&queue, offset, queue.tail);
    EXPECT_EQ(offset, queue.tail);
}

TEST_F(Queue, WrapsWithoutRoomForASkipMarker)
{
    push(24U);               /* 0..39   */
    push(24U);               /* 40..79  */
    push(24U);               /* 80..119 */
    pop();                   /* head 40 */
    EXPECT_EQ(push(4U), 0U); /* 8 left at the end: no header fits, wrap without marker */
    uint16_t offset = baton_queue_first(&queue, queue.head, queue.tail);
    offset = baton_queue_next(&queue, offset, queue.tail);
    EXPECT_EQ(offset, 80U);
    offset = baton_queue_next(&queue, offset, queue.tail); /* 120: no header fits -> 0 */
    EXPECT_EQ(offset, 0U);
}

TEST_F(Queue, WrapRequiresSpaceStrictlyBeforeTheHead)
{
    push(24U);
    push(24U);
    push(24U);                     /* tail 120 */
    pop();                         /* head 40 */
    EXPECT_EQ(push(24U), 0xFFFFU); /* need 40, head 40: not strictly more */
    EXPECT_EQ(push(20U), 0U);      /* need 36 < 40 */
}

TEST_F(Queue, AllocBetweenTailAndHead)
{
    push(24U);
    push(24U);
    push(24U);
    pop();
    pop();                        /* head 80 */
    push(4U);                     /* wrapped: 0..19, tail 20 */
    EXPECT_EQ(push(24U), 20U);    /* 60 free before head 80 */
    EXPECT_EQ(push(4U), 0xFFFFU); /* 20 needed, 20 free: not strictly more */
    EXPECT_EQ(baton_queue_used(&queue), static_cast<uint16_t>((128U - 80U) + 60U));
}

TEST_F(Queue, EmptyQueueResetsToTheStart)
{
    push(24U);
    push(24U);
    pop();
    pop();
    EXPECT_EQ(queue.head, queue.tail);
    EXPECT_EQ(push(24U), 0U);
}

TEST_F(Queue, FirstOfAnEmptySnapshotIsTheTail)
{
    EXPECT_EQ(baton_queue_first(&queue, 0U, 0U), 0U);
    queue.head = 64U;
    queue.tail = 64U;
    EXPECT_EQ(baton_queue_first(&queue, 64U, 64U), 64U);
}

TEST_F(Queue, InvalidArguments)
{
    uint16_t offset = 0U;
    baton_record record{};
    baton_queue_init(nullptr, memory, 16U);
    baton_queue_init(&queue, nullptr, 16U);
    EXPECT_EQ(queue.size, 128U); /* unchanged */
    EXPECT_FALSE(baton_queue_alloc(nullptr, 16U, &offset));
    EXPECT_FALSE(baton_queue_alloc(&queue, 16U, nullptr));
    EXPECT_FALSE(baton_queue_alloc(&queue, 4U, &offset)); /* smaller than a header */
    EXPECT_EQ(baton_queue_first(nullptr, 0U, 8U), 8U);
    EXPECT_EQ(baton_queue_next(nullptr, 0U, 8U), 8U);
    EXPECT_EQ(baton_queue_used(nullptr), 0U);
    EXPECT_EQ(baton_queue_data(nullptr, 0U), nullptr);
    baton_queue_read(nullptr, 0U, &record);
    baton_queue_read(&queue, 0U, nullptr);
    baton_queue_write(nullptr, 0U, &record);
    baton_queue_write(&queue, 0U, nullptr);
    baton_queue_set_masks(nullptr, 0U, 0U, 0U);
    baton_queue_set_flags(nullptr, 0U, 0U);
}
