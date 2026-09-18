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

**Samarth's prediction:** *(written before the run — throughput and p99 at 1, 50 and 500)*

**Claude's prediction:** *(committed after Samarth's, before M2's code)*

- **Measured:**
- **Wrong about:**

## E2 — Thread-per-connection, open-loop (M2)

Same server. Connections held at the ramp's knee; sweep `--rate` through `connections ÷ 0.02s`. The
mode where the queue is outside the service.

Watch for `connection_wait` in the rig's output: past the knee it is the pool being too small, not
the rig falling behind — which is unit 0's E5 and the reason these runs are legible at all.

**Samarth's prediction:**

**Claude's prediction:**

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
