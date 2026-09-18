#include "server/server.hpp"

#include <thread>
#include <utility>

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
        client.set_timeouts(kIdleTimeout, kIdleTimeout);

        // Detached, not joined. A thread per connection means there is no list
        // of them to keep and nothing to join them at: each one ends when its
        // client hangs up. Creating one per connection is not free, and that
        // cost is part of what M2 is measuring rather than something to hide.
        thread(serve_connection, std::move(client)).detach();
    }
}

}  // namespace dariyaraah
