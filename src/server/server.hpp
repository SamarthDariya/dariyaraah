#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "core/endpoint.hpp"
#include "core/listener.hpp"
#include "core/units.hpp"

namespace dariyaraah {

// How long an accepted connection may say nothing before it is dropped.
//
// Accepted sockets inherit no timeouts from the listener, and without one a
// client that connects and never speaks holds a thread for the life of the
// process. At M1 a thread is the whole of the server's capacity, so that is not
// a leak, it is a denial of service with one connection.
inline constexpr dariyanaap::Millis kIdleTimeout{30'000};

// A thread per connection, which is the design under test rather than a
// recommendation.
//
// It is also not a straw man. Apache prefork is this, and it is a perfectly
// good server right up to the concurrency where the scheduler stops coping —
// finding that concurrency, and the number just before it, is M2's whole job.
class Server {
public:
    // `port` of 0 asks the kernel to choose one, which port() then reports.
    //
    // Host and port rather than an Endpoint, because Endpoint cannot express
    // this: it rejects port 0 in its constructor, and rightly — as a
    // destination, port 0 means nothing. Only a bind address can mean "you
    // pick". An earlier version of this took an Endpoint and branched on
    // port() == 0, which was a branch nothing could reach.
    Server(const std::string& host, std::uint16_t port);

    // The port actually bound, for when port 0 was asked for.
    std::uint16_t port() const { return listener_.port(); }

    // Accept forever, handing each connection to a thread. Returns only when
    // the listener stops accepting.
    void run();

    // Make run() return. Safe to call from another thread, which is the only
    // place it can be called from, since run() does not come back on its own.
    //
    // There is no portable way to interrupt a blocking accept(), and closing
    // the listener's descriptor underneath it is the kind of undefined
    // behaviour that works until it does not. So this connects to the listener
    // to give accept() something to return, and run() checks the flag before
    // doing anything with it. Unit 0's test target settled on the same trick.
    void stop();

private:
    std::atomic<bool> stopping_{false};
    dariyanaap::Listener listener_;
};

}  // namespace dariyaraah
