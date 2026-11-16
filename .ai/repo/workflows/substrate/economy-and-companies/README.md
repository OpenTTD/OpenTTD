---
schema: context/v1
id: fork.substrate.economy-and-companies
kind: context
title: "Economy and companies: money, prices, inflation, loans, bankruptcy, mergers and cargodist"
description: "Read before changing company finances, prices, periodic economy timers or the threaded link graph."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/economy*'
  - 'src/company_base.h'
  - 'src/company_cmd.*'
  - 'src/company_func.h'
  - 'src/company_type.h'
  - 'src/misc_cmd.*'
  - 'src/table/pricebase.h'
  - 'src/linkgraph/**'
  - 'src/saveload/economy_sl.cpp'
  - 'src/saveload/company_sl.cpp'
  - 'src/saveload/linkgraph_sl.cpp'
  - 'src/table/settings/linkgraph_settings.ini'
  - 'docs/linkgraph.md'
---

# Economy and companies

## Purpose and ownership

Owns the `Company` pool and its money (`money`, `money_fraction`, `current_loan`, expenses
history), global economic state (`_economy`, `_price`, inflation, recession), cargo payment,
infrastructure maintenance, bankruptcy and takeover, and cargodist: per-cargo `LinkGraph`s whose
`LinkGraphJob`s are solved on worker threads and merged back at a fixed game date.

## Entry points

- `src/company_base.h:Company` / `src/company_base.h:CompanyProperties`, pool `src/company_base.h:CompanyPool`; owners `OWNER_TOWN`, `OWNER_NONE`, `OWNER_WATER`, `OWNER_DEITY` in `src/company_type.h`.
- Money in/out: `src/company_cmd.cpp:SubtractMoneyFromCompany` (called by `src/command.cpp:CommandHelperBase::InternalExecuteProcessResult` after every successful command), `src/company_cmd.cpp:SubtractMoneyFromCompanyFract`, `src/company_cmd.cpp:CheckCompanyHasMoney`.
- Commands: `src/company_cmd.cpp:CmdCompanyCtrl` (new/AI/delete), `src/company_cmd.cpp:CmdGiveMoney`, `src/misc_cmd.cpp:CmdIncreaseLoan`, `src/misc_cmd.cpp:CmdDecreaseLoan`, `src/misc_cmd.cpp:CmdSetCompanyMaxLoan`, `src/misc_cmd.cpp:CmdChangeBankBalance`, `src/economy.cpp:CmdBuyCompany`.
- Prices: `src/economy.cpp:RecomputePrices` from `src/table/pricebase.h:_price_base_specs` into `_price`; `src/economy.cpp:GetTransportedGoodsIncome`; `src/economy.cpp:AddInflation`.
- Payment: `src/economy_base.h:CargoPayment`, `src/economy.cpp:CargoPayment::PayFinalDelivery`, `src/economy.cpp:CargoPayment::PayTransfer`.
- Ownership transfer: `src/economy.cpp:ChangeOwnershipOfCompanyItems` (vehicles, groups, a whole-map `ChangeTileOwner` pass, stations, signs, goals, story pages).
- Link graph: `src/linkgraph/linkgraphschedule.cpp:OnTick_LinkGraph`, `src/linkgraph/linkgraphschedule.cpp:StateGameLoop_LinkGraphPauseControl`, `src/linkgraph/linkgraphjob.cpp:LinkGraphJob::SpawnThread`; edges are fed by `src/station_cmd.cpp:IncreaseStats` (from `src/linkgraph/refresh.cpp:LinkRefresher::Run`) and pruned by `src/station_cmd.cpp:DeleteStaleLinks`.
- **[fork]** `src/prismatic_mcp/prismatic_mcp_server.cpp:ResourceCompanies` reads id, name, AI flag, money (read-only, gated by `OTTD_PRISMATIC_MCP_SERVER`).

## Lifecycle

- **Init**: `src/company_cmd.cpp:InitializeCompanies`, `src/economy.cpp:InitializeEconomy`, `src/economy.cpp:StartupEconomy`; companies at game start via `src/company_cmd.cpp:StartupCompanies` / `DoStartupNewCompany`.
- **Per tick**: `src/company_cmd.cpp:OnTick_Companies` serves one company slot per tick (`_cur_company_tick_index`, saved) for name generation and takeover offers, and schedules AI starts; `src/company_cmd.cpp:UpdateLandscapingLimits` refills limits; `OnTick_LinkGraph` acts only at `date_fract == SPAWN_JOIN_TICK`.
- **Economy month** (`_economy_companies_monthly`, `src/economy.cpp`): `CompaniesGenStatistics` (bankruptcy check, infrastructure maintenance; quarterly history and rating), `CompaniesPayInterest`, `HandleEconomyFluctuations`. **Economy year**: `_economy_companies_yearly` in `src/company_cmd.cpp` rotates `yearly_expenses`.
- **Calendar month**: `_calendar_inflation_monthly` in `src/economy.cpp` runs `AddInflation` + `RecomputePrices`. Inflation follows the calendar; interest, maintenance and bankruptcy follow the economy clock (they differ under wallclock units, see `src/misc.cpp:InitializeGame`).
- **Link graph cycle**: every `recalc_interval` at tick 21 of the day, `LinkGraphSchedule::SpawnNext` copies the next graph (size >= 2) into a job and starts a thread; half an interval later `LinkGraphSchedule::JoinNext` deletes the front job once `join_date` is reached, which joins the thread and writes flows into `GoodsEntry` in `LinkGraphJob::~LinkGraphJob`.
- **Save/load**: `PLYR` (`src/saveload/company_sl.cpp`), `PRIC`/`CAPR`/`ECMY`/`CAPY` (`src/saveload/economy_sl.cpp`), `LGRP`/`LGRJ`/`LGRS` (`src/saveload/linkgraph_sl.cpp`). After load: `src/saveload/company_sl.cpp:AfterLoadCompanyStats` recounts infrastructure from the map, `RecomputePrices` rebuilds `_price`, `src/saveload/linkgraph_sl.cpp:AfterLoadLinkGraphs` restarts every saved job thread.
- **Teardown**: `src/linkgraph/linkgraphschedule.cpp:LinkGraphSchedule::Clear` aborts jobs before pools are cleaned (`src/misc.cpp:InitializeGame`, `ShutdownGame` in `src/openttd.cpp`).

## Invariants

- **[verified]** A job never reads live game state: the constructor in `src/linkgraph/linkgraphjob.cpp` copies the `LinkGraph` and `_settings_game.linkgraph` and fixes `join_date`; the handlers (`src/linkgraph/demands.cpp`, `src/linkgraph/mcf.cpp`, `src/linkgraph/flowmapper.cpp`) read `job.Settings()`, not globals.
- **[verified]** Results land at a fixed date on every peer: `JoinNext` only proceeds when `IsScheduledToBeJoined()` (`join_date <= date`) and `delete` blocks in `LinkGraphJob::JoinThread`; `job_completed` is an atomic hint only (comment in `LinkGraphSchedule::Run`).
- **[verified]** To avoid that block, the server posts `Commands::Pause` with `PauseMode::LinkGraph` two `date_fract` ticks before the join when the job is unfinished, and unpauses via command; only the server or single player runs it (`src/openttd.cpp:StateGameLoop`).
- **[verified]** A saved running job stores its inputs (`join_date`, the graph copy, the settings snapshot) and is recomputed after load (`SpawnAll`); if the join is already due, `AfterLoad_LinkGraphPauseControl` sets the pause flag directly.
- **[verified]** Join tolerates world changes during the run: `LinkGraphJob::~LinkGraphJob` drops flows for deleted stations, merged graphs and removed edges.
- **[verified]** Every successful command charges its cost: `src/command.cpp` calls `SubtractMoneyFromCompany` with the execute result; the cost's `ExpensesType` decides income vs expense bookkeeping and must not be `Invalid` (assert).
- **[verified]** Bankruptcy escalates by month in `src/economy.cpp:CompanyCheckBankrupt`: warn at month 4, offer for sale at 7, delete at 10 via `Commands::CompanyControl` posted only by the server or single player (a client must wait for the server's command).
- **[verified]** Maintenance is computed from the cached `src/company_base.h:CompanyInfrastructure` counters, which `src/cachecheck.cpp:CheckCaches` compares against `AfterLoadCompanyStats` under `-d desync=2`.
- **[verified]** `src/timer/timer_manager.h` orders same-period timers by pointer address, so every non-`None` `Priority` in `src/timer/timer_game_common.h` holds one timer per trigger (the header comment says "you can only setup a single timer on a single priority").
- **[verified]** `_economy.max_loan` is a multiple of `LOAN_INTERVAL` (`RecomputePrices`); `CmdIncreaseLoan` rejects amounts that are not.

## Extension points

- New price: `src/economy_type.h:Price` and a row in `src/table/pricebase.h:_price_base_specs` (the array is indexed by `Price`).
- New expense category: `src/economy_type.h:ExpensesType`; classify it in the static sets of `SubtractMoneyFromCompany`.
- New periodic economy work: an `IntervalTimer<TimerGameEconomy>` with a new `Priority` in `src/timer/timer_game_common.h` if it calls `Random()`.
- New cargodist step: a `ComponentHandler` (`src/linkgraph/linkgraphschedule.h:ComponentHandler`) added to the `handlers` array in the `LinkGraphSchedule` constructor; it may touch only the job.
- New link graph setting: `src/table/settings/linkgraph_settings.ini` (saved with each job via `GetLinkGraphJobDesc`).

## Traps and forbidden operations

- A link graph handler must not read pools, `_settings_game`, timers or `Random()`; it runs concurrently with the game loop.
- Do not delete a company or change `_current_company` from inside the state loop: `CompanyCheckBankrupt` explains why it posts a command instead.
- `ChangeOwnershipOfCompanyItems` sets the old owner's money to `UINT64_MAX >> 2` before removing property with `new_owner == INVALID_OWNER`; a new `change_tile_owner_proc` must handle `INVALID_OWNER` by removing or neutralising the item.
- A new tile or station that a company owns must update `c->infrastructure` and the recount in `AfterLoadCompanyStats`, or maintenance and the desync cache check diverge.
- **[inferred]** At the `MAX_YEAR` wrap, `src/timer/timer_game_economy.cpp` shifts `LinkGraph` dates but not running jobs' `join_date` (unlike `LinkGraphSchedule::ShiftDates`); from reading only, not tested.
- `docs/linkgraph.md` names `InitializeLinkGraphs`, which no longer exists; the clear happens in `LinkGraphSchedule::Clear`.

## Tests and validation

- Regression (needs a base set): `regression/regression/main.nut` sections `Company`, `Prices`, `Cargo`, `PrintSubsidy`; `regression/gs/main.nut` for the Game Script view.
- Unit: `src/tests/history_func.cpp`.
- Tier: `tools/gate.sh change`; threading or join changes deserve `tools/gate.sh full`.
- Debug: `-d desync=1` logs every command and saves `dmp_cmds_*.sav` every 32 days, `-d desync=2` adds cache checks (`docs/debugging_desyncs.md`); the frame-rate window measures `GameLoopLinkGraph`.

## Decisions, rules and knowledge

- Upstream architecture: [ADR 0024](../../../adrs/0024-the-cargodist-link-graph-is-computed-in-background-threads-a.md) (link graph threads), [ADR 0025](../../../adrs/0025-game-time-runs-on-separate-tick-calendar-economy-and-real-ti.md) (game timers).
- Neighbouring maps: [goals-story-news-and-history](../goals-story-news-and-history/README.md).
- Rules: `project.game-state-mutates-only-through-commands`, `project.savegame-fields-are-version-gated`, `project.public-api-is-exposed`.
- ADR 0007 (the bridge steers a company through its AI), ADR 0008 (embedded MCP server).
- Knowledge: `bridge-control-is-coarse-ai-restart`, `auto-approve-is-the-owners-switch`.
- Upstream docs: `docs/linkgraph.md`, `docs/desync.md`, `docs/debugging_desyncs.md`, `docs/savegame_format.md`.

## Typical tasks

- To change a payment rule: start at `src/economy.cpp:GetTransportedGoodsIncome`, then `src/economy.cpp:DeliverGoods`; validate with the `Prices` and `Cargo` regression sections.
- To change loans or interest: start at `src/misc_cmd.cpp:CmdIncreaseLoan` and `src/economy.cpp:CompaniesPayInterest`; validate with the `Company` regression section.
- To change bankruptcy or takeover: start at `src/economy.cpp:CompanyCheckBankrupt`, `src/company_cmd.cpp:HandleBankruptcyTakeover`, `src/economy.cpp:CmdBuyCompany`; validate in a network game, since deletion is server-posted.
- To change cargodist: start at `src/linkgraph/linkgraphschedule.cpp:LinkGraphSchedule::SpawnNext` and the handler in `src/linkgraph/mcf.cpp`; validate with `tools/gate.sh change` and a saved game with a running job reloaded.
- To expose finances to an agent: extend `src/prismatic_mcp/prismatic_mcp_server.cpp:ResourceCompanies`; validate with `tools/gate.sh research`.
