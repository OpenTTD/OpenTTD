# Town model

**RESEARCH-ONLY.** `derive.py` reads the engine sources and writes the town-authority model:
- what raises and lowers a company's rating in a town;
- what the authority then allows or refuses;
- the town actions, and the rules tying a town to a company's stations.

Every rule carries the file and line it came from. The mechanism is described in
[`research/mechanisms/town-authority.md`](../../../research/mechanisms/town-authority.md).

```sh
python3 tools/research/town-model/derive.py          # regenerate every output
python3 tools/research/town-model/derive.py --check  # fail when an output is stale (tools/gate.sh change runs it)
```

Outputs, all generated:

| File | Reader |
|---|---|
| `town_model.json` | the model; the panel's `/api/town/model` |
| `../prismatic-ai/PrismaticAI/town_model.nut` | PrismaticAI (`TOWN_MODEL`, helpers in `town.nut`) |
| `../mcp-panel/rhai/town_model.rhai` | Rhai scripts (`town_lib.rhai`: bands, gates, monthly update, advice) |
| the tables of `research/mechanisms/town-authority.md` | readers of the mechanism page |

Re-run it after an engine change touching town ratings. The JSON's `sources` lists the blob id of
every file a rule came from.
