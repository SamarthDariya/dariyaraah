#include "server/server.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/address.hpp"
#include "core/errors.hpp"
#include "core/socket.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "server/connection.hpp"
#include "server/event_loop.hpp"
#include "server/handler.hpp"
#include "server/queue.hpp"

using namespace std;

namespace dariyaraah {

Server::Server(const string& host, uint16_t port, size_t workers, bool event_loop)
    : workers_(workers),
      event_loop_(event_loop),
      listener_(port == 0 ? dariyanaap::Listener::bind_ephemeral(host)
                          : dariyanaap::Listener::bind(dariyanaap::Endpoint(host, port))) {}

void Server::run() {
    if (event_loop_) {
        run_event_loop();
    } else if (workers_ == 0) {
        run_thread_per_connection();
    } else {
        run_pool();
    }
}

void Server::run_thread_per_connection() {
    for (;;) {
        dariyanaap::Socket client = listener_.accept();
        if (stopping_.load(memory_order_relaxed)) {
            return;  // stop()'s own connection, not a client
        }
        client.set_timeouts(kIdleTimeout, kIdleTimeout);

        // Detached, not joined. A thread per connection means there is no list
        // of them to keep and nothing to join them at: each one ends when its
        // client hangs up. Creating one per connection is not free, and that
        // cost is part of what M2 is measuring rather than something to hide.
        thread(serve_connection, std::move(client)).detach();
    }
}

void Server::run_pool() {
    BlockingQueue<dariyanaap::Socket> waiting(workers_ * kQueuePerWorker);

    vector<thread> pool;
    pool.reserve(workers_);
    for (size_t i = 0; i < workers_; ++i) {
        pool.emplace_back([&waiting] {
            // A worker serves one connection to completion, then takes the
            // next. With keep-alive that means it is held for the whole life of
            // the connection, which is M3's finding rather than an oversight.
            while (optional<dariyanaap::Socket> client = waiting.pop()) {
                serve_connection(std::move(*client));
            }
        });
    }

    for (;;) {
        dariyanaap::Socket client = listener_.accept();
        if (stopping_.load(memory_order_relaxed)) {
            break;
        }
        client.set_timeouts(kIdleTimeout, kIdleTimeout);
        waiting.push(std::move(client));  // blocks when full: that is the point
    }

    waiting.close();
    for (thread& worker : pool) {
        worker.join();
    }
}

namespace {

// Everything one connection needs, for a loop that owns them all at once.
// M1 and M3 keep this on a thread's stack; here it has to be somewhere the
// single thread can put it down and pick up again.
struct Parked {
    // A constructor rather than aggregate initialisation, and not for style.
    // Parked{socket, {}, {}, {}} compiles, and the trailing {} overrides
    // scratch's size with zero — after which read_some returns zero bytes,
    // which this loop reads as a clean hang-up, and the server closes every
    // connection the moment it arrives while reporting nothing wrong. That was
    // a real bug for the length of one commit. A constructor makes the shape
    // that caused it impossible to write.
    explicit Parked(dariyanaap::Socket socket) : client(std::move(socket)) {}

    dariyanaap::Socket client;
    string received;
    string out;
    vector<char> scratch = vector<char>(kReadChunkBytes);
};

// One turn: read what is there, and serve a request if a whole one has arrived.
// Returns false when this connection is finished.
//
// The handler BLOCKS for 20ms inside here, which is M4's first experiment. One
// thread plus a blocking handler means one request at a time for the whole
// server, and the point is to measure how bad that is rather than to assume it.
bool serve_one_turn(Parked& parked) {
    try {
        // Asserted rather than assumed: a zero-length buffer makes read_some
        // return zero, which is indistinguishable here from a client hanging
        // up, and the server then closes every connection the instant it
        // arrives while reporting nothing wrong.
        assert(!parked.scratch.empty() && "read buffer must have room in it");
        const size_t got = parked.client.read_some(
            {parked.scratch.data(), parked.scratch.size()});
        if (got == 0) {
            return false;  // clean hang-up
        }
        parked.received.append(parked.scratch.data(), got);

        const optional<size_t> length = http::find_header_end(parked.received);
        if (!length) {
            return parked.received.size() <= kMaxHeaderBytes;
        }

        const optional<http::Request> request = http::parse_request_line(parked.received);
        const http::Response response =
            request ? handle(*request) : http::Response{400, "Bad Request", ""};
        http::write_response(response, parked.out);
        parked.received.erase(0, *length);
        parked.client.write_all({parked.out.data(), parked.out.size()});
        return request.has_value();
    } catch (const dariyanaap::IoError&) {
        return false;
    }
}

}  // namespace

void Server::run_event_loop() {
    EventLoop loop;
    loop.watch_read(listener_.fd());
    unordered_map<int, Parked> parked;

    while (!stopping_.load(memory_order_relaxed)) {
        for (const Event& event : loop.wait(kPollInterval)) {
            const int fd = static_cast<int>(event.ident);
            if (fd == listener_.fd()) {
                dariyanaap::Socket client = listener_.accept();
                if (stopping_.load(memory_order_relaxed)) {
                    return;
                }
                client.set_timeouts(kIdleTimeout, kIdleTimeout);
                const int client_fd = client.fd();
                loop.watch_read(client_fd);
                parked.emplace(client_fd, std::move(client));
                continue;
            }

            const auto found = parked.find(fd);
            if (found != parked.end() && !serve_one_turn(found->second)) {
                loop.unwatch_read(fd);
                parked.erase(found);  // the Socket's destructor closes it
            }
        }
    }
}

void Server::stop() {
    stopping_.store(true, memory_order_relaxed);
    try {
        const dariyanaap::Socket poke = dariyanaap::Socket::connect_any(
            dariyanaap::resolve(dariyanaap::Endpoint("127.0.0.1", listener_.port())),
            dariyanaap::Millis(500));
    } catch (const dariyanaap::IoError&) {
        // run() was not in accept(), or is already gone. Either way the flag is
        // set and it will not take another connection.
    }
}

}  // namespace dariyaraah
