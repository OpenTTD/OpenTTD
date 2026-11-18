# Owner answers: the event bus and the store

Answers the owner gave about the event bus on ZeroMQ and the DuckDB store (ADR 0033, I0080, ADR 0034,
I0081-I0087), recorded verbatim with the session and the time.

## 2026-11-18T00:45Z, session cd8d20a8-9d79-4332-b63e-b2592beb2cbd

The owner's request (sent four times unchanged): "nastuduj si veskre zdrojove kody, updatni veskre .ai/
at muzeme lepe optimalizovat a dynamicky generovat a napojovat jak na algoglyph, tak prismatic / velice
ciste a decentne se nekam napichni na bus, to posilej na nejake zeromq, at k tomu maji pristup vsichni v
rust / blackboard / agents / taktez to rovnou prubezne per session/instance/game ukladej do duckdb, at se
to da pri kazdem rozhodnuti pouzit / Kazdy event to updatuje / rhai f riendly / ui/ux/app/web exposed /
vse rid pomoci .ai/ / majordomus"

Question: "Your bus/ZeroMQ/DuckDB/.ai message arrived four times unchanged. Everything in it is already
running: ADR 0033 + I0080 in .ai/, a Rust agent writing the ZeroMQ PUB tap and the DuckDB store (628
lines so far), a /data page agent, and a .ai/ update agent (ADR 0034, a milestone for history-driven
planning). What do you want next?" (several answers allowed)

Answer, all four options:

- "Keep going as is (Recommended)"
- "Wire subscribers now": start the Prismatic (Elixir SUB into prismatic_blackboard /
  prismatic_storage_duckdb) and AlgoGlyph subscriber work now against the ADR 0033 contract, in
  parallel, each in its own repository under its own rules.
- "Majordomus blackboard too": also forward game events (or a digest) to the Majordomus mesh blackboard,
  so every agent session sees them there, not only on ZeroMQ.
- "My replies are not visible": keep each reply to a few lines.
