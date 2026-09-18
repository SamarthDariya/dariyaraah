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

}  // namespace dariyaraah::http
