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

function Regression::StationList_Cargo()
{
	print("");
	print("--StationList_Cargo--");

	for (local mode = AIStationList_Cargo.CM_WAITING; mode <= AIStationList_Cargo.CM_PLANNED; ++mode) {
		print("  " + mode);
		for (local selector = AIStationList_Cargo.CS_BY_FROM; selector <= AIStationList_Cargo.CS_FROM_BY_VIA ; ++selector) {
			print("    " + selector);
			local list = AIStationList_Cargo(mode, selector, 6, 0, 7);
			for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
				print("      " + i + " => " + list.GetValue(i));
			}
		}
	}
};

function Regression::StationList_CargoPlanned()
{
	print("");
	print("--StationList_CargoPlanned--");

	for (local selector = AIStationList_Cargo.CS_BY_FROM; selector <= AIStationList_Cargo.CS_FROM_BY_VIA; ++selector) {
		print("    " + selector);
		local list = AIStationList_CargoPlanned(selector, 6, 0, 7);
		for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
			print("      " + i + " => " + list.GetValue(i));
		}
	}
};

function Regression::StationList_CargoPlannedByFrom()
{
	print("");
	print("--StationList_CargoPlannedByFrom--");
	local list = AIStationList_CargoPlannedByFrom(2, 0);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoPlannedByVia()
{
	print("");
	print("--StationList_CargoPlannedByVia--");
	local list = AIStationList_CargoPlannedByVia(2, 0);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoPlannedViaByFrom()
{
	print("");
	print("--StationList_CargoPlannedViaByFrom--");
	local list = AIStationList_CargoPlannedViaByFrom(6, 0, 7);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoPlannedFromByVia()
{
	print("");
	print("--StationList_CargoPlannedFromByVia--");
	local list = AIStationList_CargoPlannedFromByVia(6, 0, 7);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoWaiting()
{
	print("");
	print("--StationList_CargoWaiting--");

	for (local selector = AIStationList_Cargo.CS_BY_FROM; selector <= AIStationList_Cargo.CS_FROM_BY_VIA; ++selector) {
		print("    " + selector);
		local list = AIStationList_CargoWaiting(selector, 6, 0, 7);
		for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
			print("      " + i + " => " + list.GetValue(i));
		}
	}
};

function Regression::StationList_CargoWaitingByFrom()
{
	print("");
	print("--StationList_CargoWaitingByFrom--");
	local list = AIStationList_CargoWaitingByFrom(2, 0);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoWaitingByVia()
{
	print("");
	print("--StationList_CargoWaitingByVia--");
	local list = AIStationList_CargoWaitingByVia(2, 0);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoWaitingViaByFrom()
{
	print("");
	print("--StationList_CargoWaitingViaByFrom--");
	local list = AIStationList_CargoWaitingViaByFrom(6, 0, 7);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
	}
};

function Regression::StationList_CargoWaitingFromByVia()
{
	print("");
	print("--StationList_CargoWaitingFromByVia--");
	local list = AIStationList_CargoWaitingFromByVia(2, 0, 2);
	for (local i = list.Begin(); !list.IsEnd(); i = list.Next()) {
		print("      " + i + " => " + list.GetValue(i));
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
	StationList_Cargo();
	StationList_CargoPlanned();
	StationList_CargoPlannedByFrom();
	StationList_CargoPlannedByVia();
	StationList_CargoPlannedViaByFrom();
	StationList_CargoPlannedFromByVia();
	StationList_CargoWaiting();
	StationList_CargoWaitingByFrom();
	StationList_CargoWaitingByVia();
	StationList_CargoWaitingViaByFrom();
	StationList_CargoWaitingFromByVia();
	StationList_Vehicle();
}
