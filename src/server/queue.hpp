#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace dariyaraah {

// A bounded queue between one producer and N consumers.
//
// The first shared mutable state in this series. Unit 0 went out of its way not
// to have any — a histogram per thread, merged once at the end — and this is
// where that stops being possible: the accept loop and the workers have to meet
// somewhere.
//
// BOUNDED, and the bound is the point. An unbounded queue does not remove a
// limit, it moves it: connections pile up in memory instead of in the kernel's
// accept backlog, every one of them accruing latency nobody will ever be told
// about, until the machine runs out. A full queue here blocks the accept loop
// instead, the backlog fills, and the kernel starts refusing connects — which
// the client sees immediately and can report. Backpressure that reaches the
// client beats a queue that hides it.
//
// Two condition variables rather than one. With a single one, a push waking a
// blocked push instead of a blocked pop is a lost wakeup that a bounded queue
// cannot recover from.
template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}

    // Blocks while full. Returns false once closed, so a producer can stop.
    bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || items_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        items_.push(std::move(value));
        lock.unlock();  // unlocked before notifying: a woken consumer that has
        not_empty_.notify_one();  // to wait for this lock is a wasted wakeup
        return true;
    }

    // Blocks while empty. nullopt once closed AND drained, so workers finish
    // what was accepted rather than dropping it on the way out.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !items_.empty(); });
        if (items_.empty()) {
            return std::nullopt;
        }
        T value = std::move(items_.front());
        items_.pop();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    // Wakes everyone, once. Idempotent, because shutdown races are ordinary.
    void close() {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> items_;
    std::size_t capacity_;
    bool closed_ = false;
};

}  // namespace dariyaraah
