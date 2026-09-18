#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>

// The assumptions every number in this repo rests on.
//
// A suite that tests the toolchain rather than the code looks like padding
// until one of these is false. Both have a failure mode that produces plausible
// numbers rather than an error, which is the only kind of bug this track
// genuinely cannot survive.

TEST_CASE("the compiler is actually building C++20") {
    // CMake's CXX_STANDARD is a request, not a guarantee: a toolchain that
    // cannot honour it downgrades, and the build still succeeds.
    static_assert(__cplusplus >= 202002L, "C++20 is required");
    CHECK(__cplusplus >= 202002L);
}

TEST_CASE("steady_clock is monotonic on this platform") {
    // Unit 0's decision 12, inherited. system_clock can step backwards when
    // NTP corrects it, and a duration measured across that step is negative or
    // enormous — reported, either way, as a latency. is_steady is the standard
    // library's own promise that this one cannot.
    static_assert(std::chrono::steady_clock::is_steady, "steady_clock must be steady");
    CHECK(std::chrono::steady_clock::is_steady);
}

// ---------------------------------------------------------------------------
// The rig, vendored
// ---------------------------------------------------------------------------

#include "core/endpoint.hpp"

TEST_CASE("the vendored rig is linkable, and its endpoint parser works") {
    // dariyanaap's vendor-smoke-test.sh has asserted since its M5 that adding
    // this repo as a subdirectory yields two libraries and nothing else. This
    // is the first time anything has depended on that being true.
    //
    // Endpoint is not an arbitrary choice of proof: it is what this server's
    // --listen flag will parse, so unit 1 never writes a host:port parser.
    const dariyanaap::Endpoint parsed = dariyanaap::Endpoint::parse("127.0.0.1:8080");
    CHECK(parsed.host() == "127.0.0.1");
    CHECK(parsed.port() == 8080);
    CHECK(parsed.str() == "127.0.0.1:8080");
}
