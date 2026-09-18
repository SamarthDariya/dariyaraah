// dariyaraah — one endpoint, one 20ms database call, one thread per connection.
//
//     dariyaraah                    127.0.0.1:8080
//     dariyaraah --port 0           kernel-chosen port, printed below
//     dariyaraah --host 0.0.0.0
//     dariyaraah --workers 32       a bounded pool instead of a thread each
//     dariyaraah --event-loop 1     one thread, kqueue, and a blocking handler
//
// The bound address is the first line of stdout and is flushed, so a sweep
// script can read the port back after asking for an ephemeral one. Same shape
// as dariyanaap-null's, so the same awk line parses both.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "core/errors.hpp"
#include "core/flags.hpp"
#include "server/handler.hpp"
#include "server/server.hpp"

using namespace dariyaraah;
using namespace dariyanaap;
using namespace std;

int main(int argc, char** argv) {
    try {
        if (Flags::wants_help(argc, argv)) {
            fputs("usage: dariyaraah [options]\n"
                  "  --host H     bind address (default 127.0.0.1)\n"
                  "  --port P     0 for a kernel-chosen port (default 8080)\n"
                  "  --workers N  bounded pool of N threads; 0 (default) is a\n"
                  "               thread per connection\n"
                  "  --event-loop 1   one thread and kqueue, overriding --workers\n",
                  stdout);
            return 0;
        }
        const Flags flags = Flags::parse(argc, argv, {"host", "port", "workers", "event-loop"});
        const string host = flags.text("host", "127.0.0.1");
        const uint64_t port = flags.number("port", 8080);
        if (port > 65535) {
            throw UsageError("--port must be at most 65535");
        }
        const uint64_t workers = flags.number("workers", 0);
        const bool event_loop = flags.number("event-loop", 0) != 0;

        Server server(host, static_cast<uint16_t>(port), static_cast<size_t>(workers),
                      event_loop);
        // The threading model is printed, not just configured. A sweep that
        // compares two models across runs has to be able to tell from the log
        // which one a given run was, or the comparison is an act of faith.
        const string model = event_loop ? "single-threaded event loop (kqueue)"
                             : workers == 0
                                 ? "thread per connection"
                                 : "pool of " + to_string(workers) + " workers";
        printf("listening %s:%u  GET / -> 200 after %lldms, %s\n", host.c_str(), server.port(),
               static_cast<long long>(kDatabaseDelay.count()),
               model.c_str());
        fflush(stdout);

        server.run();
        return 0;
    } catch (const UsageError& e) {
        fprintf(stderr, "dariyaraah: %s\n", e.what());
        return 2;
    } catch (const Error& e) {
        fprintf(stderr, "dariyaraah: %s\n", e.what());
        return 1;
    }
}
