#pragma once

#include <string>
#include <string_view>

namespace dariyaraah::http {

// What the handler decided. Borrows its strings, like Request: a response is
// three fields and a body, and none of them need to be owned to be written.
struct Response {
    int status = 200;
    std::string_view reason = "OK";
    std::string_view body;
};

// Serialise into `out`, clearing it first.
//
// The buffer belongs to the caller and is reused for every request on a
// connection. Returning a fresh std::string per response would be one
// allocation per request on the measured path, reported as this service's
// latency — the same reasoning as Request's borrowed fields.
//
// Content-Length only: no chunked encoding, matching dariyanaap's client, which
// reports a chunked response as a protocol error by name rather than
// mis-framing it.
//
// No Connection header, because HTTP/1.1 is persistent by default and saying so
// is noise. No Date and no Server either — a real server sends both, and
// formatting a date per response is real work that would land in the numbers
// while teaching nothing this unit is about.
void write_response(const Response& response, std::string& out);

}  // namespace dariyaraah::http
