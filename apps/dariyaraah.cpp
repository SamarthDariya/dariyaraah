// dariyaraah — one endpoint, one 20ms database call, one thread per connection.
//
//     dariyaraah                    127.0.0.1:8080
//     dariyaraah --port 0           kernel-chosen port, printed below
//     dariyaraah --host 0.0.0.0
//     dariyaraah --workers 32       a bounded pool instead of a thread each
//     dariyaraah --event-loop 1     one thread, kqueue, and a blocking handler
//     DARIYANAAP_FAULT_LATENCY_MS=234 dariyaraah     ten times slower, for unit 2
//     dariyaraah --fault-port 7777  change faults while it is running
//
// The bound address is the first line of stdout and is flushed, so a sweep
// script can read the port back after asking for an ephemeral one. Same shape
// as dariyanaap-null's, so the same awk line parses both.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <memory>

#include "core/errors.hpp"
#include "core/flags.hpp"
#include "fault/control.hpp"
#include "fault/knobs.hpp"
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
                  "  --event-loop N   one thread and kqueue, overriding --workers:\n"
                  "               1 = the handler still blocks (E4a)\n"
                  "               2 = the database call becomes a timer (E4b)\n"
                  "  --fault-port P   serve dariyanaap's fault control socket here\n"
                  "\n"
                  "Faults are also read from the environment at startup:\n"
                  "  DARIYANAAP_FAULT_LATENCY_MS / _JITTER_MS / _DROP / _HANG\n"
                  "This is EXTRA latency, on top of the 20ms database call.\n",
                  stdout);
            return 0;
        }
        // Throws on a value it cannot read, rather than running a different
        // experiment from the one that was asked for.
        fault::load_from_env();

        const Flags flags =
            Flags::parse(argc, argv, {"host", "port", "workers", "event-loop", "fault-port"});
        const string host = flags.text("host", "127.0.0.1");
        const uint64_t port = flags.number("port", 8080);
        if (port > 65535) {
            throw UsageError("--port must be at most 65535");
        }
        const uint64_t workers = flags.number("workers", 0);
        // 1 and 2 are E4a and E4b, and the flag says so in --help. Both are
        // kept: they are separate findings, and deleting the slow one would
        // keep the conclusion while throwing away the evidence.
        const uint64_t loop_choice = flags.number("event-loop", 0);
        if (loop_choice > 2) {
            throw UsageError("--event-loop must be 0, 1 or 2");
        }
        const LoopMode loop = loop_choice == 0   ? LoopMode::Off
                              : loop_choice == 1 ? LoopMode::BlockingHandler
                                                 : LoopMode::DatabaseTimer;

        const uint64_t fault_port = flags.number("fault-port", 0);
        if (fault_port > 65535) {
            throw UsageError("--fault-port must be at most 65535");
        }
        // Held for the life of the process. Unit 2 needs a backend that can be
        // made to hang in the MIDDLE of a run — "restart a backend and watch it
        // get slammed" is not expressible as startup configuration.
        unique_ptr<fault::ControlServer> control;
        if (fault_port > 0) {
            control = make_unique<fault::ControlServer>("127.0.0.1",
                                                        static_cast<uint16_t>(fault_port));
        }

        Server server(host, static_cast<uint16_t>(port), static_cast<size_t>(workers), loop);
        // The threading model is printed, not just configured. A sweep that
        // compares two models across runs has to be able to tell from the log
        // which one a given run was, or the comparison is an act of faith.
        const string model =
            loop == LoopMode::BlockingHandler ? "event loop, blocking handler (kqueue)"
            : loop == LoopMode::DatabaseTimer ? "event loop, database as a timer (kqueue)"
            : workers == 0                    ? "thread per connection"
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
