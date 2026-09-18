// dariyaraah — one endpoint, one 20ms database call, one thread per connection.
//
//     dariyaraah                    127.0.0.1:8080
//     dariyaraah --port 0           kernel-chosen port, printed below
//     dariyaraah --host 0.0.0.0
//
// The bound address is the first line of stdout and is flushed, so a sweep
// script can read the port back after asking for an ephemeral one. Same shape
// as dariyanaap-null's, so the same awk line parses both.

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
                  "  --port P     0 for a kernel-chosen port (default 8080)\n",
                  stdout);
            return 0;
        }
        const Flags flags = Flags::parse(argc, argv, {"host", "port"});
        const string host = flags.text("host", "127.0.0.1");
        const uint64_t port = flags.number("port", 8080);
        if (port > 65535) {
            throw UsageError("--port must be at most 65535");
        }

        Server server(host, static_cast<uint16_t>(port));
        printf("listening %s:%u  GET / -> 200 after %lldms, thread per connection\n",
               host.c_str(), server.port(),
               static_cast<long long>(kDatabaseDelay.count()));
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
