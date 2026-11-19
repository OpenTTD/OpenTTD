/*
 * Statues (RESEARCH-ONLY; setting statue_money, in thousands; 0 = off).
 *
 * Learned from AAAHogEx (main.nut, statues once the bank holds 400k): a statue in a town raises the
 * rating of every station of ours there, so more of its passengers and cargo come to us. Once the
 * bank holds `statue_money` thousand, the bot builds one statue a heartbeat in a town next to one
 * of its stations that has none yet.
 */

function PrismaticAI::BuildStatues(b) {
	if (b.statue_money <= 0) return;
	if (AICompany.GetBankBalance(AICompany.COMPANY_SELF) < b.statue_money * 1000) return;
	local seen = {};
	foreach (st, _ in AIStationList(AIStation.STATION_ANY)) {
		local town = AIStation.GetNearestTown(st);
		if (!AITown.IsValidTown(town) || seen.rawin(town)) continue;
		seen[town] <- true;
		if (AITown.HasStatue(town) || !AITown.IsActionAvailable(town, AITown.TOWN_ACTION_BUILD_STATUE)) continue;
		if (AITown.PerformTownAction(town, AITown.TOWN_ACTION_BUILD_STATUE)) {
			AILog.Info("PrismaticAI: statue built in " + AITown.GetName(town));
			return;
		}
	}
}
