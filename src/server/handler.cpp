#include "server/handler.hpp"

#include <thread>

using namespace std;

namespace dariyaraah {

http::Response route(const http::Request& request) {
    if (request.method != "GET") {
        return {405, "Method Not Allowed", ""};
    }
    if (request.target != "/") {
        return {404, "Not Found", ""};
    }
    return {200, "OK", "dariyaraah\n"};
}

bool needs_database(const http::Response& response) {
    return response.status == 200;
}

http::Response handle(const http::Request& request) {
    // Routing before the database, as it would be in a real service, so a run
    // pointed at the wrong path returns in microseconds rather than 20ms —
    // a misconfiguration that announces itself.
    const http::Response response = route(request);
    if (needs_database(response)) {
        // Decision 1: a real sleep, on the thread that is serving this request.
        // Which thread that is, is the whole subject of the unit.
        this_thread::sleep_for(kDatabaseDelay);
    }
    return response;
}

}  // namespace dariyaraah
