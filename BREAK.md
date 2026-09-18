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

The same two ramps against a fixed pool with a request queue in front. The first design in this
series with genuinely shared mutable state.

**Samarth's prediction:**

**Claude's prediction:**

- **Measured:**
- **Wrong about:**

## E4 — Event loop (M4)

The same two ramps again, one thread, non-blocking sockets, `kqueue`. The 20ms sleep cannot stay a
sleep here, and what it becomes is the interesting part.

**Samarth's prediction:**

**Claude's prediction:**

- **Measured:**
- **Wrong about:**

---

## Carried forward

Numbers this unit establishes that later units quote rather than re-derive.

*(Filled in as they are measured.)*
