/* $Id$ */

AISign.GetMaxSignID <- function()
{
	AILog.Warning("AISign::GetMaxSignID is deprecated and will be removed soon, please use AISignList instead.");
	local list = AISignList();
	local max_id = 0;
	foreach (id, d in list) {
		if (id > max_id) max_id = id;
	}
	return max_id;
}

AITile.GetHeight <- function(tile)
{
	AILog.Warning("AITile::GetHeight is deprecated and will be removed soon, please use GetMinHeight/GetMaxHeight/GetCornerHeight instead.");
	if (!AIMap.IsValidTile(tile)) return -1;

	return AITile.GetCornerHeight(tile, AITile.CORNER_N);
}

AIOrder.ChangeOrder <- function(vehicle_id, order_position, order_flags)
{
	AILog.Warning("AIOrder::ChangeOrder is deprecated and will be removed soon, please use AIOrder::SetOrderFlags instead.")

	return AIOrder.SetOrderFlags(vehicle_id, order_position, order_flags);
}

AIWaypoint.WAYPOINT_INVALID <- 0xFFFF;

AISubsidy.SourceIsTown <- function(subsidy_id)
{
	AILog.Warning("AISubsidy::SourceIsTown is deprecated and will be removed soon, please use AISubsidy::GetSourceType instead.");
	if (!AISubsidy.IsValidSubsidy(subsidy_id) || AISubsidy.IsAwarded(subsidy_id)) return false;

	return AISubsidy.GetSourceType(subsidy_id) == AISubsidy.SPT_TOWN;
}

AISubsidy.GetSource <- function(subsidy_id)
{
	AILog.Warning("AISubsidy::GetSource is deprecated and will be removed soon, please use AISubsidy::GetSourceIndex instead.");
	if (!AISubsidy.IsValidSubsidy(subsidy_id)) return AIBaseStation.INVALID_STATION;

	if (AISubsidy.IsAwarded(subsidy_id)) {
		AILog.Error("AISubsidy::GetSource returned INVALID_STATION due to internal changes in the Subsidy logic.");
		return AIBaseStation.INVALID_STATION;
	}

	return AISubsidy.GetSourceIndex(subsidy_id);
}

AISubsidy.DestinationIsTown <- function(subsidy_id)
{
	AILog.Warning("AISubsidy::DestinationIsTown is deprecated and will be removed soon, please use AISubsidy::GetDestinationType instead.");
	if (!AISubsidy.IsValidSubsidy(subsidy_id) || AISubsidy.IsAwarded(subsidy_id)) return false;

	return AISubsidy.GetDestinationType(subsidy_id) == AISubsidy.SPT_TOWN;
}

AISubsidy.GetDestination <- function(subsidy_id)
{
	AILog.Warning("AISubsidy::GetDestination is deprecated and will be removed soon, please use AISubsidy::GetDestinationIndex instead.");
	if (!AISubsidy.IsValidSubsidy(subsidy_id)) return AIBaseStation.INVALID_STATION;

	if (AISubsidy.IsAwarded(subsidy_id)) {
		AILog.Error("AISubsidy::GetDestination returned INVALID_STATION due to internal changes in the Subsidy logic.");
		return AIBaseStation.INVALID_STATION;
	}

	return AISubsidy.GetDestinationIndex(subsidy_id);
}
