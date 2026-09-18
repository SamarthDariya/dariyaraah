#include "server/server.hpp"

#include <thread>
#include <utility>

#include "core/address.hpp"
#include "core/errors.hpp"
#include "core/socket.hpp"
#include "server/connection.hpp"

using namespace std;

namespace dariyaraah {

Server::Server(const dariyanaap::Endpoint& endpoint)
    : listener_(endpoint.port() == 0 ? dariyanaap::Listener::bind_ephemeral(endpoint.host())
                                     : dariyanaap::Listener::bind(endpoint)) {}

void Server::run() {
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
