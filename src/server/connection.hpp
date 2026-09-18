#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/socket.hpp"

namespace dariyaraah {

// How much this server will accumulate looking for the end of a header block.
//
// The read timeout already bounds how long a silent client can hold a thread.
// This bounds a talkative one: without it, a client that sends headers forever
// and never a blank line grows the buffer until the process dies. Sixteen
// kilobytes is far past anything dariyanaap sends and far short of a problem.
inline constexpr std::size_t kMaxHeaderBytes = 16 * 1024;

// Read from `client` until `received` holds at least one whole request, and
// return that request's length in bytes.
//
// nullopt means stop talking to this client: it hung up cleanly, or it went
// past the cap above. Both end the connection, and neither is an error worth a
// response — there is nobody left to read one, or nothing parseable to answer.
//
// `received` and `scratch` belong to the caller and are reused for every
// request on the connection. A fresh buffer per request would put the allocator
// on the measured path and report it as this service's latency.
//
// Bytes past the returned length stay in `received` on purpose. Two requests
// can arrive in one read, and dropping the tail would lose the second one.
std::optional<std::size_t> read_request(dariyanaap::Socket& client, std::string& received,
                                        std::vector<char>& scratch);

// One read per request in the common case: a header block is well under this.
inline constexpr std::size_t kReadChunkBytes = 8 * 1024;

// Serve requests on `client` until it goes away. This is the body of a thread,
// and M1's entire threading model is that there is one of these per connection.
//
// Takes the socket by value and owns it: the thread outlives the accept loop's
// stack frame, and a reference would dangle the moment the loop came round
// again. Closing is the destructor's job.
//
// Never throws. That is not politeness — an exception escaping a thread's
// entry point calls std::terminate, so one client hitting a read timeout would
// take down a server that is otherwise serving 499 other connections perfectly.
void serve_connection(dariyanaap::Socket client);

}  // namespace dariyaraah
