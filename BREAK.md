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

- **Measured:**
- **Wrong about:**

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

- **Measured:**
- **Wrong about:**

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
