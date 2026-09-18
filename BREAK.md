# BREAK.md — dariyaraah

Track rule 4: three lines per experiment — what I **predicted**, what I **measured**, and what I got
**wrong**. This file is the learning artifact; the code is just how it gets produced.

Rule: **the prediction is written before the run.** A prediction filled in afterwards is worth
nothing, and the prediction error shrinking across twelve repos is the whole skill being trained.

Unit 0 recorded that three of its five experiments were never predicted at all — the discipline
turned out harder to keep than the code was to write. So this repo changes the order slightly:

> **Samarth's prediction is committed first, then Claude's, then the code that measures either.**

Unit 0 wrote Claude's prediction into E3 before the run and left Samarth's slot open below it. That
is the right sequence for rule 4 and the wrong one for the person doing the learning, because a
number already on the page is an anchor whether or not you mean to read it.

Unit 1 has one trade-off — **the threading model** — so it is one ramp, run three times, plus the
question that unit 0 left open.

---

## The standing question

`builds/README.md` says of this unit:

> p50 stays ~20ms while p99 goes to hundreds of ms.

Unit 0's E2 measured the opposite, about a thread-per-connection program, and named this repo while
doing it:

> The rig's p99 does not "detach" from its p50 at all… past saturation both rise *together*, because
> the latency is not a tail effect — it is queueing, and queueing delays every request equally.

Both cannot be right about the same experiment. DESIGN.md decision 3 states the hypothesis: the
detachment is **open-loop only**, because closed-loop is self-limiting while open-loop puts the queue
outside the service, where the wait lands on some requests and not others. E1 and E2 test it.

---

## E1 — Thread-per-connection, closed-loop (M2)

Ramp `--connections` 1 → 2 → 4 → 8 → 16 → 32 → 64 → 128 → 256 → 500 against `GET /` with a 20ms
`sleep_for` in the handler. Server thread count equals connection count (decision 2), so this ramps
the server's concurrency from the client side.

**Samarth's prediction:** *not recorded.* Rule 4 says a prediction filled in afterwards is worth
nothing, so the slot stays empty rather than being back-filled. That is now the fourth time across
two repos, and the honest reading is that the discipline is the hard part, not the code.

**Claude's prediction**, committed before the sweep script existed:

| connections | throughput | p50 | p99 |
|---|---|---|---|
| 1 | ~48 rps | 20.3 ms | ~21 ms |
| 50 | ~2,400 rps | 20.5 ms | ~23 ms |
| 500 | ~23,000 rps | ~21 ms | ~35 ms |

And three specific claims, so that being wrong is cheap to detect:

1. **Throughput is linear in connections across the whole ramp, and does not flatline.** The brief
   says it flatlines at `threads / 0.02s` — but under *closed-loop* against a thread-per-connection
   server, threads *are* connections, so that ceiling rises with every connection added. The flatline
   the brief describes needs the thread count held fixed, which is M3, or the load decoupled from the
   replies, which is E2.
2. **p99 does not detach from p50.** Both stay within a factor of two of each other at every step.
   This sides with unit 0's E2 against the brief, and for the reason E2 gave: past saturation the
   delay is queueing, and queueing delays every request equally.
3. **The naive server does not collapse at 500 connections.** A 20ms sleep means each thread wakes
   ~49 times a second, so 500 threads is ~24,000 wakeups a second — far under what the scheduler can
   do. The brief's "collapses on context switching well before the CPU is busy" assumes a handler
   that computes; a handler that sleeps is the cheapest possible thread to have around. If collapse
   appears at all it is thousands of connections away, and `kern.ipc.somaxconn` = 128 will produce
   connect errors long before the scheduler does.

- **Measured:** `./scripts/e1-ramp.sh`, HTTP `GET /`, 3s after 500ms warm-up per step, one server
  for the whole sweep.

  | conns | req/s | p50 | p99 | p99/p50 |
  |---|---|---|---|---|
  | 1 | 41 | 24.77 ms | 25.69 ms | 1.04 |
  | 2 | 83 | 24.51 | 25.69 | 1.05 |
  | 4 | 166 | 24.38 | 25.69 | 1.05 |
  | 8 | 328 | 24.90 | 25.69 | 1.03 |
  | 16 | 655 | 24.77 | 25.82 | 1.04 |
  | 32 | 1,291 | 24.90 | 26.21 | 1.05 |
  | 64 | 2,595 | 24.90 | 26.87 | 1.08 |
  | 128 | 5,096 | 25.04 | 26.48* | 1.06 |
  | 256 | 10,164 | 25.30 | 28.71 | 1.13 |
  | 500 | 19,525 | 25.69 | 29.10 | 1.13 |
  | 1000 | 38,794 | 25.82 | 30.28 | 1.17 |
  | 2000 | 74,115 | 26.87 | 38.54 | 1.43 |
  | 3000 | 74,872 | 38.80 | 57.41 | 1.48 |
  | ~~4000~~ | ~~60,994~~ | ~~56.62~~ | ~~115.87~~ | **void — see below** |

  Zero errors of any kind at every valid step, and every step started every connection it asked for,
  including 3,000 — `kern.ipc.somaxconn` is 128 here and never bit.

  **Throughput is linear to 2,000 connections, then flat at ~74,500 rps.** Little's law holds
  throughout: at 3,000 connections, 3000 ÷ 74,872 = 40.1 ms against a measured p50 of 38.8 ms.

  **The 4,000 row is void, and catching that is the most useful thing this experiment did.** It looks
  like the thread-per-connection collapse the brief promises — throughput *falls* to 60,994, p50
  doubles, p999 hits 199 ms. Then the rig was run against `dariyanaap-null` at the same concurrencies:

  | conns | rig's own ceiling | dariyaraah measured |
  |---|---|---|
  | 1000 | 107,478 | 38,794 |
  | 2000 | 103,148 | 74,115 |
  | 3000 | 97,389 | 74,872 |
  | 4000 | **57,784** | 60,994 |

  The rig collapses at 4,000 on its own, against a target that does nothing. dariyaraah measured
  *above* the rig's RawEcho ceiling there, which is not a thing that can happen — so that row
  describes the machine running out of threads, not the server. At 2,000 and 3,000 the rig could
  still do 97–103k while the server sat at 74.5k, so the flatline is the server's and is real.

  \* the 128 row is the second run's. The first gave p99 36.44 ms, and p99 63.18 ms at 500 — see
  below.

- **Wrong about:** the arithmetic was right and two of the constants in it were wrong.

  **`sleep_for(20ms)` is 23.4 ms on this machine.** Measured directly, 100 sleeps × 3 trials:
  mean 23.35 / 23.51 / 23.47 ms, worst 25.25 ms. A 17% overshoot, and it accounts for almost all of
  the gap between the predicted 20.3 ms p50 and the measured 24.8 ms. The prediction said 48 rps per
  connection from a 20.5 ms service time; the real service time is 24.4 ms and the real figure is 41.
  **The model was right and its input was wrong**, which is a better way to be wrong than the reverse
  and is only visible because the sleep was measured separately afterwards.

  So of the 24.8 ms a request takes at one connection, **23.4 ms is the sleep asking for 20** and
  roughly 1.4 ms is everything else — two syscalls each way, parsing, the thread wake, loopback TCP.
  That is the unit's title question answered on the first row of the table: where the millisecond
  goes is *the timer*, not the server.

  **Prediction 1 — no flatline — was right for 1 → 2,000 and wrong after.** It flatlines at 74,500
  rps, not because of the arithmetic the brief cites (`threads / 0.02s` at 3,000 connections would be
  150,000 rps) but because the machine stops delivering wakeups faster than that. The brief and the
  prediction were arguing about the wrong ceiling; there are two, and the scheduler's is lower.

  **Prediction 2 — p99 does not detach from p50 — was right, and nearly reported wrong.** The first
  sweep gave p99 = 63.18 ms at 500 connections, a ratio of 2.47, which reads as exactly the
  detachment the brief predicts. Five repeats at 500 gave p99 = 28.97, 30.15, 28.97, 29.23, 29.36 ms.
  **The first number was a 1-in-7 outlier and was about to become the finding.** The real ratio
  climbs gently from 1.04 to 1.48 across three orders of magnitude of concurrency.

  That is the lesson of E1, and it is a methodological one rather than a systems one: **the effect
  being looked for was the same size as the run-to-run noise, so a single sweep could not have
  answered the question in either direction.** Unit 0 measures its own ceiling to avoid attributing
  the instrument to the target; this is the same mistake one level up — attributing variance to the
  system.

  **Prediction 3 — no collapse at 500 — was right**, and stronger than claimed: no collapse at 3,000
  either, and the apparent one at 4,000 belongs to the rig.

## E2 — Thread-per-connection, open-loop (M2)

Same server. Connections held at the ramp's knee; sweep `--rate` through `connections ÷ 0.02s`. The
mode where the queue is outside the service.

Watch for `connection_wait` in the rig's output: past the knee it is the pool being too small, not
the rig falling behind — which is unit 0's E5 and the reason these runs are legible at all.

**Samarth's prediction:** *not recorded,* as above.

**Claude's prediction**, same commit, 32 connections — a knee at `32 / 0.0205s` ≈ 1,560 rps:

| offered | achieved | p50 | p99 | `connection_wait` p50 |
|---|---|---|---|---|
| 800 rps | ~800 | 20.5 ms | ~22 ms | ~0 |
| 1,500 rps | ~1,500 | ~21 ms | ~30 ms | ~1 ms |
| 3,200 rps | ~1,560 | **~1,200 ms** | ~2,400 ms | ~1,200 ms |

The claim worth being wrong about: **neither mode produces the shape the brief describes.** "p50
stays ~20ms while p99 goes to hundreds" requires a queue that forms and drains — a *transient*. Under
a sustained 2× overload the queue never drains, so latency-from-due grows roughly linearly across the
measured window, which puts p50 near half the maximum and p99 near all of it. Both percentiles leave
20ms together, and the ratio settles near 2.

If that is right, the brief is describing a stall rather than a ramp — which is unit 0's E3, not this
unit's E1 — and the reconciliation is that queueing is uniform whoever is doing the queueing. If it
is wrong, the interesting question is what makes the tail selective, and that is a better finding
than the prediction would have been.

- **Measured:** `./scripts/e2-rate.sh`, 32 connections held fixed, 3s after 500ms warm-up.

  | offered | achieved | p50 | p99 | p99/p50 | `connection_wait` p50 | `rig_lag` p50 |
  |---|---|---|---|---|---|---|
  | 400 | 400 | 34.87 ms | 40.37 ms | 1.16 | 0.000 ms | **7.373 ms** |
  | 800 | 800 | 30.93 | 36.18 | 1.17 | 0.000 | **2.703** |
  | 1,200 | 1,158 | 83.89 | 135.27 | 1.61 | 56.62 | 0.001 |
  | 1,300 | 1,153 | 191.89 | 354.42 | 1.85 | 163.58 | 0.001 |
  | 2,600 | 1,199 | 822.08 | 1,627.39 | **1.98** | 792.72 | 0.001 |

  Zero errors throughout. The knee is at ~1,175 rps, implying a service time of 27.2 ms — three
  milliseconds worse than closed-loop's 24.4 ms, which is what contention for 32 connections costs.

- **Wrong about:** the headline claim was right and the reasoning has a hole in it.

  **"Neither mode produces the shape the brief describes" — confirmed.** Under sustained overload p50
  does not stay at 20 ms while p99 runs to hundreds. Both leave together: at 2,600 rps offered, p50 is
  822 ms and p99 is 1,627 ms. The predicted ratio of 2 was measured at **1.98**, and the approach to
  it is visible across the sweep: 1.16, 1.17, 1.61, 1.85, 1.98.

  So the brief describes a **transient** — a queue that forms and drains, which is unit 0's E3 stall,
  not this unit's ramp. Under a queue that never drains, latency-from-due grows linearly across the
  window, which puts p50 near half the maximum and p99 near all of it. Queueing delays every request,
  and unit 0's E2 was right about that in both load modes, not just the self-limiting one.

  **What the prediction missed entirely: the rig's own scheduling lag at LOW rates.** At 400 rps
  offered, `rig_lag` p50 is **7.373 ms** — larger than the 2.5 ms schedule interval — and it falls to
  2.703 ms at 800 rps and to a microsecond past the knee. It is the same phenomenon as the 20 ms
  sleep taking 23.4 ms: macOS coalesces timers, and the longer a thread intends to sleep the more it
  overshoots. Past the knee no worker ever sleeps, so it vanishes.

  Consequence, and it is the reason M6 was worth reopening unit 0 for: **open-loop reports a p50 about
  10 ms worse than closed-loop at low rates against the same server, and 7.4 ms of that is the rig.**
  Without the `rig_lag` / `connection_wait` split, the honest reading of these first two rows would
  have been "the server is slower under open-loop", which is false. The split names the 7.373 ms as
  the instrument's, and it is the only reason these rows are interpretable at all.

## E3 — Bounded pool and queue (M3)

The same closed-loop ramp against a pool of **32 workers held fixed**, connections 1 → 500. Open-loop
adds nothing while connections are under the worker count — it is E2 with extra steps — so the
interesting sweep is the one where connections exceed workers.

**Samarth's prediction:** *not recorded,* as before.

**Claude's prediction**, committed before `e3-pool.sh` existed:

| conns | req/s | p50 | p99 | errors |
|---|---|---|---|---|
| 1 | 41 | 24.8 ms | 25.7 ms | 0 |
| 16 | 655 | 24.9 | 25.8 | 0 |
| 32 | 1,310 | 24.9 | 26.2 | 0 |
| 64 | **1,310** | ~25 | ~26 | timeouts |
| 128 | **1,310** | ~25 | ~26 | timeouts + connects refused |
| 500 | **1,310** | ~25 | ~26 | many |

**The flatline finally appears, at `workers / 0.0244s` ≈ 1,310 rps**, and no number of extra
connections moves it. That is the brief's sentence, arriving one milestone later than the brief puts
it, and for the reason the brief gives: threads have stopped tracking connections.

The claim worth being wrong about is the latency column. **I expect p50 and p99 to stay flat at
~25/26 ms all the way to 500 connections — better than M1's 25.7/29.1 ms at the same step — while the
service gets dramatically worse.** A worker holds a keep-alive connection for its whole life, so past
32 connections the *served* ones are served exactly as fast as before, and the starved ones produce
timeouts and refused connects rather than slow requests. They never reach the histogram at all.

If that is right, E3's lesson is not about pools. It is that **latency percentiles cannot see
starvation**: the tail improves while most clients get nothing, and every number in the latency table
is a true statement about a shrinking subset of the traffic. The error counters are the only place
the failure appears, which is unit 0's decision 6 — errors counted by kind, never folded into one
rate — earning its keep three repos later.

- **Measured:** `./scripts/e3-pool.sh`, 32 workers held fixed, 3s after 500ms warm-up per step.

  | conns | req/s | p50 | p90 | p99 | errors |
  |---|---|---|---|---|---|
  | 1 | 42 | 24.64 ms | 25.30 | 25.56 | — |
  | 8 | 336 | 24.25 | 25.30 | 25.56 | — |
  | 16 | 656 | 24.64 | 25.56 | 25.95 | — |
  | 32 | **1,333** | 24.38 | 25.43 | 25.82 | — |
  | 64 | **1,365** | 24.51 | 25.43 | **1,002.44** | 64 timeout |
  | 128 | **1,367** | 24.77 | 26.21 | **1,002.44** | 95 connect · 97 timeout |
  | 256 | **1,365** | 24.90 | 26.35 | 133.69 | **448 connect** |
  | 500 | **1,441** | 24.90 | 35.65 | 161.48 | **968 connect** |

  **The flatline, exactly where predicted.** 1,365 rps from 32 connections onward, and 500
  connections buys nothing over 64. Little's law again, and this time almost embarrassingly clean:
  32 workers / 1,365 rps = 23.4 ms, which is E1's measured `sleep_for(20ms)` figure to three
  significant figures. The pool's service time is *pure timer*.

- **Wrong about:** the throughput column, the p50 column and the conclusion held. The p99 column was
  wrong, and the way it was wrong is better than the prediction.

  **p50 stays flat at ~24.9 ms to 500 connections — better than M1's 25.69 ms at the same step, while
  serving 1,441 rps instead of 19,525.** Predicted, and it held.

  **p99 does not stay flat: it hits 1,002.44 ms at 64 and 128 connections.** The prediction assumed
  starved connections never reach the histogram. They do — as timeouts. Unit 0's `exchange.cpp`
  records a timed-out request at its *actual* elapsed time rather than dropping it, deliberately, "so
  it never under-reports", and 1,002.44 ms is the 1,000 ms read timeout showing through. The claim
  was wrong because it forgot a design decision inside the instrument, which is the same class of
  mistake as E1's 20 ms constant.

  **And then p99 gets BETTER as the service gets worse.** At 256 and 500 connections it falls to
  133.69 and 161.48 ms while refused connections climb 95 → 448 → 968. The failure mode changed: a
  connection *accepted and starved* eventually times out, and a timeout is recorded; a connection
  *refused* has no duration at all, is counted by kind, and never enters the histogram. **The more
  severe failure is the one the latency table cannot see.**

  So the conclusion is stronger than the prediction, not weaker. Latency percentiles cannot see
  starvation, and they *improve* as it deepens. A dashboard showing p50 24.9 ms with p99 falling from
  1,002 ms to 161 ms reads as a service recovering. It was refusing two thirds of its clients. The
  only honest signal is the error counters split by kind — unit 0's decision 6, "errors are counted
  by kind, never as one rate", earning its keep three repos later for a reason nobody had in mind
  when it was written.

## E4a — Event loop, handler still blocking (M4)

One thread, `kqueue`, and `handle()` unchanged — so the single thread sleeps for 20ms in the middle
of everything. Built naive first, per track rule 1, because "never block the event loop" is advice
everyone repeats and almost nobody has measured.

**Samarth's prediction:** *not recorded,* as before.

**Claude's prediction:**

| conns | req/s | p50 | p99 |
|---|---|---|---|
| 1 | ~42 | 24.8 ms | 25.7 ms |
| 32 | **~42** | ~750 ms | ~780 ms |
| 500 | **~42** | ~11,700 ms | timeouts |

Throughput is `1 / service_time` at every step — one thread, one request at a time, and connections
buy nothing. **This should be the worst model in the unit by three orders of magnitude at the top of
the ramp**: 42 rps against M1's 19,525 and M3's 1,365.

Latency is where it gets ugly. Little's law runs in reverse: with N connections all waiting on one
server, p50 ≈ N × 23.4 ms. At 32 connections that is 750 ms; at 500 it exceeds the rig's 1,000 ms read
timeout, so most requests should fail rather than return, and throughput should *fall* as the run
spends itself on connections that time out and reconnect.

- **Measured:** `./scripts/e4-loop.sh`, 3s after 500ms warm-up per step.

  | conns | req/s | p50 | p90 | p99 | errors |
  |---|---|---|---|---|---|
  | 1 | 42 | 24.38 ms | 25.43 | 25.56 | — |
  | 2 | 42 | 47.71 | 50.07 | 50.33 | — |
  | 4 | 42 | 72.88 | 143.66 | 146.80 | — |
  | 8 | 42 | 167.77 | 329.25 | 339.74 | — |
  | 16 | **45** | 214.96 | 1,002.44 | 1,002.44 | 17 timeout |
  | 32 | **61** | 220.20 | 1,002.44 | 1,002.44 | 69 timeout |

  **42 rps, flat, from one connection to eight.** Exactly `1 / service_time`, and connections buy
  literally nothing — the 1-connection and 8-connection rows serve the same 127 requests in three
  seconds. Against M1 at 32 connections (1,291 rps) and M3 (1,333 rps), this is **21× worse**, on the
  same machine, serving the same handler.

  p50 climbs 24 → 48 → 73 → 168 ms as clients queue behind the one thread. Little's law running
  backwards: latency is concurrency ÷ throughput, throughput is fixed at 42, so every connection added
  is 24 ms on everyone's latency.

- **Wrong about:** the direction of the throughput number past the timeout threshold.

  The prediction said throughput should **fall** at high concurrency as the run spent itself on
  connections that timed out and reconnected. It **rose** — 42 → 45 → 61 — and the reason is the
  third time in this repo that a prediction has been bent by a design decision inside the instrument.
  `exchange.cpp` records a timed-out request in the histogram at its true elapsed time, so a timeout
  is a sample, and `per_second()` counts samples. At 32 connections, 69 of the 183 attempts were
  timeouts: the honest successful rate is ~38 rps, and the reported 61 is a third failure by volume.

  So **the throughput column counts failures as work**, which is E3's finding arriving from the
  opposite direction. There, the latency table improved as the service collapsed because refused
  connections have no duration. Here, the throughput table improves as the service collapses because
  timed-out connections do. Both are true statements about a distribution that stopped describing the
  thing anyone cared about, and in both cases the only honest reading came from the per-kind error
  counters sitting next to it.

  The p50 figure at 32 connections was predicted at ~750 ms and measured at 220 ms, for the same
  reason: everything slower than 1,000 ms left the distribution as a timeout, so the surviving median
  describes the requests lucky enough to be served.

## E4b — Event loop, the database call as a timer (M4)

The fix, and the one change that makes an event loop an event loop: the 20ms stops being a sleep and
becomes a `kqueue` timer, with the connection parked until it fires. `handle()` splits into `route()`
plus a declared delay, so all four models still do identical work and only *who waits* differs —
which is what a threading model is.

**Samarth's prediction:** *not recorded,* as before.

**Claude's prediction:** *numerically not recorded* — and that is a lapse worth writing down rather
than hiding, because it is the same one unit 0 made three times. The design was committed before the
code (the split into `route()` plus a declared delay, in the commit that created these slots), and the
claim "this should produce the best numbers in the unit" was made in conversation. But **no table was
written before the run**, so there is nothing here to be wrong against. Four experiments predicted
properly, one not, and the one not predicted is the one whose result was most expected — which is
exactly how the discipline slips.

- **Measured:** `MODE=2 ./scripts/e4-loop.sh`, 3s after 500ms warm-up per step, beside M1's numbers
  from E1 at the same steps.

  | conns | **E4b** req/s | E4b p50 | E4b p99 | M1 req/s | M1 p50 | M1 p99 |
  |---|---|---|---|---|---|---|
  | 1 | 42 | 24.38 ms | 25.82 | 41 | 24.77 | 25.69 |
  | 8 | 327 | 24.64 | 25.82 | 328 | 24.90 | 25.69 |
  | 32 | 1,289 | 24.90 | 26.48 | 1,291 | 24.90 | 26.21 |
  | 128 | 5,117 | 25.17 | 27.79 | 5,096 | 25.04 | 26.48 |
  | 256 | 10,133 | 25.30 | 30.15 | 10,164 | 25.30 | 28.71 |
  | 500 | **19,207** | 25.82 | 30.54 | 19,525 | 25.69 | 29.10 |

  Zero errors at every step. **One thread reaches 98.4% of what five hundred threads reach**, with p50
  within 0.13 ms and p99 within 1.4 ms of them.

  Against E4a at 32 connections — the same single thread, the same handler, the same 20 ms — it is
  **21× faster** (1,289 rps against 61). Nothing changed except who does the waiting.

- **Wrong about:** nothing measurable, having failed to write a number down. Two things are worth
  recording anyway.

  **The result is more complete than expected.** "Comparable to thread-per-connection" would have been
  a good outcome; matching it to within 2% across three orders of magnitude, with 500 timers armed
  concurrently and no errors, is a stronger claim than seemed safe to make. The event loop is not a
  trade — at this workload it is strictly better, because it buys the same throughput without 500
  thread stacks.

  **The 500-thread model is not embarrassed by this, and that is the honest reading.** M1 held its
  own to 3,000 connections (E1) and the machine, not the design, is what eventually gave way. The
  case for the event loop here is not speed. It is that it reaches the same speed at a resource cost
  that keeps going: E1 needed 3,000 OS threads to get its ceiling, and this needs one.

---

## Carried forward

Numbers this unit establishes that later units quote rather than re-derive.

| Number | Value | From |
|---|---|---|
| `sleep_for(20ms)` on this machine | **23.4 ms** — a 17% overshoot. Any "inject N ms" is really N + 3.4 | E1 |
| dariyaraah's own ceiling | **~74,500 rps**, reached at 2,000 connections, flat to 3,000 | E1 |
| Thread-per-connection is fine until | **at least 3,000 connections** with a sleeping handler. The apparent collapse at 4,000 is the rig's | E1 |
| The rig's ceiling past 1,000 conns | 107k @1000 · 103k @2000 · 97k @3000 · **57.8k @4000**. E2's table stops at 1,000; this extends it | E1 |
| Run-to-run p99 noise at 500 conns | 29–63 ms, i.e. **up to 2×**. One sweep cannot resolve a tail effect smaller than that | E1 |
| Open-loop `rig_lag` at low rates | **7.4 ms at 400 rps**, 2.7 ms at 800, ~0 past the knee. Subtract it before comparing modes | E2 |
| p99/p50 under sustained overload | converges on **2**, both percentiles rising together | E2 |
| A bounded pool's ceiling | **`workers / service_time`** exactly — 32 workers, 23.4 ms, 1,365 rps | E3 |
| What a latency table hides | **starvation, entirely.** A refused connection has no duration, so p99 *improves* as refusals rise. Only per-kind error counts show it | E3 |
| What a throughput table hides | **timeouts, which it counts as work.** Reported 61 rps where 38 were real. The mirror image of the row above | E4a |
| An event loop with a blocking call | `1 / service_time`, whatever the concurrency. **21× worse** than either threaded model | E4a |
| An event loop with the wait as an event | **98.4% of thread-per-connection's throughput on one thread** — 19,207 vs 19,525 rps, p50 within 0.13 ms | E4b |

**The four models, at 500 connections, same handler, same machine:**

| model | threads | req/s | p50 | p99 | errors |
|---|---|---|---|---|---|
| thread per connection | 500 | 19,525 | 25.69 ms | 29.10 | 0 |
| **event loop + timer** | **1** | **19,207** | 25.82 | 30.54 | 0 |
| pool of 32 | 32 | 1,441 | 24.90 | 161.48 | **968 refused** |
| event loop, blocking | 1 | 61* | 220.20 | 1,002.44 | **69 timeout** |

\* at 32 connections; it cannot reach 500 inside the rig's read timeout. Of that 61, ~23 is timeouts
counted as throughput.

**The unit in one line:** the work was identical in all four. Every difference in that table is
*who waits*.

**The standing question, answered:** p99 does **not** detach from p50 — not in closed-loop (1.04 →
1.48 across 1 → 3,000 connections) and not in open-loop, where the ratio converges on 2 with *both*
percentiles leaving 20 ms together. Unit 0's E2 was right and the track brief describes a transient:
a queue that forms and drains, which is unit 0's E3 stall, not a ramp. Queueing delays every request
equally, and that holds in both load modes rather than only the self-limiting one.
