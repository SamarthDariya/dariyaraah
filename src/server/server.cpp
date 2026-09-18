#include "server/server.hpp"

#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "core/address.hpp"
#include "core/errors.hpp"
#include "core/socket.hpp"
#include "server/connection.hpp"
#include "server/queue.hpp"

using namespace std;

namespace dariyaraah {

Server::Server(const string& host, uint16_t port, size_t workers)
    : workers_(workers),
      listener_(port == 0 ? dariyanaap::Listener::bind_ephemeral(host)
                          : dariyanaap::Listener::bind(dariyanaap::Endpoint(host, port))) {}

void Server::run() {
    if (workers_ == 0) {
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
