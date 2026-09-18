#include "server/event_loop.hpp"

#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>

#include "core/errors.hpp"

using namespace std;

namespace dariyaraah {
namespace {

constexpr int kMaxEventsPerWake = 256;

// strerror() is not thread-safe — it may hand back a pointer into a shared
// static buffer — and this loop runs alongside the accept thread and, in the
// pool model, N workers. generic_category().message() returns a std::string and
// is safe. dariyanaap uses strerror in places; this is not the repo to copy
// that from.
dariyanaap::IoError failure(const char* what, int code) {
    return dariyanaap::IoError(string(what) + ": " + generic_category().message(code));
}

}  // namespace

EventLoop::EventLoop() : kq_(kqueue()) {
    if (kq_ < 0) {
        throw failure("kqueue", errno);
    }
    ready_.reserve(kMaxEventsPerWake);
}

EventLoop::~EventLoop() {
    if (kq_ >= 0) {
        close(kq_);
    }
}

void EventLoop::watch_read(int fd) {
    struct kevent change;
    EV_SET(&change, static_cast<uintptr_t>(fd), EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (kevent(kq_, &change, 1, nullptr, 0, nullptr) < 0) {
        throw failure("kevent EV_ADD", errno);
    }
}

void EventLoop::unwatch_read(int fd) {
    struct kevent change;
    EV_SET(&change, static_cast<uintptr_t>(fd), EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    // ENOENT means it was already gone — closing a descriptor deregisters it,
    // so losing the race with a hang-up is ordinary rather than an error.
    (void)kevent(kq_, &change, 1, nullptr, 0, nullptr);
}

const vector<Event>& EventLoop::wait(dariyanaap::Millis timeout) {
    struct kevent events[kMaxEventsPerWake];
    const timespec deadline{timeout.count() / 1000,
                            (timeout.count() % 1000) * 1'000'000};

    const int count = kevent(kq_, nullptr, 0, events, kMaxEventsPerWake, &deadline);
    ready_.clear();
    if (count < 0) {
        // EINTR is a signal arriving, not a failure. Returning nothing lets the
        // caller loop round and ask again, which is what it would do anyway.
        if (errno == EINTR) {
            return ready_;
        }
        throw failure("kevent wait", errno);
    }

    for (int i = 0; i < count; ++i) {
        ready_.push_back({events[i].ident, events[i].filter == EVFILT_TIMER
                                               ? EventKind::Timer
                                               : EventKind::Readable});
    }
    return ready_;
}

}  // namespace dariyaraah
