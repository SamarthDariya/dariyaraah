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

Server::Server(const string& host, uint16_t port, size_t workers, LoopMode loop)
    : workers_(workers),
      loop_(loop),
      listener_(port == 0 ? dariyanaap::Listener::bind_ephemeral(host)
                          : dariyanaap::Listener::bind(dariyanaap::Endpoint(host, port))) {}

void Server::run() {
    if (loop_ != LoopMode::Off) {
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
// Where a connection is up to. Reading is the only state M1 and M3 ever have,
// because there a thread waiting on the database is simply blocked and the
// state lives in its stack frame. One thread serving everything has to write it
// down.
enum class Phase { Reading, WaitingOnDatabase };

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

    Phase phase = Phase::Reading;
    // Bytes of `received` the parked request occupies. Nothing is read into the
    // buffer while parked — the socket is unwatched — so the request is still
    // there, unmoved, when the timer fires.
    size_t length = 0;
};

// One turn: read what is there, and serve a request if a whole one has arrived.
// Returns false when this connection is finished.
//
// The handler BLOCKS for 20ms inside here, which is M4's first experiment. One
// thread plus a blocking handler means one request at a time for the whole
// server, and the point is to measure how bad that is rather than to assume it.
// Read once into the connection's buffer. False when the client hung up.
bool read_more(Parked& parked) {
    assert(!parked.scratch.empty() && "read buffer must have room in it");
    const size_t got = parked.client.read_some({parked.scratch.data(), parked.scratch.size()});
    if (got == 0) {
        return false;
    }
    parked.received.append(parked.scratch.data(), got);
    return true;
}

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

// Answer the request occupying the first `length` bytes, and consume them.
// Returns false when the connection is finished.
bool answer_now(Parked& parked, size_t length) {
    const optional<http::Request> request = http::parse_request_line(parked.received);
    const http::Response response =
        request ? route(*request) : http::Response{400, "Bad Request", ""};
    http::write_response(response, parked.out);
    parked.received.erase(0, length);
    parked.client.write_all({parked.out.data(), parked.out.size()});
    return request.has_value();
}

// Begin on whatever is buffered: park on a timer if answering costs the
// database, answer immediately if it does not, and go back to reading when
// there is no whole request left.
//
// The request is re-parsed when the timer fires rather than being carried
// across the wait. Request borrows from `received`, and a Response may borrow
// from a Request, so holding either across a park would be a lifetime bet on
// what route() happens to return today. Parsing twice costs a scan of a few
// hundred bytes and costs nothing to reason about.
bool begin_next(Parked& parked, EventLoop& loop, int fd) {
    for (;;) {
        const optional<size_t> length = http::find_header_end(parked.received);
        if (!length) {
            parked.phase = Phase::Reading;
            loop.watch_read(fd);
            return parked.received.size() <= kMaxHeaderBytes;
        }

        const optional<http::Request> request = http::parse_request_line(parked.received);
        if (request && needs_database(route(*request))) {
            parked.phase = Phase::WaitingOnDatabase;
            parked.length = *length;
            // Unwatched while waiting, or a level-triggered loop would report
            // this socket readable on every pass and spin at 100% CPU.
            loop.unwatch_read(fd);
            loop.arm_timer(static_cast<uintptr_t>(fd), kDatabaseDelay);
            return true;
        }
        if (!answer_now(parked, *length)) {
            return false;
        }
    }
}

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
            if (found == parked.end()) {
                continue;
            }

            bool alive = false;
            try {
                if (event.kind == EventKind::Timer) {
                    // The database call has returned. Same work the other
                    // models do on a thread; nobody was blocked waiting for it.
                    alive = answer_now(found->second, found->second.length) &&
                            begin_next(found->second, loop, fd);
                } else if (loop_ == LoopMode::DatabaseTimer) {
                    alive = read_more(found->second) && begin_next(found->second, loop, fd);
                } else {
                    alive = serve_one_turn(found->second);
                }
            } catch (const dariyanaap::IoError&) {
                alive = false;
            }

            if (!alive) {
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
