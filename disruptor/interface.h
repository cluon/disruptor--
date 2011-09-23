// Copyright 2011 <François Saint-Jacques>

#include <climits>
#include <vector>

#include <boost/function.hpp> // NOLINT

#include "disruptor/sequence.h"
#include "disruptor/sequence_batch.h"
#include "disruptor/event.h"

#ifndef DISRUPTOR_INTERFACE_H_ // NOLINT
#define DISRUPTOR_INTERFACE_H_ // NOLINT

namespace disruptor {

class ClaimStrategyInterface {
 public:
    // Claim the next sequence index in the {@link RingBuffer} and increment.
    // @return the {@link AbstractEvent} index to be used for the publisher.
    virtual int64_t IncrementAndGet() = 0;

    // Increment by a delta and get the result.
    // @param delta to increment by.
    // @return the result after incrementing.
    virtual int64_t IncrementAndGet(const int64_t& delta) = 0;

    // Set the current sequence value for claiming {@link AbstractEvent} in
    // the {@link RingBuffer}
    // @param sequence to be set as the current value.
    virtual void SetSequence(const int64_t& sequence) = 0;

    // Ensure dependent processors are in range without over taking them for
    // the buffer size.
    // @param sequence to check is in range
    // @param dependentSequences to be checked for range.
    virtual void EnsureProcessorsAreInRange(const int64_t& sequence,
        const std::vector<Sequence*>& dependent_sequences) = 0;

    // Serialise publishing in sequence.
    // @param cursor to serialise against.
    // @param sequence sequence to be applied
    // @param batchSize of the sequence.
    virtual void SerialisePublishing(const Sequence& cursor,
                                     const int64_t& sequence,
                                     const int64_t& batch_size) = 0;
};

class SequenceBarrierInterface {
 public:
    // Wait for the given sequence to be available for consumption.
    //
    // @param sequence to wait for
    // @return the sequence up to which is available
    // @throws AlertException if a status change has occurred for the
    // Disruptor
    // @throws InterruptedException if the thread needs awaking on a
    // condition variable.
    virtual int64_t WaitFor(const int64_t& sequence) = 0;

     // Wait for the given sequence to be available for consumption with a
     // time out.
     //
     // @param sequence to wait for
     // @param timeout in microseconds
     // @return the sequence up to which is available
     // @throws AlertException if a status change has occurred for the
     // Disruptor
     // @throws InterruptedException if the thread needs awaking on a
     // condition variable.
    virtual int64_t WaitFor(const int64_t& sequence,
                          const int64_t& timeout_micro) = 0;

    // Delegate a call to the {@link RingBuffer#getCursor()}
    //  @return value of the cursor for entries that have been published.
    virtual int64_t GetCursor() const = 0;

    // The current alert status for the barrier.
    // @return true if in alert otherwise false.
    virtual bool IsAlerted() const = 0;

    // Alert the {@link EventProcessor}s of a status change and stay in this
    // status until cleared.
    virtual void Alert() = 0;

    // Clear the current alert status.
    virtual void ClearAlert() = 0;
};

template<typename T>
class EventHandlerInterface {
public:
// Called when a publisher has published an {@link AbstractEvent} to the
// {@link RingBuffer}
// @param event published to the {@link RingBuffer}
// @param endOfBatch flag to indicate if this is the last event in a batch
// from the {@link RingBuffer}
// @throws Exception if the EventHandler would like the exception handled
// further up the chain.
virtual void OnEvent(T* event, bool end_of_batch) = 0;
};

template<typename T>
class EventProcessorInterface {
 public:
     // Get a reference to the {@link Sequence} being used by this
     // {@link EventProcessor}.
     // @return reference to the {@link Sequence} for this
     // {@link EventProcessor}
    virtual Sequence* GetSequence() = 0;

    // Signal that this EventProcessor should stop when it has finished
    // consuming at the next clean break.
    // It will call {@link DependencyBarrier#alert()} to notify the thread to
    // check status.
    virtual void Halt() = 0;
};

template<typename T>
class PublisherPortInterface {
 public:
    // Get the {@link Event<T>} for a given sequence from the underlying
    // {@link RingBuffer<T>}.
    // @param sequence of the {@link Event<T>} to get.
    // @return the {@link Event<T>} for the sequence.
    virtual Event<T>* GetEvent(const int64_t& sequence) = 0;

    // Delegate a call to the {@link RingBuffer#GetCursor()}
    // @return value of the cursor for entries that have been published.
    virtual int64_t GetCursor() = 0;

    // Claim the next {@link Event<T>} in sequence for a publisher on the
    // {@link RingBuffer<T>}
    // @return the claimed {@link Event<T>}
    virtual Event<T>* NextEvent() = 0;

    // Claim the next batch of {@link AbstractEvent}s in sequence.
    // @param sequenceBatch to be updated for the batch range.
    // @return the updated sequenceBatch.
    virtual SequenceBatch* NextEvents(SequenceBatch* sequence_batch) = 0;

    // Publish an event back to the {@link RingBuffer<T>} to make it visible
    // to {@link EventProcessor}s
    // @param event to be published from the {@link RingBuffer<T>}
    virtual void Publish(const long& sequence) = 0;

    // Publish the batch of events from to the {@link RingBuffer<T>}.
    // @param sequenceBatch to be published.
    virtual void Publish(const SequenceBatch& sequence_batch) = 0;
};

class WaitStrategyInterface {
 public:
    //  Wait for the given sequence to be available for consumption in a
    //  {@link RingBuffer}
    //
    //  @param dependents further back the chain that must advance first
    //  @param ringBuffer on which to wait.
    //  @param barrier the consumer is waiting on.
    //  @param sequence to be waited on.
    //  @return the sequence that is available which may be greater than the
    //  requested sequence.
    //
    //  @throws AlertException if the status of the Disruptor has changed.
    //  @throws InterruptedException if the thread is interrupted.
    virtual int64_t WaitFor(const std::vector<Sequence*>& dependents,
                         const Sequence& cursor,
                         const SequenceBarrierInterface& barrier,
                         const int64_t& sequence) = 0;

    //  Wait for the given sequence to be available for consumption in a
    //  {@link RingBuffer} with a timeout specified.
    //
    //  @param dependents further back the chain that must advance first
    //  @param ringBuffer on which to wait.
    //  @param barrier the consumer is waiting on.
    //  @param sequence to be waited on.
    //  @param timeout value to abort after.
    //  @param units of the timeout value.
    //  @return the sequence that is available which may be greater than the
    //  requested sequence.
    //
    //  @throws AlertException if the status of the Disruptor has changed.
    //  @throws InterruptedException if the thread is interrupted.
    virtual int64_t WaitFor(const std::vector<Sequence*>& dependents,
                         const Sequence& cursor,
                         const SequenceBarrierInterface& barrier,
                         const int64_t & sequence,
                         const int64_t & timeout_micros) = 0;

    // Signal those waiting that the {@link RingBuffer} cursor has advanced.
    virtual void SignalAll() = 0;
};

template<typename T>
int64_t GetMinimumSequence(
        const std::vector<EventProcessorInterface<T>*>& event_processors) {
        int64_t minimum = LONG_MAX;

        for (int i = 0; i < event_processors.size(); i++) {
            int64_t sequence = event_processors[i]->GetSequence()->sequence();
            minimum = minimum < sequence ? minimum : sequence;
        }

        return minimum;
};

int64_t GetMinimumSequence(
        const std::vector<Sequence*>& sequences) {
        int64_t minimum = LONG_MAX;

        for (int i = 0; i < sequences.size(); i++) {
            int64_t sequence = sequences[i]->sequence();
            minimum = minimum < sequence ? minimum : sequence;
        }

        return minimum;
};

};  // namespace disruptor

#endif // DISRUPTOR_INTERFACE_H_ NOLINT