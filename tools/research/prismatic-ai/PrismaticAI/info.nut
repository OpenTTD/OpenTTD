/*
 * Minimal company AI whose whole purpose is to be externally steerable via
 * the bridge's /ai/start and /ai/reload/{company_id} endpoints (which wrap
 * OpenTTD's `start_ai`/`reload_ai` console commands over the Admin Network).
 *
 * It intentionally does NOT implement real gameplay strategy yet -- see
 * main.nut. It's the hook point: settings below become tunable from outside
 * the game via the bridge, without touching OpenTTD's C++ core.
 */
class PrismaticAI extends AIInfo {
	function GetAuthor()      { return "prismatic-platform"; }
	function GetName()        { return "PrismaticAI"; }
	function GetShortName()   { return "PRIS"; }
	function GetDescription() { return "Company AI steerable from outside the game via the openttd-prismatic-bridge REST API."; }
	function GetVersion()     { return 2; }
	function GetAPIVersion()  { return "16"; }
	function GetDate()        { return "2026-11-18"; }
	function CreateInstance() { return "PrismaticAI"; }
	function UseAsRandomAI()  { return false; }

	function GetSettings() {
		AddSetting({
			name = "aggressiveness",
			description = "How aggressively to expand (0 = passive, 100 = maximal). Set via start_ai/reload_ai settings string, e.g. 'aggressiveness=75'.",
			min_value = 0,
			max_value = 100,
			easy_value = 20, medium_value = 20, hard_value = 20, custom_value = 20,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "heartbeat_seconds",
			description = "How often (in-game seconds) the AI logs a heartbeat -- useful to confirm the bridge can see it's alive via /rcon list_ai and the Admin console feed.",
			min_value = 1,
			max_value = 300,
			easy_value = 30,
			medium_value = 30,
			hard_value = 30,
			custom_value = 30,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "max_routes",
			description = "How many bus routes (one per town) the company may run. Raising it while the AI runs makes it expand.",
			min_value = 1,
			max_value = 30,
			easy_value = 2,
			medium_value = 4,
			hard_value = 8,
			custom_value = 4,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "cash_reserve",
			description = "Money the company keeps in the bank: no new route is built while the balance is below it.",
			min_value = 0,
			max_value = 500000,
			easy_value = 40000,
			medium_value = 20000,
			hard_value = 5000,
			custom_value = 20000,
			step_size = 5000,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "intercity",
			description = "1: connect pairs of towns with a road the bot builds tile by tile and run buses between them; 0: routes inside one town only.",
			min_value = 0,
			max_value = 1,
			easy_value = 1,
			medium_value = 1,
			hard_value = 1,
			custom_value = 1,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddSetting({
			name = "air_a",
			description = "First town of the air route the owner's planner chose (town id; -1: none). Set by the panel from mcp-plan by_mode.air.",
			min_value = -1,
			max_value = 65535,
			easy_value = -1,
			medium_value = -1,
			hard_value = -1,
			custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_b",
			description = "Second town of the planner's air route (town id; -1: none).",
			min_value = -1,
			max_value = 65535,
			easy_value = -1,
			medium_value = -1,
			hard_value = -1,
			custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_a",
			description = "First town of the rail route the owner's planner chose (town id; -1: none). Set by the panel from mcp-plan by_mode.rail.",
			min_value = -1, max_value = 65535,
			easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_b",
			description = "Second town of the planner's rail route (town id; -1: none).",
			min_value = -1, max_value = 65535,
			easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_after",
			description = "The planner's freight lines built before its passenger rail pair: those that score more (1: after the first, as before).",
			min_value = 0, max_value = 16,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_wagons",
			description = "Wagons behind the locomotive of a new rail line's train (0: derive from the line's monthly supply, distance and speed, and fill the platform; rail.nut PlatformPlan).",
			min_value = 0, max_value = 30,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_len",
			description = "Platform length in tiles of a new rail line (0: derive from demand, capped by the game's vehicle.max_train_length and station.station_spread).",
			min_value = 0, max_value = 16,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_platforms",
			description = "Platforms at each terminus of a new rail line: 2 builds a throat with path signals so two trains share the single track (each waits in a platform, never on the line); 1 is one platform and one train.",
			min_value = 1, max_value = 2,
			easy_value = 2, medium_value = 2, hard_value = 2, custom_value = 2, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "fr_from",
			description = "Producing industry of the freight rail route the owner's planner chose (industry id; -1: none). Set by the panel from the planner's portfolio.",
			min_value = -1, max_value = 65535,
			easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "fr_to",
			description = "Accepting industry of the planner's freight rail route (industry id; -1: none).",
			min_value = -1, max_value = 65535,
			easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "fr_cargo",
			description = "Cargo of the planner's freight rail route (cargo id; -1: none).",
			min_value = -1, max_value = 63,
			easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
			flags = CONFIG_INGAME,
		});
		/* The planner's next freight rail lines, in its order (fr2_* to fr16_*): the bot builds each
		 * one once the money and the loan headroom cover a new line. Sixteen: after four lines a
		 * 21-game-year duel left the bot's money idle, and after eight the best 36-month run on
		 * OpenTTD seed 31337 held 308074 idle with its loan repaid. FreightLines (main.nut) reads the
		 * same sixteen. */
		for (local i = 2; i <= 16; i++) {
			AddSetting({
				name = "fr" + i + "_from",
				description = "Producing industry of the planner's freight rail line " + i + " (industry id; -1: none).",
				min_value = -1, max_value = 65535,
				easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
				flags = CONFIG_INGAME,
			});
			AddSetting({
				name = "fr" + i + "_to",
				description = "Accepting industry of the planner's freight rail line " + i + " (industry id; -1: none).",
				min_value = -1, max_value = 65535,
				easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
				flags = CONFIG_INGAME,
			});
			AddSetting({
				name = "fr" + i + "_cargo",
				description = "Cargo of the planner's freight rail line " + i + " (cargo id; -1: none).",
				min_value = -1, max_value = 63,
				easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
				flags = CONFIG_INGAME,
			});
		}
		/* The Rhai steward's targets for each rail line, by slot (lines.nut; set through
		 * openttd.ai.set_setting, gated like every change): trains (0 closes the line) and wagons per
		 * train. 24 slots: the planner's 16 freight lines, its passenger pair and the bot's own. The
		 * steward derives the slot count from these declarations, never from a copy of it. */
		for (local i = 1; i <= 24; i++) {
			AddSetting({
				name = "line" + i + "_trains",
				description = "Trains on rail line " + i + " (the steward's target; 0 closes the line, -1: no target).",
				min_value = -1, max_value = 8,
				easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
				flags = CONFIG_INGAME,
			});
			AddSetting({
				name = "line" + i + "_wagons",
				description = "Wagons per train on rail line " + i + " (the steward's target; -1: no target).",
				min_value = -1, max_value = 30,
				easy_value = -1, medium_value = -1, hard_value = -1, custom_value = -1,
				flags = CONFIG_INGAME,
			});
		}
		AddSetting({
			name = "own_lines",
			description = "Freight rail lines the bot plans itself once every line the planner handed over was tried and money waits (0: none; the panel sets it for a bot it starts from a plan, so a bot started without one stays unplanned).",
			min_value = 0, max_value = 32,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "line_budget",
			description = "Money (bank balance plus loan headroom) the bot wants at hand before it builds the planner's next freight rail line.",
			min_value = 10000, max_value = 1000000,
			easy_value = 90000, medium_value = 90000, hard_value = 90000, custom_value = 90000, step_size = 10000,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "disclose_name",
			description = "Name the company so it reads as an AI the host steers (\"<name> (AI)\"), for a real game against people (research/real-game.md). 0 keeps the game's own name.",
			min_value = 0, max_value = 1,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddSetting({
			name = "air_routes_max",
			description = "Autopilot: most own air routes (autopilot.nut).",
			min_value = 0, max_value = 30,
			easy_value = 3, medium_value = 3, hard_value = 3, custom_value = 3, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_towns",
			description = "Autopilot: how many of the most populous towns air pairs are chosen from.",
			min_value = 4, max_value = 60,
			easy_value = 50, medium_value = 50, hard_value = 50, custom_value = 50, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_min_tiles",
			description = "Autopilot: shortest air pair, in tiles.",
			min_value = 20, max_value = 400,
			easy_value = 60, medium_value = 60, hard_value = 60, custom_value = 60, step_size = 10,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_max_tiles",
			description = "Autopilot: longest air pair, in tiles.",
			min_value = 60, max_value = 800,
			easy_value = 420, medium_value = 420, hard_value = 420, custom_value = 420, step_size = 10,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "foreign_airports",
			description = "Autopilot: skip a town with this many airports of other companies within 16 tiles (0: never skip).",
			min_value = 0, max_value = 5,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "invest_min",
			description = "Fleet: idle money (in thousands) above which InvestSurplus buys vehicles where they earn most.",
			min_value = 0, max_value = 2000,
			easy_value = 50, medium_value = 50, hard_value = 50, custom_value = 50, step_size = 10,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "invest_ppv_div",
			description = "Fleet: a route gets one more vehicle while its vehicles earn more than price / this per year.",
			min_value = 1, max_value = 10,
			easy_value = 2, medium_value = 2, hard_value = 2, custom_value = 2, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "invest_road_cap",
			description = "Fleet: most road vehicles per route that InvestSurplus grows to.",
			min_value = 1, max_value = 40,
			easy_value = 20, medium_value = 20, hard_value = 20, custom_value = 20, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "invest_air_cap",
			description = "Fleet: most planes per route (InvestSurplus, and fleet growth with the steward on).",
			min_value = 1, max_value = 30,
			easy_value = 12, medium_value = 12, hard_value = 12, custom_value = 12, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "reinvest_budgets",
			description = "Steward: own lines beyond own_lines need this many line budgets in the bank.",
			min_value = 1, max_value = 10,
			easy_value = 2, medium_value = 2, hard_value = 2, custom_value = 2, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "catchment_stops",
			description = "Catchment ring (learned from AAAHogEx): most joined drive-through road stops around each passenger airport, so it collects the whole town (catchment.nut; default 8 together with air_add_waiting=10, see its evidence; 0: off).",
			min_value = 0, max_value = 30,
			easy_value = 8, medium_value = 8, hard_value = 8, custom_value = 8, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "catchment_spacing",
			description = "Catchment ring: tiles between two of its road stops.",
			min_value = 2, max_value = 10,
			easy_value = 4, medium_value = 4, hard_value = 4, custom_value = 4, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "statue_money",
			description = "Statues (learned from AAAHogEx): money in thousands above which the bot builds a statue in each town next to its stations (statues.nut; default 0: off, it cost ~30% on the fitness seed at 6 years; the battle's GA explores it).",
			min_value = 0, max_value = 5000,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 50,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_full_load",
			description = "1: planes wait for a full load at both airports (learned from AAAHogEx); 0 (default): they leave at once; full load cost ~5% on the fitness seed at 6 years.",
			min_value = 0, max_value = 1,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "invest_borrow",
			description = "1: InvestSurplus may borrow up to the loan limit for a vehicle that pays back (learned from AAAHogEx, which keeps its cash near zero); 0 (default): bank money only; borrowing changed nothing on the fitness seed at 6 years.",
			min_value = 0, max_value = 1,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "air_add_waiting",
			description = "Planes (learned from AAAHogEx): add a plane to an air route while the passengers waiting at BOTH of its airports exceed this, up to invest_air_cap (fleet.nut; default 10 together with catchment_stops=8 since 2026-11-19: in the battle, 20 years, paired on 36 common seeds, +14.8% mean value and better in 27 of 36; 0: the old rule of 1.5 loads at one end).",
			min_value = 0, max_value = 500,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 10,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_twin_max",
			description = "Rail capacity (after AAAHogEx's double track): most twin lines, each a second line with its own track and platforms between the same two industries, built when a line runs all its trains and still has a trainload waiting (steward.nut TwinLines; 0: off).",
			min_value = 0, max_value = 24,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0, step_size = 1,
			flags = CONFIG_INGAME,
		});
		AddSetting({
			name = "rail_double",
			description = "1: the bot's own new freight lines are double track (after AAAHogEx): one one-way track each way with path signals, so a line runs many trains and the in-bot steward adds one while half a trainload waits and no train loads at the source (rail.nut BuildDoubleRailLine); 0 (default): single track, two trains a line. Default 1 since 2026-11-19: in the battle, 20 years, paired on 74 common seeds, +14.8% mean value and better in 56 of 74; 0: single track.",
			min_value = 0, max_value = 1,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddSetting({
			name = "autopilot",
			description = "1 (default): a bot started without a plan from the panel (no planner line, air or rail pair) grows on its own: in-bot steward, own freight lines (at least 8), intercity roads, up to 30 routes and a 5000 reserve. 0: the unplanned bot stays as small as its settings say (the duels' rival before 2026-11-19).",
			min_value = 0, max_value = 1,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddSetting({
			name = "self_steward",
			description = "1: with no steward's target for a rail line, the bot applies the Rhai steward's policy itself (lengthen, add a train, shrink, close) and reinvests idle money in more own lines beyond own_lines (steward.nut); 0: lines run what the external steward sets.",
			min_value = 0, max_value = 1,
			easy_value = 0, medium_value = 0, hard_value = 0, custom_value = 0,
			flags = CONFIG_INGAME | CONFIG_BOOLEAN,
		});
		AddSetting({
			name = "air_planes",
			description = "Aircraft on the planner's air route.",
			min_value = 1,
			max_value = 8,
			easy_value = 1, medium_value = 1, hard_value = 1, custom_value = 1,
			step_size = 1,
			flags = CONFIG_INGAME,
		});
	}
}

RegisterAI(PrismaticAI());
