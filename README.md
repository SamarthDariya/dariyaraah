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
reconciles them, and BREAK.md's E1 and E2 test it.

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

**M0 complete. Nothing serves traffic yet.**

| Milestone | What lands | Status |
|---|---|---|
| M0 — Skeleton | CMake, doctest, sanitizers, the rig vendored, `check.sh` | ✅ |
| M1 — The naive server | accept loop, thread per connection, minimal HTTP/1.1, `GET /` → 20ms → 200 | |
| M2 — The ramp | predictions committed first, then E1 and E2 | |
| M3 — Bounded pool | a fixed pool behind a request queue, same ramps, E3 | |
| M4 — Event loop | one thread, non-blocking sockets, `kqueue`, E4 | |
| M5 — Write-up | Little's law derived, DESIGN/BREAK/README finished | |

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
