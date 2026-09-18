# DESIGN.md — dariyaraah

Why anything here is the way it is, with the alternatives that were rejected. Decisions are numbered
and amended in place; an amendment says what measurement forced it.

---

## The thesis

> **Throughput is not a property of the server. It is `concurrency ÷ latency`, and the server only
> gets to choose the concurrency.**

A handler that sleeps 20ms can serve exactly `threads ÷ 0.02s` requests per second and not one more,
whatever the CPU is doing. That is Little's law, and unit 1 exists to derive it by hitting it rather
than by reading it. Three threading models are three answers to "what is the concurrency here", and
the interesting one is the *third* number — the concurrency at which p99 stops resembling p50.

---

## Part I — Conceptual design

### 1. The fake database is a real `sleep_for(20ms)` in the handler

Not `fault::inject_latency`, though the rig is vendored and the knob is right there.

The knob applies latency immediately before the *response* is written. A database call happens in the
middle of a request, and the difference matters the moment a thread pool exists: what is occupied
during those 20ms is the thing being measured. Putting the delay where the work would be keeps the
naive server honestly naive, and costs nothing at M1 because v1 has no configuration to speak of.

`dariyanaap::fault` gets linked at the milestone that needs it, which is unit 2 — one backend made
10× slower, mid-run, without a rebuild. At that point this repo will have **two different delays** in
it, and the README has to say which is which, or a later reader will reasonably assume they are the
same mechanism.

**Rejected:** using `fault::before_response()` for the DB call from the start. It buys a control
socket nothing yet needs, in exchange for putting the delay in the wrong place and making unit 1
depend on the rig on both sides of the wire.

### 2. "Thread-per-request" is really thread-per-connection, and the README will say so

The track's brief says thread-per-request. What gets built is a thread per *connection*, held for the
connection's lifetime. This is forced, not chosen:

`dariyanaap` sends no `Connection:` header, so HTTP/1.1 persistence is the default, and its worker
connects once and reconnects only after a failure — following a deliberate `reconnect_delay`. A
server that closed after each response would make every run measure that delay instead of this
service. So connections are held open.

Two consequences, both good:

- It is a real design, not a cheat. Apache prefork is this.
- **Server thread count is pinned to the client's `--connections`.** The independent variable is set
  from the other end of the wire, so `throughput = threads ÷ 0.02s` becomes directly checkable rather
  than inferred: 500 connections should mean 500 threads and 25,000 rps.

### 3. The ramp runs both load modes, and they answer different questions

Closed-loop ramps **connections**. Open-loop fixes connections and sweeps **rate** through the knee
at `connections ÷ 0.02s`.

Not to re-teach coordinated omission — that was unit 0's lesson and track rule 2 says one trade-off
per repo. The reason is that unit 0 left a contradiction worth settling. `builds/README.md` predicts
for this unit that "p50 stays ~20ms while p99 goes to hundreds of ms". Unit 0's E2 measured the
opposite about a thread-per-connection program, and said so explicitly as a prediction about this
repo:

> The rig's p99 does not "detach" from its p50 at all… past saturation both rise *together*, because
> the latency is not a tail effect — it is queueing, and queueing delays every request equally.

**The hypothesis M2 tests:** the detachment is an open-loop-only phenomenon. Closed-loop is
self-limiting — every connection owns a thread, so queueing is uniform and both percentiles rise
together. Open-loop puts the queue *outside* the service, where the wait lands on some requests and
not others, and that is where a tail comes from. If that is right, the brief and E2 are both correct
about different experiments, and neither alone would have shown it.

---

## Part II — Structure

Four layers, and the seams between them are chosen so that M3 and M4 replace exactly one.

```
apps/dariyaraah.cpp     flags, bind, run
  └── Server            accept loop  ──►  thread per connection   ← M3 and M4 replace THIS
        └── serve_connection          read, frame, parse, hand over, write
              ├── http::  find_header_end · parse_request_line · write_response
              └── handle(Request) -> Response        the 20ms, and no socket in sight
```

**`handle` is the fixed point.** It takes a `Request` and returns a `Response` and cannot reach a
socket, which is what lets three threading models run *the same work*. A handler reachable only
through a connection object would have to be reimplemented per model, and E1–E4 would then be
comparing three different programs while calling the difference a threading result.

**Everything borrows.** `Request`'s fields are `string_view`s into the connection's read buffer;
`Response`'s body is a `string_view`; `write_response` serialises into a caller-owned `string`. Three
places where an allocation per request would otherwise land on the measured path and be reported as
this service's latency. The buffers are owned by `serve_connection` and reused for the life of the
connection — the same rule as dariyanaap's `perform_request`.

**Two bounds guard a connection, and they guard different things.** `kIdleTimeout` (30s, set on
accept) bounds a client that connects and says nothing; `kMaxHeaderBytes` (16 KiB) bounds one that
says too much. Neither is a policy about clients — at M1 a thread *is* the unit of capacity, so an
unbounded connection is a denial of service costing the attacker one socket.

**Framing is checked before reading, not after.** Keep-alive plus one TCP segment carrying two
requests is ordinary, so a server that always read first would block waiting for a request it was
already holding — visible only under load, and looking like the target's fault.

What unit 0 supplies, and this repo therefore never writes: `Listener`, `Socket`, `Endpoint`,
`Flags`, `MonotonicClock`, and the error hierarchy. Unit 1 writes HTTP, a handler, and a threading
model, which is the whole of what it is about.

---

## What unit 0 established, and this repo must not re-derive

From [dariyanaap's BREAK.md](../dariyanaap/BREAK.md):

| | | Why it matters here |
|---|---|---|
| Rig peak **132,834 rps** at 32 connections | E2 | a target measuring near 130k is measuring the rig |
| Rig p99 floor **51.7 µs** | E2 | nothing here can be measured as faster |
| Rig is **saturated** past 32 connections | E2 | its 4.55ms p50 at 500 connections is Little's law *at 107k rps*, not a per-connection tax. Against a 20ms target issuing ~25k rps the rig's own share is sub-100µs — and the write-up must say so, or it reads as a quarter of the signal |
| Open-loop ceiling is **`connections ÷ service_time`** | E5 | there is no pipelining; offering more than that queues, and the queue is reported as `connection_wait` |
| `kern.ipc.somaxconn` is **128**, clamped silently | E2 | this server's `listen()` backlog inherits it. 500 simultaneous connects need a gradual ramp or `sysctl -w kern.ipc.somaxconn=2048`, and the results must say which was done |

---

## Open questions

Recorded rather than resolved, to be settled by the milestone that needs them:

- **Non-blocking sockets for M4.** `dariyanaap::Socket` sets `O_NONBLOCK` internally for its connect
  timeout but does not expose it. The event loop needs it. Reach through `Socket::fd()` from this
  repo, or add a method to unit 0? Leaning: reach through `fd()`, and leave unit 0 alone unless a
  second unit wants the same thing.
- **Whether the pool and the event loop share an HTTP parser.** They should, but only M3 will show
  whether the parser's interface survives not owning its own thread.
