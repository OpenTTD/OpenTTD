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

## 2026-11-18T14:50 local, session cd8d20a8-9d79-4332-b63e-b2592beb2cbd

Question: "The real-world scenario slice (session openttd-07) uses Open-Meteo for elevation (Copernicus
GLO-90 behind a non-commercial API, sampled at about 642 m) instead of the source policy's Copernicus
GLO-30 from AWS (30 m, no account, open licence). Which should it use?"

Answer: "Copernicus GLO-30 (Recommended)": switch to GLO-30 range-read from the AWS open-data bucket;
finer terrain, no non-commercial term.

Question: "Two Python server.py panels from /Users/korczis/dev/openTTD have run for 2 days on ports
59186 and 59188 (PIDs 63228, 63216). They answer 502 and keep I0084 from passing its hub check. I did
not start them. May I stop them?"

Answer: "Stop them (Recommended)": send them SIGTERM and remove their stale registry records, then rerun
the hub check for I0084.

Done by the lead at about 14:55 local: both panels stopped; no registry record referenced them.

## 2026-11-20T10:05 local, session cd8d20a8-9d79-4332-b63e-b2592beb2cbd

Question: "The agent's next commit and push need a local Elixir compile in their pre-commit and pre-push
hooks. The machine is out of swap (≈41 GB used), largely because the ttd-battle run holds 32 OpenTTD
processes. What should happen?"

Answer: "Stop ttd-battle, compile": the lead stops the ttd-battle OpenTTD processes, waits for memory to
free, then the Prismatic agent commits and pushes; matches in progress end.

Done by the lead at about 10:06 local: `pkill -f 'ttd-battle/openttd-src/build/openttd'`; 0 processes left
and none respawned within 2 minutes; load fell from about 48 to 25. No session on the mesh had claimed the
run (openttd-07, -94, -28, -cb and this session denied it on 2026-11-19).

The same answer set also covered Prismatic !204: "Yes, part of the ratchet (Recommended)" for the
quarantine of non-compiling test files (#2226) and "Keep 30 min (Recommended)" for the `docs:verify`
timeout; those are recorded in prismatic-platform's rule `land-on-green-mr-pipeline`.
