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
