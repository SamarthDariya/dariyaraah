#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <thread>

#include "core/clock.hpp"
#include "load/closed_loop.hpp"
#include "load/http11_get.hpp"
#include "server/handler.hpp"
#include "server/server.hpp"

using namespace dariyaraah;
using namespace dariyanaap;
using namespace std;

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

// ---------------------------------------------------------------------------
// End to end
// ---------------------------------------------------------------------------

TEST_CASE("the rig can put load on a real server, and Little's law holds") {
    Server server("127.0.0.1", 0);  // ephemeral: two suites must not collide
    thread accepting([&server] { server.run(); });

    ClosedLoopPlan plan;
    plan.target = Endpoint("127.0.0.1", server.port());
    plan.connections = 4;
    plan.duration = Millis(500);
    const Http11Get protocol("127.0.0.1", "/");
    const ClosedLoopRun run = run_closed_loop(protocol, plan);

    server.stop();
    accepting.join();

    CHECK(run.result.errors.total() == 0);
    REQUIRE(run.result.latency.has_value());

    // Every request pays the database call. If this fails, the 20ms is not on
    // the path the client's requests take.
    CHECK(run.result.latency->p50 >= kDatabaseDelay);

    // The first appearance of the number this whole repo is about: four
    // connections, each serialised behind a 20ms handler, cannot exceed
    // 4 / 0.02s = 200 rps. Asserted as a ceiling rather than a range, because
    // a ceiling is physics and a range is a guess about this machine's mood —
    // and under TSan the mood is very different.
    CHECK(run.result.latency->per_second() <= 210.0);
    CHECK(run.result.latency->per_second() > 50.0);
}

TEST_CASE("a pool bounds throughput by its workers, not by its connections") {
    // M3's finding, pinned. Two workers, eight connections: a worker holds a
    // keep-alive connection for that connection's whole life, so six of the
    // eight are never served at all and throughput is 2/service_time rather
    // than 8/service_time. Under M1's model the same plan gives ~328 rps.
    Server server("127.0.0.1", 0, 2);
    thread accepting([&server] { server.run(); });

    ClosedLoopPlan plan;
    plan.target = Endpoint("127.0.0.1", server.port());
    plan.connections = 8;
    plan.duration = Millis(1000);
    const Http11Get protocol("127.0.0.1", "/");
    const ClosedLoopRun run = run_closed_loop(protocol, plan);

    server.stop();
    accepting.join();

    REQUIRE(run.result.latency.has_value());
    // The discriminating number: two workers behind a ~24ms handler cannot
    // exceed ~82 rps. Eight threads would be four times that.
    CHECK(run.result.latency->per_second() < 150.0);
}
