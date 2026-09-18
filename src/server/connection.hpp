#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/socket.hpp"
#include "http/response.hpp"

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

// Send one response, after giving the fault library its say.
//
// Every model funnels through here, which is the point: unit 2 makes one of
// three backends ten times slower, and a knob that only affected one threading
// model would make that experiment a comparison of two different things.
//
// Returns false when the fault library says to drop the response, and the
// caller then closes the connection. Dropping is the caller's decision to make
// — unit 0 is explicit that the library will not make it — and closing is the
// only coherent one here: these connections are keep-alive, so a response
// silently withheld would leave the client's NEXT request answered by the reply
// after it, which is the desynchronisation both sides of this series keep
// refusing to guess through.
//
// KNOWN LIMIT: fault::before_response() blocks, so injected latency stalls the
// event loop exactly the way E4a's handler did. Useful in the threaded models,
// which is what unit 2 runs its backends as. Making it a timer would mean
// teaching unit 0's fault library about kqueue, and that is a larger idea than
// unit 2 needs.
bool send_response(dariyanaap::Socket& client, const http::Response& response,
                   std::string& out);

}  // namespace dariyaraah
