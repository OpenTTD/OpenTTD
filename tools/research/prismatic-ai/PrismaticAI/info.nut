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
			easy_value = 25,
			medium_value = 50,
			hard_value = 75,
			custom_value = 50,
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
			easy_value = 80000, medium_value = 80000, hard_value = 80000, custom_value = 80000, step_size = 10000,
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
			easy_value = 2,
			medium_value = 2,
			hard_value = 3,
			custom_value = 2,
			step_size = 1,
			flags = CONFIG_INGAME,
		});
	}
}

RegisterAI(PrismaticAI());
