# Use cases

A use case is an executable object of this repository, not documentation about it:
one file under `.ai/repo/use-cases/`, the commands, rules, claims, responsibilities and
applications it names, and a scenario the tool runs against itself. `docs/USE_CASES.md`
is the contract; this is what a worker does with it.

## When you change a capability

1. `majordomus usecase impact` names the use cases, scenarios and cases your change
   reaches. Run the scenarios it lists; update a use case whose behaviour genuinely
   changed rather than writing a near-duplicate.
2. A new public command, a new guaranteed claim or a new MCP tool is a coverage gap the
   moment it exists. `majordomus usecase coverage` shows it; `majordomus usecase scaffold
   --for command:<name>` writes a draft with what is already known. Complete the
   narrative (`# Situation`, `# Outcome`), tighten the assertions, set `status: active`.
3. `majordomus usecase validate`, then `majordomus usecase run <id>`. A step that does
   not behave as the scenario says is a failure with the step named, never a page.
4. `scripts/generate-site-data` and commit the regenerated data with the change: the
   generator executes every scenario and embeds the evidence, and CI refuses stale data.
5. `majordomus finish` refuses completion while a required capability has no active use
   case running it (policy `use_cases.coverage`, finish key `use_cases_covered`).

## What never goes into a use case

A command's description or syntax, a rule's text, a claim's wording, captured output, a
status somebody wrote by hand. All of it is derived from the objects the use case names
and from the evidence of its run; the schema refuses a key it does not declare.

## Reading one

`majordomus usecase list`, `majordomus usecase show <id>`; over MCP,
`majordomus://use-case/<id>`; on the site, `/use-cases/<id>/` with the executed output
of the generation that built the page.
