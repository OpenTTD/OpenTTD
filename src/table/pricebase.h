/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pricebase.h Table of all default price bases. */

static const EnumIndexArray<PriceBaseSpec, Price, Price::End> _price_base_specs = {
	PriceBaseSpec(100, PCAT_NONE, GrfSpecFeature::End, Price::Invalid), // Price::StationValue
	PriceBaseSpec(100, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildRail
	PriceBaseSpec(95, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildRoad
	PriceBaseSpec(65, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildSignals
	PriceBaseSpec(275, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildBridge
	PriceBaseSpec(600, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotTrain
	PriceBaseSpec(500, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotRoad
	PriceBaseSpec(700, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotShip
	PriceBaseSpec(450, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildTunnel
	PriceBaseSpec(200, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationRail
	PriceBaseSpec(180, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationRailLength
	PriceBaseSpec(600, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationAirport
	PriceBaseSpec(200, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationBus
	PriceBaseSpec(200, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationTruck
	PriceBaseSpec(350, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationDock
	PriceBaseSpec(400000, PCAT_CONSTRUCTION, GrfSpecFeature::Trains, Price::Invalid), // Price::BuildVehicleTrain
	PriceBaseSpec(2000, PCAT_CONSTRUCTION, GrfSpecFeature::Trains, Price::Invalid), // Price::BuildVehicleWagon
	PriceBaseSpec(700000, PCAT_CONSTRUCTION, GrfSpecFeature::Aircraft, Price::Invalid), // Price::BuildVehicleAircraft
	PriceBaseSpec(14000, PCAT_CONSTRUCTION, GrfSpecFeature::RoadVehicles, Price::Invalid), // Price::BuildVehicleRoad
	PriceBaseSpec(65000, PCAT_CONSTRUCTION, GrfSpecFeature::Ships, Price::Invalid), // Price::BuildVehicleShip
	PriceBaseSpec(20, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildTrees
	PriceBaseSpec(250, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::Terraform
	PriceBaseSpec(20, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearGrass
	PriceBaseSpec(40, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearRough
	PriceBaseSpec(200, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearRocks
	PriceBaseSpec(500, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearFields
	PriceBaseSpec(20, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearTrees
	PriceBaseSpec(-70, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearRail
	PriceBaseSpec(10, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearSignals
	PriceBaseSpec(50, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearBridge
	PriceBaseSpec(80, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotTrain
	PriceBaseSpec(80, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotRoad
	PriceBaseSpec(90, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotShip
	PriceBaseSpec(30, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearTunnel
	PriceBaseSpec(10000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearWater
	PriceBaseSpec(50, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationRail
	PriceBaseSpec(30, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationAirport
	PriceBaseSpec(50, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationBus
	PriceBaseSpec(50, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationTruck
	PriceBaseSpec(55, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationDock
	PriceBaseSpec(1600, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearHouse
	PriceBaseSpec(40, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::ClearRoad
	PriceBaseSpec(5600, PCAT_RUNNING, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainSteam
	PriceBaseSpec(5200, PCAT_RUNNING, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainDiesel
	PriceBaseSpec(4800, PCAT_RUNNING, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainElectric
	PriceBaseSpec(9600, PCAT_RUNNING, GrfSpecFeature::Aircraft, Price::Invalid), // Price::RunningAircraft
	PriceBaseSpec(1600, PCAT_RUNNING, GrfSpecFeature::RoadVehicles, Price::Invalid), // Price::RunningRoadveh
	PriceBaseSpec(5600, PCAT_RUNNING, GrfSpecFeature::Ships, Price::Invalid), // Price::RunningShip
	PriceBaseSpec(1000000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Invalid), // Price::BuildIndustry
	PriceBaseSpec(1600, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearHouse), // Price::ClearIndustry
	PriceBaseSpec(40, PCAT_CONSTRUCTION, GrfSpecFeature::Objects, Price::ClearRough), // Price::BuildObject
	PriceBaseSpec(40, PCAT_CONSTRUCTION, GrfSpecFeature::Objects, Price::ClearRough), // Price::ClearObject
	PriceBaseSpec(600, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::BuildDepotTrain), // Price::BuildWaypointRail
	PriceBaseSpec(80, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearDepotTrain), // Price::ClearWaypointRail
	PriceBaseSpec(350, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::BuildStationDock), // Price::BuildWaypointBuoy
	PriceBaseSpec(50, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearStationTruck), // Price::ClearWaypointBuoy
	PriceBaseSpec(1000000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::BuildIndustry), // Price::TownAction
	PriceBaseSpec(250, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::Terraform), // Price::BuildFoundation
	PriceBaseSpec(8000000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::BuildIndustry), // Price::BuildIndustryRaw
	PriceBaseSpec(1000000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::BuildIndustry), // Price::BuildTown
	PriceBaseSpec(5000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearWater), // Price::BuildCanal
	PriceBaseSpec(5000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearWater), // Price::ClearCanal
	PriceBaseSpec(10000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearWater), // Price::BuildAqueduct
	PriceBaseSpec(2000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearBridge), // Price::ClearAqueduct
	PriceBaseSpec(7500, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearWater), // Price::BuildLock
	PriceBaseSpec(2000, PCAT_CONSTRUCTION, GrfSpecFeature::End, Price::ClearWater), // Price::ClearLock
	PriceBaseSpec(10, PCAT_RUNNING, GrfSpecFeature::End, Price::BuildRail), // Price::InfrastructureRail
	PriceBaseSpec(10, PCAT_RUNNING, GrfSpecFeature::End, Price::BuildRoad), // Price::InfrastructureRoad
	PriceBaseSpec(8, PCAT_RUNNING, GrfSpecFeature::End, Price::BuildCanal), // Price::InfrastructureWater
	PriceBaseSpec(100, PCAT_RUNNING, GrfSpecFeature::End, Price::StationValue), // Price::InfrastructureStation
	PriceBaseSpec(5000, PCAT_RUNNING, GrfSpecFeature::End, Price::BuildStationAirport), // Price::InfrastructureAirport
};
