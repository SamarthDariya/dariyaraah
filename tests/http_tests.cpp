#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <string_view>

#include "http/request.hpp"

using namespace dariyaraah::http;
using namespace std;

// The request dariyanaap actually sends, which is the only one this server will
// see in anger. Written out rather than built up, so a change to the rig that
// broke framing here would show as a diff in a literal.
static constexpr string_view kRigRequest =
    "GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nUser-Agent: dariyanaap 0.1.0\r\n"
    "Accept: */*\r\n\r\n";

TEST_CASE("a complete header block ends just past the blank line") {
    const optional<size_t> end = find_header_end(kRigRequest);
    REQUIRE(end.has_value());
    // Past, not at: the offset is also how many bytes this request consumed.
    CHECK(*end == kRigRequest.size());
}

TEST_CASE("a header block arriving one byte at a time is not complete early") {
    // The failure this guards against does not appear on loopback, where the
    // whole request lands in one read. It appears on a real network, once.
    for (size_t taken = 0; taken < kRigRequest.size(); ++taken) {
        CHECK_FALSE(find_header_end(kRigRequest.substr(0, taken)).has_value());
    }
    CHECK(find_header_end(kRigRequest).has_value());
}

TEST_CASE("a second request in the buffer does not extend the first") {
    // Keep-alive means two requests can share one read. Framing past the end of
    // the first is how a server starts answering request N with reply N+1 — the
    // desynchronisation unit 0 refuses to guess its way through.
    const string pipelined = string(kRigRequest) + string(kRigRequest);
    const optional<size_t> end = find_header_end(pipelined);
    REQUIRE(end.has_value());
    CHECK(*end == kRigRequest.size());
}
