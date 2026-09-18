# dariyaraah

> *"Dariya rāh"* — Dariya's path.

Unit **1** of the [`builds/`](../../../hld/builds/README.md) HLD track: one HTTP endpoint, one fake
database call that sleeps 20ms, and three ways of serving it — thread-per-connection, a bounded pool
behind a queue, and an event loop. The point is not the server. The point is the concurrency at which
each one stops keeping up, and why.

Measured with [dariyanaap](../dariyanaap), unit 0, vendored here as a submodule.

Sibling projects: [dariyanache](https://github.com/SamarthDariya/DariyanAche) (Redis clone, Go),
[dariyakyu](../dariyakyu) (Kafka-style commit log, C++).

---

**Where to look:** [The question](#the-question) · [Build](#build) · [Status](#status) ·
[Checkpoints](#checkpoints) — and [DESIGN.md](DESIGN.md) for why anything is the way it is,
[BREAK.md](BREAK.md) for what was predicted, what was measured, and what the predictions got wrong.

---

## The question

> Where does a millisecond go, and at what concurrency does p99 stop resembling p50?

A handler that sleeps 20ms serves exactly `threads ÷ 0.02s` requests per second and not one more,
whatever the CPU is doing. That is Little's law, and this repo exists to hit it rather than read it.

There is a genuine disagreement to settle. The track brief predicts that "p50 stays ~20ms while p99
goes to hundreds of ms". Unit 0's E2 measured the opposite about a thread-per-connection program —
past saturation p50 and p99 rise *together*, because queueing delays every request equally — and
named this repo while saying so. [DESIGN.md decision 3](DESIGN.md) gives the hypothesis that
reconciles them, and BREAK.md's E1 and E2 tested it.

**Answered: p99 does not detach from p50.** Closed-loop, the ratio climbs from 1.04 to 1.48 across
1 → 3,000 connections. Open-loop under sustained overload it converges on 2 — with *both* percentiles
leaving 20ms together, p50 at 822ms and p99 at 1,627ms. Unit 0's E2 was right. The brief is describing
a **transient**: a queue that forms and drains, which is unit 0's E3 stall rather than a ramp.

And a methodological finding that nearly went the other way: the first sweep reported p99 = 63.18ms at
500 connections, a ratio of 2.47, which reads exactly like the detachment. Five repeats gave 28.97,
30.15, 28.97, 29.23, 29.36. **The effect being looked for was the same size as the run-to-run noise**,
so a single sweep could not have answered the question in either direction.

---

## Build

```sh
git clone --recurse-submodules https://github.com/SamarthDariya/dariyaraah.git
# already cloned without it:
git submodule update --init --recursive

cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or in one command, which is what the inner loop actually uses:

```sh
./scripts/check.sh          # build + test
./scripts/check.sh --all    # also ASan/UBSan and TSan — the gate for "done"
```

Requires a C++20 compiler (developed against Apple Clang 17) and CMake 3.20+. doctest is fetched at
configure time; dariyanaap arrives as a submodule. Nothing else.

**Platform:** macOS, so `kqueue` rather than `epoll` at M4. `kern.ipc.somaxconn` is 128 here and
macOS clamps `listen()` to it silently, which matters the moment the ramp asks for 500 connections —
see DESIGN.md.

---

## Status

**M2 complete. It serves, it has been measured, and the standing question is settled.**

| Milestone | What lands | Status |
|---|---|---|
| M0 — Skeleton | CMake, doctest, sanitizers, the rig vendored, `check.sh` | ✅ |
| M1 — The naive server | accept loop, thread per connection, minimal HTTP/1.1, `GET /` → 20ms → 200 | ✅ |
| M2 — The ramp | predictions committed first, then E1 and E2 | ✅ |
| M3 — Bounded pool | a fixed pool behind a request queue, same ramps, E3 | |
| M4 — Event loop | one thread, non-blocking sockets, `kqueue`, E4 | |
| M5 — Write-up | Little's law derived, DESIGN/BREAK/README finished | |

---

## Running it

```sh
./build/dariyaraah --port 0          # kernel-chosen port, printed on line one
curl -D- http://127.0.0.1:<port>/    # 200 OK, 20ms later
```

```
listening 127.0.0.1:57468  GET / -> 200 after 20ms, thread per connection
```

`--port 0` matters more than it looks: the first line is flushed in the same shape
`dariyanaap-null` prints, so M2's sweep script reads the port back out of it and two runs on one
machine cannot collide.

---

## Checkpoints

Each milestone ends in something runnable or measurable. Unlike the earlier repos in this series,
work lands directly on `main` in small commits rather than on a `feature/*` branch merged by PR — the
unit is two days long, nobody is reviewing a pull request, and a branch per milestone would be
ceremony around a single author. The commits are the review surface instead, so they are kept small
enough to read one at a time.

### M0 — Skeleton ✅
- [x] CMake, C++20, `RelWithDebInfo` by default — no number comes from a `-O0` build
- [x] `-Werror` unconditionally: this repo is a leaf, so there is no parent build to break
- [x] `-Wconversion` on, because this repo's arithmetic converts between counts and durations
- [x] doctest fetched header-only, sidestepping the CMake 4 / `cmake_minimum_required(3.0)` trap
- [x] ASan/UBSan and TSan build options, mutually exclusive, with `assert()` kept live
- [x] `dariyanaap` vendored at the commit that splits schedule lag by cause — a prerequisite, since
      without it every open-loop run past the knee reports **VOID**
- [x] the vendoring claim **checked rather than trusted**: only `build_tests` is produced
- [x] first suite tests the assumptions, not the code — C++20 really on, `steady_clock` really steady
- [x] `scripts/check.sh` — build, test, sanitizers, and the `system_clock` ban in one command

### M1 — The naive server ✅
- [x] `find_header_end` — framing, with the byte-at-a-time and pipelined cases tested
- [x] `parse_request_line` — rejects rather than guesses; no opinion about methods or versions
- [x] `write_response` — `Content-Length` only, and **no `Date` header**: the 20ms is meant to be the
      only thing on the path
- [x] responses checked against `dariyanaap::Http11Get`, the parser that will actually read them
- [x] `handle()` is a free function `Request -> Response` that cannot see a socket — the seam that
      makes M1, M3 and M4 comparable rather than three different programs
- [x] the 20ms sleep is **after routing**, and tested to be: a run pointed at the wrong path returns
      in microseconds instead of looking like a fast server
- [x] `serve_connection` never throws — an exception out of a thread's entry point is `std::terminate`
- [x] one `Server`, one binary; M3 and M4 add a flag rather than an executable
- [x] end-to-end: the rig against a real server, with the Little's law ceiling asserted
- [x] green under plain, ASan/UBSan and TSan

**Nothing has been benchmarked.** Track rule 4 says the prediction is written before the run, so
running the ramp before BREAK.md has predictions in it would spend the experiment to satisfy
curiosity. The only number produced so far is the end-to-end test's ceiling, which is arithmetic.

### M2 — The ramp ✅
- [x] predictions committed **before** the sweep script existed
- [x] `scripts/e1-ramp.sh` — closed-loop, 1 → 3,000 connections, one server per sweep
- [x] `scripts/e2-rate.sh` — open-loop, connections fixed, rate swept through the knee
- [x] **throughput linear to 2,000 connections**, then flat at ~74,500 rps; Little's law within 3%
- [x] **`sleep_for(20ms)` measured at 23.4ms** — 23.4 of the 24.8ms a request takes is the timer, and
      ~1.4ms is syscalls, parsing, the thread wake and loopback TCP put together
- [x] the apparent collapse at 4,000 connections **traced to the rig**, which does 57,784 rps there
      against a target that does nothing
- [x] five repeats at 500 connections, because the first sweep's p99 was a 1-in-7 outlier that would
      have become the finding
- [x] open-loop's `rig_lag` at low rates identified as the instrument's, not the server's — the
      reason unit 0's M6 was worth reopening a closed repo for

---

## What unit 0 says to know before reading any number here

| | |
|---|---|
| Rig peak | **132,834 rps** at 32 connections — a target measuring near 130k is measuring the rig |
| Rig p99 floor | **51.7 µs** — nothing here can be measured as faster |
| Open-loop ceiling | **`connections ÷ service_time`**. No pipelining; the excess queues as `connection_wait` and does *not* void the run |
| The rig's own latency | E2's 4.55ms p50 at 500 connections is Little's law **at 107k rps**, not a per-connection tax. Against a 20ms target it contributes sub-100µs |

---

## Deliberately out of scope

Track rule 5 is "cap the scope", and this unit's stop-here line is short: `GET /` is enough.

| | Why |
|---|---|
| HTTP/2, TLS | the subject is threading models, not protocol surface |
| A routing framework | one endpoint. A router is a different repo's lesson |
| Keep-alive tuning, pipelining | the rig sends one request at a time per connection; matching it is the honest baseline |
| A real database | the 20ms sleep *is* the experiment. A real one adds variance and teaches nothing here |
| Load balancing across instances | that is unit 2, `dariyabaant` |
