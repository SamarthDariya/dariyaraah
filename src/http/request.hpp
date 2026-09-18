#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace dariyaraah::http {

// Where a request ends.
//
// HTTP/1.1 frames a GET with a blank line and nothing else: no length prefix,
// no sentinel byte, just CRLF CRLF. Which means a server cannot know a request
// has arrived until it has seen those four bytes, and must be prepared for them
// to land across two reads — or across four, one byte at a time, because TCP
// is a stream and the segment boundaries are the network's business.
//
// Returns the offset just PAST the blank line, so it doubles as "how many bytes
// of the buffer this request consumed" — which is what a keep-alive connection
// needs in order to find the next one.
//
// Scans the whole buffer every time it is called rather than resuming where the
// last scan stopped. That is quadratic in the number of reads a header block is
// split across, and it is the naive version on purpose (track rule 1): a header
// block is under a kilobyte and arrives in one read essentially always, so the
// cost is theoretical until something makes it real. If something does, this
// comment is where to start.
std::optional<std::size_t> find_header_end(std::string_view bytes);

// A parsed request line. Headers are deliberately absent: this server answers
// one path and needs none of them, and parsing what it will not read is the
// kind of completeness track rule 5 exists to refuse.
//
// Every field BORROWS from the caller's buffer, which must outlive the Request.
// Copying three substrings per request would put the allocator on the measured
// path and then report it as this service's latency — the same reasoning that
// keeps dariyanaap's read buffers owned by the connection rather than the
// exchange.
struct Request {
    std::string_view method;
    std::string_view target;
    std::string_view version;
};

// Parse the first line of `bytes`. nullopt means the line is not a request
// line — not that it has not arrived, which find_header_end answers.
//
// Structure only: a method is whatever came before the first space, and no
// check is made that it is a method anyone has heard of, or that the version is
// one this server speaks. Those are the handler's business, and a parser with
// opinions about them is a parser that has to be edited to accept a new method.
std::optional<Request> parse_request_line(std::string_view bytes);

}  // namespace dariyaraah::http
