#include "http/response.hpp"

using namespace std;

namespace dariyaraah::http {

void write_response(const Response& response, string& out) {
    out.clear();
    out += "HTTP/1.1 ";
    out += to_string(response.status);
    out += ' ';
    out += response.reason;
    out += "\r\nContent-Length: ";
    out += to_string(response.body.size());
    out += "\r\n\r\n";
    out += response.body;
}

}  // namespace dariyaraah::http
