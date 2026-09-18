#pragma once

#include <cstdint>
#include <vector>

#include "core/units.hpp"

namespace dariyaraah {

// What woke the loop.
//
// `ident` is the socket's descriptor for Readable, and a caller-chosen id for
// Timer — kqueue lets a timer pick its own, so M4's second half can park a
// connection on a deadline and find it again by the same number it watches the
// socket with.
enum class EventKind { Readable, Timer };

struct Event {
    std::uintptr_t ident = 0;
    EventKind kind = EventKind::Readable;
};

// A thin kqueue. Enough for one server, and not a general-purpose reactor.
//
// macOS, so kqueue rather than epoll — a difference the track's platform note
// calls out, and the one place in this repo where the operating system shows
// through. The shape would be the same on Linux with epoll_wait; the flags
// would not.
//
// LEVEL-TRIGGERED, which is the default and is what makes the rest of the
// server simple: "readable" keeps being reported until the data is consumed, so
// a handler that reads once per wake-up and returns cannot lose a request it
// did not finish reading. Edge-triggered would require draining each socket to
// EAGAIN on every event, which is where a subtle event loop stops being
// readable — and readability is the point of this repo.
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Watch a descriptor for readability. Registering twice is harmless.
    void watch_read(int fd);

    // Stop watching. Closing a descriptor removes it from the kqueue anyway,
    // so this is for the case where the socket outlives its interest.
    void unwatch_read(int fd);

    // Block until something is ready, or until `timeout` passes. The returned
    // span is valid until the next call: the buffer belongs to the loop, so a
    // busy server does not allocate once per wake-up.
    const std::vector<Event>& wait(dariyanaap::Millis timeout);

private:
    int kq_ = -1;
    std::vector<Event> ready_;
};

}  // namespace dariyaraah
