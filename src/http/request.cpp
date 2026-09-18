#include "http/request.hpp"

using namespace std;

namespace dariyaraah::http {

optional<size_t> find_header_end(string_view bytes) {
    const size_t blank = bytes.find("\r\n\r\n");
    if (blank == string_view::npos) {
        return nullopt;
    }
    return blank + 4;
}

optional<Request> parse_request_line(string_view bytes) {
    const size_t eol = bytes.find("\r\n");
    if (eol == string_view::npos) {
        return nullopt;
    }
    const string_view line = bytes.substr(0, eol);

    // Exactly two spaces, and the fields between them. A request line with
    // three is not a request line with an extra field, it is malformed — and
    // "GET /a b HTTP/1.1" reaching a handler as target "/a" would be a request
    // silently served as one nobody sent.
    const size_t first = line.find(' ');
    const size_t second = first == string_view::npos ? first : line.find(' ', first + 1);
    if (second == string_view::npos || line.find(' ', second + 1) != string_view::npos) {
        return nullopt;
    }

    const Request request{line.substr(0, first), line.substr(first + 1, second - first - 1),
                          line.substr(second + 1)};
    if (request.method.empty() || request.target.empty() || request.version.empty()) {
        return nullopt;
    }
    return request;
}

}  // namespace dariyaraah::http
