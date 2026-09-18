#include "server/connection.hpp"

#include "http/request.hpp"

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

}  // namespace dariyaraah
