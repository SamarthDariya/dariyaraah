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

}  // namespace dariyaraah
