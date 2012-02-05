// Copyright (c) 2011, François Saint-Jacques
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the disruptor-- nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL FRANÇOIS SAINT-JACQUES BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <atomic>
#include <iostream>
#include <gtest/gtest.h>

#include <disruptor/sequencer.h>

#define BUFFER_SIZE 4

namespace disruptor {
namespace test {

struct SequencerFixture : public testing::Test {
    SequencerFixture() :
        sequencer(BUFFER_SIZE,
                  kSingleThreadedStrategy,
                  kSleepingStrategy),
        gating_sequence(kInitialCursorValue) {
            std::vector<Sequence*> sequences;
            sequences.push_back(&gating_sequence);
            sequencer.set_gating_sequences(sequences);
        }

    ~SequencerFixture() {}

    void FillBuffer() {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            int64_t sequence = sequencer.Next();
            sequencer.Publish(sequence);
        }
    }

    Sequencer sequencer;
    Sequence gating_sequence;
};

TEST_F(SequencerFixture, ShouldStartWithValueInitialized) {
    EXPECT_TRUE(sequencer.GetCursor() == kInitialCursorValue);
}

TEST_F(SequencerFixture, ShouldGetPublishFirstSequence) {
    const int64_t sequence = sequencer.Next();
    EXPECT_TRUE(sequencer.GetCursor() == kInitialCursorValue);
    EXPECT_TRUE(sequence == 0);

    sequencer.Publish(sequence);
    EXPECT_TRUE(sequencer.GetCursor() == sequence);
}

TEST_F(SequencerFixture, ShouldIndicateAvailableCapacity) {
    EXPECT_TRUE(sequencer.HasAvalaibleCapacity());
}

TEST_F(SequencerFixture, ShouldIndicateNoAvailableCapacity) {
    FillBuffer();
    EXPECT_TRUE(sequencer.HasAvalaibleCapacity() == false);
}

TEST_F(SequencerFixture, ShouldForceClaimSequence) {
    const int64_t claim_sequence = 3;
    const int64_t sequence = sequencer.Claim(claim_sequence);

    EXPECT_TRUE(sequencer.GetCursor() == kInitialCursorValue);
    EXPECT_TRUE(sequence == claim_sequence);

    sequencer.ForcePublish(sequence);
    EXPECT_TRUE(sequencer.GetCursor() == claim_sequence);
}

TEST_F(SequencerFixture, ShouldPublishSequenceBatch) {
    const int batch_size = 3;
    BatchDescriptor batch_descriptor(batch_size);
    sequencer.Next(&batch_descriptor);

    EXPECT_TRUE(sequencer.GetCursor() == kInitialCursorValue);
    EXPECT_TRUE(batch_descriptor.end() == kInitialCursorValue + batch_size);
    EXPECT_TRUE(batch_descriptor.size() == batch_size);

    sequencer.Publish(batch_descriptor);
    EXPECT_TRUE(sequencer.GetCursor() == kInitialCursorValue + batch_size);
}

TEST_F(SequencerFixture, ShouldWaitOnSequence) {
    std::vector<Sequence*> dependents(0);
    ProcessingSequenceBarrier* barrier = sequencer.NewBarrier(dependents);

    const int64_t sequence = sequencer.Next();
    sequencer.Publish(sequence);

    EXPECT_TRUE(sequence == barrier->WaitFor(sequence));
}

TEST_F(SequencerFixture, ShouldWaitOnSequenceShowingBatchingEffect) {
    std::vector<Sequence*> dependents(0);
    ProcessingSequenceBarrier* barrier = sequencer.NewBarrier(dependents);

    sequencer.Publish(sequencer.Next());
    sequencer.Publish(sequencer.Next());

    const int64_t sequence = sequencer.Next();
    sequencer.Publish(sequence);

    EXPECT_TRUE(sequence == barrier->WaitFor(kInitialCursorValue + 1LL));
}

TEST_F(SequencerFixture, ShouldSignalWaitingProcessorWhenSequenceIsPublished) {
    std::vector<Sequence*> dependents(0);
    ProcessingSequenceBarrier* barrier = sequencer.NewBarrier(dependents);

    std::atomic<bool> waiting(true);
    std::atomic<bool> completed(false);


    std::thread thread(
            // lambda prototype
            [](Sequence* gating_sequence,
               ProcessingSequenceBarrier* barrier,
               std::atomic<bool>* waiting,
               std::atomic<bool>* completed) {
            // body
                waiting->store(false);
                EXPECT_TRUE(kInitialCursorValue + 1LL == \
                    barrier->WaitFor(kInitialCursorValue + 1LL));
                gating_sequence->set_sequence(kInitialCursorValue + 1LL);
                completed->store(true);
            },
            &gating_sequence,
            barrier,
            &waiting,
            &completed);

    while (waiting.load()) {}
    EXPECT_TRUE(gating_sequence.sequence() == kInitialCursorValue);

    sequencer.Publish(sequencer.Next());

    while (!completed.load()) {}
    EXPECT_TRUE(gating_sequence.sequence() == kInitialCursorValue + 1LL);

    thread.join();
}

TEST_F(SequencerFixture, ShouldHoldUpPublisherWhenRingIsFull) {
    std::atomic<bool> waiting(true);
    std::atomic<bool> completed(false);

    FillBuffer();

    const int64_t expected_full_cursor = kInitialCursorValue + BUFFER_SIZE;
    EXPECT_TRUE(sequencer.GetCursor() == expected_full_cursor);

    std::thread thread(
            // lambda prototype
            [](Sequencer* sequencer,
               std::atomic<bool>* waiting,
               std::atomic<bool>* completed) {
                // body
                waiting->store(false);
                sequencer->Publish(sequencer->Next());
                completed->store(true);
            }, // end of lambda
            &sequencer,
            &waiting,
            &completed);

    while (waiting.load()) {}
    EXPECT_TRUE(sequencer.GetCursor() == expected_full_cursor);

    gating_sequence.set_sequence(kInitialCursorValue + 1LL);

    while (!completed.load()) {}
    EXPECT_TRUE(sequencer.GetCursor() == expected_full_cursor + 1LL);

    thread.join();

}


}; // namespace test
}; // namespace disruptor
