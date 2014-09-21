/* $Id$ */

class Regression extends AIController {
	function Start();
};


function Regression::StationList()
{
	local list = AIStationList(AIStation.STATION_BUS_STOP + AIStation.STATION_TRUCK_STOP);

	print("");
	print("--StationList--");
	print("  Count():             " + list.Count());
	list.Valuate(AIStation.GetLocation);
	print("  Location ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetCargoWaiting, 0);
	print("  CargoWaiting(0) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetCargoWaiting, 1);
	print("  CargoWaiting(1) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_Vehicle()
{
	local list = AIStationList_Vehicle(12);

	print("");
	print("--StationList_Vehicle--");
	print("  Count():             " + list.Count());
	list.Valuate(AIStation.GetLocation);
	print("  Location ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetCargoWaiting, 0);
	print("  CargoWaiting(0) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetCargoWaiting, 1);
	print("  CargoWaiting(1) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetCargoRating, 1);
	print("  CargoRating(1) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetDistanceManhattanToTile, 30000);
	print("  DistanceManhattanToTile(30000) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.GetDistanceSquareToTile, 30000);
	print("  DistanceSquareToTile(30000) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
	list.Valuate(AIStation.IsWithinTownInfluence, 0);
	print("  IsWithinTownInfluence(0) ListDump:");
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("    " + i + " => " + list.GetValue(i));
	}
}

function Regression::Start()
{
	StationList();
	StationList_Vehicle();
}
