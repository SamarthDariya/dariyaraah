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

## Reproducing the measurements

Every table in [BREAK.md](BREAK.md) comes from one of these. They start a server on an
ephemeral port, drive it with the vendored rig, and write CSV to `runs/`.

```sh
./scripts/e1-ramp.sh     # E1  closed-loop, connections 1 -> 500
./scripts/e2-rate.sh     # E2  open-loop, rate swept through the knee
./scripts/e3-pool.sh     # E3  32 workers fixed, connections ramped past them
./scripts/e4-loop.sh     # E4a event loop, blocking handler
MODE=2 ./scripts/e4-loop.sh   # E4b event loop, the wait as a kqueue timer
```

Each takes environment overrides rather than flags, because they get re-run with a single step over
and over — which is how E1's 1-in-7 outlier was caught:

```sh
STEPS="500" DURATION=3000 ./scripts/e1-ramp.sh     # one step, repeatedly
WORKERS=8 ./scripts/e3-pool.sh                     # a different pool size
RATES="400 800 1200" ./scripts/e2-rate.sh
```

They build the rig's CLI themselves on first run. It is not built by `cmake --build build`: unit 0
suppresses its own executables when it is vendored, which is exactly what it should do and means its
CLI needs building once, separately. `scripts/rig.sh` does that rather than leaving a fresh clone to
discover it.

**Read the error counters before the throughput column.** E3 and E4a are both runs where the summary
numbers improve while the service collapses, and the per-kind counts are the only place that shows.

---

## Status

**Complete — M0 through M5, E1 through E4b.** M6 adds the fault knobs unit 2 needs.

| Milestone | What lands | Status |
|---|---|---|
| M0 — Skeleton | CMake, doctest, sanitizers, the rig vendored, `check.sh` | ✅ |
| M1 — The naive server | accept loop, thread per connection, minimal HTTP/1.1, `GET /` → 20ms → 200 | ✅ |
| M2 — The ramp | predictions committed first, then E1 and E2 | ✅ |
| M3 — Bounded pool | a fixed pool behind a request queue, same ramps, E3 | ✅ |
| M4 — Event loop | one thread, `kqueue`, E4a blocking and E4b with the wait as a timer | ✅ |
| M5 — Write-up | Little's law derived, DESIGN/BREAK/README finished | ✅ |
| M6 — Fault knobs | `dariyanaap::fault` on the response path, for unit 2 | ✅ |

---

## What it answers

**L = λ × W.** Requests in flight = throughput × latency. It holds within 4% across all four
threading models — see [Little's law, derived](BREAK.md#littles-law-derived-rather-than-recited) —
and read the other way it is capacity estimation entire:

> serving 10,000 rps at 25ms needs **250 requests in flight** — 250 threads, or 250 parked timers, or
> 250 of whatever your model makes concurrency out of.

The row where it appears to fail is the most useful one. A pool of 32 workers, offered 500
connections, reports `λ × W` = **35.9**. The law is not broken: it is answering how many requests were
*in the system*, and the answer is thirty-six. The other 464 were never being served. So **latency
percentiles cannot see starvation, but latency times throughput can** — a detector built from two
numbers every monitoring system already collects.

---

## Running it

```sh
./build/dariyaraah --port 0          # kernel-chosen port, printed on line one
curl -D- http://127.0.0.1:<port>/    # 200 OK, 20ms later
```

**Two delays live in this server, and they are different things.** The 20ms
database call is a `sleep_for` inside the handler, and it is the subject of this
unit. Anything set through `dariyanaap::fault` is *extra*, applied just before
the response goes out, and exists so unit 2 can make one backend of three ten
times slower than its siblings:

```sh
DARIYANAAP_FAULT_LATENCY_MS=234 ./build/dariyaraah --port 0 --fault-port 7788
# 261ms per request instead of 21ms

printf 'latency 0 0\n' | nc 127.0.0.1 7788    # healed, mid-run, no restart
printf 'hang 1\n'      | nc 127.0.0.1 7788    # stops answering entirely
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

### M3 — Bounded pool ✅
- [x] `BlockingQueue` — bounded, two condition variables, closes by draining first
- [x] queue tests assert a **sum**, not a count: a count passes while two workers serve one connection
- [x] `--workers N` on one binary, 0 keeping M1's model, so both models serve identical work
- [x] **the brief's flatline, found**: 1,365 rps from 32 connections on, and 500 buys nothing over 64
- [x] Little's law to three significant figures — 32 workers ÷ 1,365 rps = 23.4 ms, which *is* E1's
      measured `sleep_for(20ms)`
- [x] **p99 improves as the service collapses** — 1,002 ms → 161 ms while refused connections go
      95 → 448 → 968
- [x] green under plain, ASan/UBSan and TSan with pool, queue, accept loop and rig all live

### M6 — Fault knobs ✅
> Added after the unit was closed, by unit 2 (`dariyabaant`), which needs one of
> three identical backends to be ten times slower than the others. Decision 1
> deferred this here on purpose rather than building it at M1.

- [x] `dariyanaap::fault` linked, and every threading model funnels through one
      `send_response` — a knob affecting only one model would make unit 2 a
      comparison of two different programs
- [x] `--fault-port` serves the control socket, so a backend can be made to hang
      **mid-run**, which is unit 2's actual experiment
- [x] a dropped response closes the connection: with keep-alive, silently
      withholding one leaves the client's next request answered by the reply
      after it
- [x] known limit stated in the header — injected latency blocks the event loop
      exactly as E4a's handler did, which is why unit 2 runs its backends
      threaded

### M5 — Write-up ✅
- [x] Little's law checked against all four models: **within 4% on seven rows**, and the eighth row's
      deviation *is* the starvation signal
- [x] both of DESIGN.md's open questions settled by what happened, not by argument
- [x] the track's verifier answered, including the part where the question was wrong

### M4 — Event loop ✅
- [x] `EventLoop` — a thin kqueue, level-triggered, with timers
- [x] **E4a: the blocking loop**, built naive first — 42 rps flat, 21× worse than either threaded
      model, because one thread that sleeps is a server doing one thing at a time
- [x] `handle()` split into `route()` + `needs_database()`, so all four models do **identical work**
      and differ only in who waits
- [x] **E4b: the wait becomes a kqueue timer** — 19,207 rps on one thread against
      thread-per-connection's 19,525 on five hundred, p50 within 0.13 ms, zero errors
- [x] the empty-buffer bug that closed every connection while four suites stayed green, and the test
      that would have caught it
- [x] green under plain, ASan/UBSan and TSan

### The four models, 500 connections, same handler, same machine

| model | threads | req/s | p50 | p99 | errors |
|---|---|---|---|---|---|
| thread per connection | 500 | 19,525 | 25.69 ms | 29.10 | 0 |
| **event loop + timer** | **1** | **19,207** | 25.82 | 30.54 | 0 |
| pool of 32 | 32 | 1,441 | 24.90 | 161.48 | **968 refused** |
| event loop, blocking | 1 | 61* | 220.20 | 1,002.44 | **69 timeout** |

\* at 32 connections — it cannot reach 500 inside the rig's read timeout, and ~23 of that 61 is
timeouts counted as throughput.

**The work was identical in all four. Every difference above is who waits.**

### What this unit was wrong about

Five experiments, four predicted, and the pattern in the errors turned out to be worth more than the
comparison. **Three of the four wrong predictions were wrong about the instrument, not the system:**
`sleep_for(20ms)` is really 23.4ms (E1); a timed-out request enters the histogram at its true elapsed
time, so p99 spikes to the read timeout (E3) and throughput counts failures as work (E4a). The
system under test behaved as predicted almost every time. What kept being mis-modelled was the thing
doing the measuring.

And once, in E1, a **1-in-7 outlier was about to become the finding** — p99 of 63.18ms at 500
connections, which reads exactly like the tail detaching. Five repeats gave 29ms. The effect being
looked for was the same size as the run-to-run noise.

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
| Non-blocking writes | the loop is level-triggered and uses kqueue for readiness only. One slow client reading a byte at a time would stall it inside `write_all`; a production server needs an outbound queue per connection. Stated in DESIGN.md rather than discovered by a reader |
| Anything past 3,000 connections | the rig collapses at 4,000 before the server does. Measuring further would mean measuring the machine |
