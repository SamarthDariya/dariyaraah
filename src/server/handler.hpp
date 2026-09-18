#pragma once

#include "core/units.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

namespace dariyaraah {

// The fake database call, and the entire subject of this repo.
//
// Twenty milliseconds is not a tuning parameter. It is large enough to dwarf
// everything else in the request path — parsing, the syscalls, the scheduler —
// so that throughput is visibly `threads / 0.02s` and nothing else, and small
// enough that a ramp to 500 connections finishes in seconds.
inline constexpr dariyanaap::Millis kDatabaseDelay{20};

// What this server does, as a function of what was asked.
//
// It does not know a socket exists, and that is the load-bearing property. M1
// calls it from a thread per connection, M3 from a bounded pool and M4 from an
// event loop, and the experiment those three milestones run is only valid if
// the work being done is identical in all of them. A handler reachable only
// through a connection object would have to be reimplemented once per threading
// model, and the comparison would then be between three different programs.
//
// Yes, this is designing for M3 and M4 at M1, against track rule 1. The rule is
// about not reaching for the *fix* early; the seam that makes the fixes
// comparable is not the fix.
http::Response handle(const http::Request& request);

// ---------------------------------------------------------------------------
// The same work, with the waiting taken out of it
// ---------------------------------------------------------------------------
//
// M4's second half cannot call handle(): a single thread that sleeps for 20ms
// is a server that does one thing at a time, which E4a measured at 42 rps. The
// event loop has to arm a timer and go and do something else.
//
// So the delay is separated from the work instead of the handler being
// rewritten for one model. `route` decides what the answer is; `needs_database`
// says whether producing it costs the 20ms; and each model waits in its own
// way — a sleeping thread, a worker off a queue, a kqueue timer.
//
// That keeps decision 7 intact where it matters. The fixed point was never the
// function's name, it was that every model does the SAME WORK and differs only
// in who waits. Splitting `handle` into "the work" and "the wait" states that
// out loud rather than leaving it implied by a sleep in the middle.

// What to answer. Pure: no sleep, no socket, no clock.
http::Response route(const http::Request& request);

// Does producing this answer cost the database call?
//
// Routing happens first, so a 404 does not pay it — the property M1's tests
// pinned, and the reason a run pointed at the wrong path returns in
// microseconds instead of looking like a fast server.
bool needs_database(const http::Response& response);

}  // namespace dariyaraah
