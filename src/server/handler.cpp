#include "server/handler.hpp"

#include <thread>

using namespace std;

namespace dariyaraah {

http::Response handle(const http::Request& request) {
    // Routing happens before the database, as it would in a real service. It
    // also means a run pointed at the wrong path returns in microseconds rather
    // than 20ms, which is a misconfiguration that announces itself.
    if (request.method != "GET") {
        return {405, "Method Not Allowed", ""};
    }
    if (request.target != "/") {
        return {404, "Not Found", ""};
    }

    // Decision 1: a real sleep, in the handler, where the work would be.
    this_thread::sleep_for(kDatabaseDelay);
    return {200, "OK", "dariyaraah\n"};
}

}  // namespace dariyaraah
