#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "server/queue.hpp"

using namespace dariyaraah;
using namespace std;

TEST_CASE("items come back out in the order they went in") {
    BlockingQueue<int> queue(4);
    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.size() == 2);
    CHECK(queue.pop().value() == 1);
    CHECK(queue.pop().value() == 2);
}

TEST_CASE("a full queue blocks its producer until a consumer makes room") {
    BlockingQueue<int> queue(1);
    CHECK(queue.push(1));

    atomic<bool> pushed{false};
    thread producer([&] {
        queue.push(2);  // must wait: capacity is one and one is in there
        pushed.store(true);
    });

    // Not a sleep-and-hope: the only thing that can unblock the producer is the
    // pop below, so if it has already returned the bound is not being enforced.
    CHECK_FALSE(pushed.load());
    CHECK(queue.pop().value() == 1);
    producer.join();
    CHECK(pushed.load());
    CHECK(queue.pop().value() == 2);
}

TEST_CASE("close wakes a blocked consumer, but only after the queue is drained") {
    BlockingQueue<int> queue(4);
    CHECK(queue.push(7));
    queue.close();

    // Drained first: a worker must finish serving what was already accepted
    // rather than dropping it because shutdown started.
    CHECK(queue.pop().value() == 7);
    CHECK_FALSE(queue.pop().has_value());
    CHECK_FALSE(queue.push(8));  // and a closed queue accepts nothing more
}

TEST_CASE("nothing is lost or duplicated under four producers and four consumers") {
    // The case TSan is here for. Every item is pushed exactly once, so the sum
    // the consumers report can only be right if none was dropped or handed out
    // twice — a count alone would pass while two workers served one connection.
    constexpr int kPerProducer = 500;
    BlockingQueue<int> queue(8);
    atomic<long long> total{0};
    atomic<int> taken{0};

    vector<thread> threads;
    for (int p = 0; p < 4; ++p) {
        threads.emplace_back([&queue, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.push(p * kPerProducer + i);
            }
        });
    }
    for (int c = 0; c < 4; ++c) {
        threads.emplace_back([&] {
            while (const optional<int> item = queue.pop()) {
                total.fetch_add(*item);
                taken.fetch_add(1);
            }
        });
    }

    for (int p = 0; p < 4; ++p) {
        threads[static_cast<size_t>(p)].join();
    }
    queue.close();
    for (size_t t = 4; t < threads.size(); ++t) {
        threads[t].join();
    }

    constexpr int kTotal = 4 * kPerProducer;
    CHECK(taken.load() == kTotal);
    CHECK(total.load() == static_cast<long long>(kTotal) * (kTotal - 1) / 2);
}
