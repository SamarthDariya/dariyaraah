# dariyaraah

> *"Dariya rāh"* — Dariya's path.

Unit **1** of the [`builds/`](../../../hld/builds/README.md) HLD track: one HTTP endpoint, one fake
database call that sleeps 20ms, and three ways of serving it — thread-per-connection, a bounded pool
behind a queue, and an event loop. The point is not the server. The point is the number where each
one stops keeping up, and why.

Measured with [dariyanaap](../dariyanaap), unit 0, vendored here as a submodule.

Sibling projects: [dariyanache](https://github.com/SamarthDariya/DariyanAche) (Redis clone, Go),
[dariyakyu](../dariyakyu) (Kafka-style commit log, C++).

---

## The question

> Where does a millisecond go, and at what concurrency does p99 stop resembling p50?

Everything here exists to answer that with a measurement instead of an opinion. See
[DESIGN.md](DESIGN.md) for why the server is shaped the way it is, and [BREAK.md](BREAK.md) for what
was predicted, what was measured, and what the predictions got wrong.

**Status:** M0 in progress. Nothing serves traffic yet.
