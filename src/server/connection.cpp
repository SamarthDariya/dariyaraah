#include "server/connection.hpp"

#include "core/errors.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "server/handler.hpp"

using namespace std;

namespace dariyaraah {

optional<size_t> read_request(dariyanaap::Socket& client, string& received,
                              vector<char>& scratch) {
    for (;;) {
        // Checked before reading, not after: the previous request's read may
        // already have delivered this one, and a server that always reads first
        // would block waiting for a request it is holding.
        if (const optional<size_t> end = http::find_header_end(received)) {
            return end;
        }
        if (received.size() > kMaxHeaderBytes) {
            return nullopt;
        }
        const size_t got = client.read_some({scratch.data(), scratch.size()});
        if (got == 0) {
            return nullopt;  // clean hang-up, which every run ends with
        }
        received.append(scratch.data(), got);
    }
}

void serve_connection(dariyanaap::Socket client) {
    string received;
    string out;
    vector<char> scratch(kReadChunkBytes);

    try {
        for (;;) {
            const optional<size_t> length = read_request(client, received, scratch);
            if (!length) {
                return;
            }

            const optional<http::Request> request = http::parse_request_line(received);
            const http::Response response =
                request ? handle(*request) : http::Response{400, "Bad Request", ""};

            // Serialise before erasing. Request borrows from `received`, and so
            // could a Response — erasing first would leave write_response
            // copying out of a buffer that had moved under it.
            http::write_response(response, out);
            received.erase(0, *length);
            client.write_all({out.data(), out.size()});

            // A client whose request line we could not parse gets told so, and
            // then gets no further turns: the stream is of unknown shape, and
            // reading on would be guessing where the next request starts.
            if (!request) {
                return;
            }
        }
    } catch (const dariyanaap::IoError&) {
        // Read timeout, connection reset, or the client vanished mid-response.
        // This connection is finished. The server is not.
    } catch (...) {
        // Nothing else should reach here, and if it does it must still not
        // escape: see the header. Losing one connection beats losing the run.
    }
}

}  // namespace dariyaraah
