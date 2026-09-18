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

}  // namespace dariyaraah::http
