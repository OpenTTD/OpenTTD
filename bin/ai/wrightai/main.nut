/* $Id$ */

class WrightAI extends AIController {
	name = null;
	towns_used = null;
	route_1 = null;
	route_2 = null;
	distance_of_route = {};
	vehicle_to_depot = {};
	delay_build_airport_route = 1000;
	passenger_cargo_id = -1;

	function Start();

	constructor() {
		this.towns_used = AIList();
		this.route_1 = AIList();
		this.route_2 = AIList();

		local list = AICargoList();
		for (local i = list.Begin(); list.HasNext(); i = list.Next()) {
			if (AICargo.HasCargoClass(i, AICargo.CC_PASSENGERS)) {
				this.passenger_cargo_id = i;
				break;
			}
		}
	}
};

/**
 * Check if we have enough money (via loan and on bank).
 */
function WrightAI::HasMoney(money)
{
	if (AICompany.GetBankBalance(AICompany.MY_COMPANY) + (AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount()) > money) return true;
	return false;
}

/**
 * Get the amount of money requested, loan if needed.
 */
function WrightAI::GetMoney(money)
{
	if (!this.HasMoney(money)) return;
	if (AICompany.GetBankBalance(AICompany.MY_COMPANY) > money) return;

	local loan = money - AICompany.GetBankBalance(AICompany.MY_COMPANY) + AICompany.GetLoanInterval() + AICompany.GetLoanAmount();
	loan = loan - loan % AICompany.GetLoanInterval();
	AILog.Info("Need a loan to get " + money + ": " + loan);
	AICompany.SetLoanAmount(loan);
}

/**
 * Build an airport route. Find 2 cities that are big enough and try to build airport in both cities.
 *  Then we can build an aircraft and make some money.
 */
function WrightAI::BuildAirportRoute()
{
	local airport_type = (AIAirport.AirportAvailable(AIAirport.AT_SMALL) ? AIAirport.AT_SMALL : AIAirport.AT_LARGE);

	/* Get enough money to work with */
	this.GetMoney(150000);

	AILog.Info("Trying to build an airport route");

	local tile_1 = this.FindSuitableAirportSpot(airport_type, 0);
	if (tile_1 < 0) return -1;
	local tile_2 = this.FindSuitableAirportSpot(airport_type, tile_1);
	if (tile_2 < 0) {
		this.towns_used.RemoveValue(tile_1);
		return -2;
	}

	/* Build the airports for real */
	if (!AIAirport.BuildAirport(tile_1, airport_type, true)) {
		AILog.Error("Although the testing told us we could build 2 airports, it still failed on the first airport at tile " + tile_1 + ".");
		this.towns_used.RemoveValue(tile_1);
		this.towns_used.RemoveValue(tile_2);
		return -3;
	}
	if (!AIAirport.BuildAirport(tile_2, airport_type, true)) {
		AILog.Error("Although the testing told us we could build 2 airports, it still failed on the second airport at tile " + tile_2 + ".");
		AIAirport.RemoveAirport(tile_1);
		this.towns_used.RemoveValue(tile_1);
		this.towns_used.RemoveValue(tile_2);
		return -4;
	}

	local ret = this.BuildAircraft(tile_1, tile_2);
	if (ret < 0) {
		AIAirport.RemoveAirport(tile_1);
		AIAirport.RemoveAirport(tile_2);
		this.towns_used.RemoveValue(tile_1);
		this.towns_used.RemoveValue(tile_2);
		return ret;
	}

	AILog.Info("Done building a route");
	return ret;
}

/**
 * Build an aircraft with orders from tile_1 to tile_2.
 *  The best available aircraft of that time will be bought.
 */
function WrightAI::BuildAircraft(tile_1, tile_2)
{
	/* Build an aircraft */
	local hangar = AIAirport.GetHangarOfAirport(tile_1);
	local engine = null;

	local engine_list = AIEngineList(AIVehicle.VEHICLE_AIR);

	/* When bank balance < 300000, buy cheaper planes */
	local balance = AICompany.GetBankBalance(AICompany.MY_COMPANY);
	engine_list.Valuate(AIEngine.GetPrice);
	engine_list.KeepBelowValue(balance < 300000 ? 50000 : (balance < 1000000 ? 300000 : 1000000));

	engine_list.Valuate(AIEngine.GetCargoType);
	engine_list.KeepValue(this.passenger_cargo_id);

	engine_list.Valuate(AIEngine.GetCapacity);
	engine_list.KeepTop(1);

	engine = engine_list.Begin();

	if (!AIEngine.IsValidEngine(engine)) {
		AILog.Error("Couldn't find a suitable engine");
		return -5;
	}
	local vehicle = AIVehicle.BuildVehicle(hangar, engine);
	if (!AIVehicle.IsValidVehicle(vehicle)) {
		AILog.Error("Couldn't build the aircraft");
		return -6;
	}
	/* Send him on his way */
	AIOrder.AppendOrder(vehicle, tile_1, AIOrder.AIOF_NONE);
	AIOrder.AppendOrder(vehicle, tile_2, AIOrder.AIOF_NONE);
	AIVehicle.StartStopVehicle(vehicle);
	this.distance_of_route.rawset(vehicle, AIMap.DistanceManhattan(tile_1, tile_2));
	this.route_1.AddItem(vehicle, tile_1);
	this.route_2.AddItem(vehicle, tile_2);

	AILog.Info("Done building an aircraft");

	return 0;
}

/**
 * Find a suitable spot for an airport, walking all towns hoping to find one.
 *  When a town is used, it is marked as such and not re-used.
 */
function WrightAI::FindSuitableAirportSpot(airport_type, center_tile)
{
	local airport_x, airport_y, airport_rad;

	airport_x = AIAirport.GetAirportWidth(airport_type);
	airport_y = AIAirport.GetAirportHeight(airport_type);
	airport_rad = AIAirport.GetAirportCoverageRadius(airport_type);

	local town_list = AITownList();
	/* Remove all the towns we already used */
	town_list.RemoveList(this.towns_used);

	town_list.Valuate(AITown.GetPopulation);
	town_list.KeepAboveValue(GetSetting("min_town_size"));
	/* Keep the best 10, if we can't find 2 stations in there, just leave it anyway */
	town_list.KeepTop(10);
	town_list.Valuate(AIBase.RandItem);

	/* Now find 2 suitable towns */
	for (local town = town_list.Begin(); town_list.HasNext(); town = town_list.Next()) {
		/* Don't make this a CPU hog */
		Sleep(1);

		local tile = AITown.GetLocation(town);

		/* Create a 30x30 grid around the core of the town and see if we can find a spot for a small airport */
		local list = AITileList();
		/* XXX -- We assume we are more than 15 tiles away from the border! */
		list.AddRectangle(tile - AIMap.GetTileIndex(15, 15), tile + AIMap.GetTileIndex(15, 15));
		list.Valuate(AITile.IsBuildableRectangle, airport_x, airport_y);
		list.KeepValue(1);
		if (center_tile != 0) {
			/* If we have a tile defined, we don't want to be within 25 tiles of this tile */
			list.Valuate(AITile.GetDistanceSquareToTile, center_tile);
			list.KeepAboveValue(625);
		}
		/* Sort on acceptance, remove places that don't have acceptance */
		list.Valuate(AITile.GetCargoAcceptance, this.passenger_cargo_id, airport_x, airport_y, airport_rad);
		list.RemoveBelowValue(10);

		/* Couldn't find a suitable place for this town, skip to the next */
		if (list.Count() == 0) continue;
		/* Walk all the tiles and see if we can build the airport at all */
		{
			local test = AITestMode();
			local good_tile = 0;

			for (tile = list.Begin(); list.HasNext(); tile = list.Next()) {
				Sleep(1);
				if (!AIAirport.BuildAirport(tile, airport_type, true)) continue;
				good_tile = tile;
				break;
			}

			/* Did we found a place to build the airport on? */
			if (good_tile == 0) continue;
		}

		AILog.Info("Found a good spot for an airport in town " + town + " at tile " + tile);

		/* Make the town as used, so we don't use it again */
		this.towns_used.AddItem(town, tile);

		return tile;
	}

	AILog.Info("Couldn't find a suitable town to build an airport in");
	return -1;
}

function WrightAI::ManageAirRoutes()
{
	local list = AIVehicleList();
	list.Valuate(AIVehicle.GetAge);
	/* Give the plane at least 2 years to make a difference */
	list.KeepAboveValue(365 * 2);
	list.Valuate(AIVehicle.GetProfitLastYear);

	for (local i = list.Begin(); list.HasNext(); i = list.Next()) {
		local profit = list.GetValue(i);
		/* Profit last year and this year bad? Let's sell the vehicle */
		if (profit < 10000 && AIVehicle.GetProfitThisYear(i) < 10000) {
			/* Send the vehicle to depot if we didn't do so yet */
			if (!vehicle_to_depot.rawin(i) || vehicle_to_depot.rawget(i) != true) {
				AILog.Info("Sending " + i + " to depot as profit is: " + profit + " / " + AIVehicle.GetProfitThisYear(i));
				AIVehicle.SendVehicleToDepot(i);
				vehicle_to_depot.rawset(i, true);
			}
		}
		/* Try to sell it over and over till it really is in the depot */
		if (vehicle_to_depot.rawin(i) && vehicle_to_depot.rawget(i) == true) {
			if (AIVehicle.SellVehicle(i)) {
				AILog.Info("Selling " + i + " as it finally is in a depot.");
				/* Check if we are the last one serving those airports; else sell the airports */
				local list2 = AIVehicleList_Station(AIStation.GetStationID(this.route_1.GetValue(i)));
				if (list2.Count() == 0) this.SellAirports(i);
				vehicle_to_depot.rawdelete(i);
			}
		}
	}

	/* Don't try to add planes when we are short on cash */
	if (!this.HasMoney(50000)) return;

	list = AIStationList(AIStation.STATION_AIRPORT);
	list.Valuate(AIStation.GetCargoWaiting, this.passenger_cargo_id);
	list.KeepAboveValue(250);

	for (local i = list.Begin(); list.HasNext(); i = list.Next()) {
		local list2 = AIVehicleList_Station(i);
		/* No vehicles going to this station, abort and sell */
		if (list2.Count() == 0) {
			this.SellAirports(i);
			continue;
		};

		/* Find the first vehicle that is going to this station */
		local v = list2.Begin();
		local dist = this.distance_of_route.rawget(v);

		list2.Valuate(AIVehicle.GetAge);
		list2.KeepBelowValue(dist);
		/* Do not build a new vehicle if we bought a new one in the last DISTANCE days */
		if (list2.Count() != 0) continue;

		AILog.Info("Station " + i + " (" + AIStation.GetLocation(i) + ") has too many cargo, adding a new vehicle for the route.");

		/* Make sure we have enough money */
		this.GetMoney(50000);

		return this.BuildAircraft(this.route_1.GetValue(v), this.route_2.GetValue(v));
	}
}

/**
  * Sells the airports from route index i
  * Removes towns from towns_used list too
  */
function WrightAI::SellAirports(i) {
	/* Remove the airports */
	AILog.Info("Removing airports as nobody serves them anymore.");
	AIAirport.RemoveAirport(this.route_1.GetValue(i));
	AIAirport.RemoveAirport(this.route_2.GetValue(i));
	/* Free the towns_used entries */
	this.towns_used.RemoveValue(this.route_1.GetValue(i));
	this.towns_used.RemoveValue(this.route_2.GetValue(i));
	/* Remove the route */
	this.route_1.RemoveItem(i);
	this.route_2.RemoveItem(i);
}

function WrightAI::HandleEvents()
{
	while (AIEventController.IsEventWaiting()) {
		local e = AIEventController.GetNextEvent();
		switch (e.GetEventType()) {
			case AIEvent.AI_ET_VEHICLE_CRASHED: {
				local ec = AIEventVehicleCrashed.Convert(e);
				local v = ec.GetVehicleID();
				AILog.Info("We have a crashed vehicle (" + v + "), buying a new one as replacement");
				this.BuildAircraft(this.route_1.GetValue(v), this.route_2.GetValue(v));
				this.route_1.RemoveItem(v);
				this.route_2.RemoveItem(v);
			} break;

			default:
				break;
		}
	}
}

function WrightAI::Start()
{
	if (this.passenger_cargo_id == -1) {
		AILog.Error("WrightAI could not find the passenger cargo");
		return;
	}

	/* Give the boy a name */
	if (!AICompany.SetName("WrightAI")) {
		local i = 2;
		while (!AICompany.SetName("WrightAI #" + i)) {
			i++;
		}
	}
	this.name = AICompany.GetName(AICompany.MY_COMPANY);
	/* Say hello to the user */
	AILog.Info("Welcome to WrightAI. I will be building airports all day long.");
	AILog.Info("  - Minimum Town Size: " + GetSetting("min_town_size"));

	/* We start with almost no loan, and we take a loan when we want to build something */
	AICompany.SetLoanAmount(AICompany.GetLoanInterval());

	/* We need our local ticker, as GetTick() will skip ticks */
	local ticker = 0;
	/* Determine time we may sleep */
	local sleepingtime = 100;
	if (this.delay_build_airport_route < sleepingtime)
		sleepingtime = this.delay_build_airport_route;

	/* Let's go on for ever */
	while (true) {
		/* Once in a while, with enough money, try to build something */
		if ((ticker % this.delay_build_airport_route == 0 || ticker == 0) && this.HasMoney(100000)) {
			local ret = this.BuildAirportRoute();
			if (ret == -1 && ticker != 0) {
				/* No more route found, delay even more before trying to find an other */
				this.delay_build_airport_route = 10000;
			}
			else if (ret < 0 && ticker == 0) {
				/* The AI failed to build a first airport and is deemed */
				AICompany.SetName("Failed " + this.name);
				AILog.Error("Failed to build first airport route, now giving up building. Repaying loan. Have a nice day!");
				AICompany.SetLoanAmount(0);
				return;
			}
		}
		/* Manage the routes once in a while */
		if (ticker % 2000 == 0) {
			this.ManageAirRoutes();
		}
		/* Try to get ride of our loan once in a while */
		if (ticker % 5000 == 0) {
			AICompany.SetLoanAmount(0);
		}
		/* Check for events once in a while */
		if (ticker % 100 == 0) {
			this.HandleEvents();
		}
		/* Make sure we do not create infinite loops */
		Sleep(sleepingtime);
		ticker += sleepingtime;
	}
}

