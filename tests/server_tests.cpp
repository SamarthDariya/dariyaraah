#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/clock.hpp"
#include "server/handler.hpp"

using namespace dariyaraah;
using namespace dariyanaap;

TEST_CASE("GET / is served, and anything else is refused") {
    const http::Response ok = handle({"GET", "/", "HTTP/1.1"});
    CHECK(ok.status == 200);
    CHECK_FALSE(ok.body.empty());

    CHECK(handle({"GET", "/health", "HTTP/1.1"}).status == 404);
    CHECK(handle({"POST", "/", "HTTP/1.1"}).status == 405);
}

TEST_CASE("the database call is really twenty milliseconds, and really in the handler") {
    // Asserting a lower bound only. sleep_for promises at least its duration
    // and no upper bound at all, and under TSan the overshoot is large enough
    // that any ceiling here would be testing the sanitiser rather than the code.
    const MonotonicClock::Instant started = MonotonicClock::now();
    CHECK(handle({"GET", "/", "HTTP/1.1"}).status == 200);
    CHECK(MonotonicClock::since(started) >= kDatabaseDelay);

    // And the refused path does not pay it, which is what pins the sleep to
    // where the work is rather than to every request that arrives. Without this
    // the case above would pass just as well with the sleep at the top of the
    // function, and a run against the wrong path would look like a fast server.
    const MonotonicClock::Instant refused_at = MonotonicClock::now();
    CHECK(handle({"GET", "/health", "HTTP/1.1"}).status == 404);
    CHECK(MonotonicClock::since(refused_at) < kDatabaseDelay);
}
