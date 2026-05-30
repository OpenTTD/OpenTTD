/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pricebase.h Table of all default price bases. */

static const EnumIndexArray<PriceBaseSpec, Price, Price::End> _price_base_specs = {
	PriceBaseSpec(100, PriceCategory::None, GrfSpecFeature::End, Price::Invalid), // Price::StationValue
	PriceBaseSpec(100, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildRail
	PriceBaseSpec(95, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildRoad
	PriceBaseSpec(65, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildSignals
	PriceBaseSpec(275, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildBridge
	PriceBaseSpec(600, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotTrain
	PriceBaseSpec(500, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotRoad
	PriceBaseSpec(700, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildDepotShip
	PriceBaseSpec(450, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildTunnel
	PriceBaseSpec(200, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationRail
	PriceBaseSpec(180, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationRailLength
	PriceBaseSpec(600, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationAirport
	PriceBaseSpec(200, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationBus
	PriceBaseSpec(200, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationTruck
	PriceBaseSpec(350, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildStationDock
	PriceBaseSpec(400000, PriceCategory::Construction, GrfSpecFeature::Trains, Price::Invalid), // Price::BuildVehicleTrain
	PriceBaseSpec(2000, PriceCategory::Construction, GrfSpecFeature::Trains, Price::Invalid), // Price::BuildVehicleWagon
	PriceBaseSpec(700000, PriceCategory::Construction, GrfSpecFeature::Aircraft, Price::Invalid), // Price::BuildVehicleAircraft
	PriceBaseSpec(14000, PriceCategory::Construction, GrfSpecFeature::RoadVehicles, Price::Invalid), // Price::BuildVehicleRoad
	PriceBaseSpec(65000, PriceCategory::Construction, GrfSpecFeature::Ships, Price::Invalid), // Price::BuildVehicleShip
	PriceBaseSpec(20, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildTrees
	PriceBaseSpec(250, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::Terraform
	PriceBaseSpec(20, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearGrass
	PriceBaseSpec(40, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearRough
	PriceBaseSpec(200, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearRocks
	PriceBaseSpec(500, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearFields
	PriceBaseSpec(20, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearTrees
	PriceBaseSpec(-70, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearRail
	PriceBaseSpec(10, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearSignals
	PriceBaseSpec(50, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearBridge
	PriceBaseSpec(80, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotTrain
	PriceBaseSpec(80, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotRoad
	PriceBaseSpec(90, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearDepotShip
	PriceBaseSpec(30, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearTunnel
	PriceBaseSpec(10000, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearWater
	PriceBaseSpec(50, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationRail
	PriceBaseSpec(30, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationAirport
	PriceBaseSpec(50, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationBus
	PriceBaseSpec(50, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationTruck
	PriceBaseSpec(55, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearStationDock
	PriceBaseSpec(1600, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearHouse
	PriceBaseSpec(40, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::ClearRoad
	PriceBaseSpec(5600, PriceCategory::Running, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainSteam
	PriceBaseSpec(5200, PriceCategory::Running, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainDiesel
	PriceBaseSpec(4800, PriceCategory::Running, GrfSpecFeature::Trains, Price::Invalid), // Price::RunningTrainElectric
	PriceBaseSpec(9600, PriceCategory::Running, GrfSpecFeature::Aircraft, Price::Invalid), // Price::RunningAircraft
	PriceBaseSpec(1600, PriceCategory::Running, GrfSpecFeature::RoadVehicles, Price::Invalid), // Price::RunningRoadveh
	PriceBaseSpec(5600, PriceCategory::Running, GrfSpecFeature::Ships, Price::Invalid), // Price::RunningShip
	PriceBaseSpec(1000000, PriceCategory::Construction, GrfSpecFeature::End, Price::Invalid), // Price::BuildIndustry
	PriceBaseSpec(1600, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearHouse), // Price::ClearIndustry
	PriceBaseSpec(40, PriceCategory::Construction, GrfSpecFeature::Objects, Price::ClearRough), // Price::BuildObject
	PriceBaseSpec(40, PriceCategory::Construction, GrfSpecFeature::Objects, Price::ClearRough), // Price::ClearObject
	PriceBaseSpec(600, PriceCategory::Construction, GrfSpecFeature::End, Price::BuildDepotTrain), // Price::BuildWaypointRail
	PriceBaseSpec(80, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearDepotTrain), // Price::ClearWaypointRail
	PriceBaseSpec(350, PriceCategory::Construction, GrfSpecFeature::End, Price::BuildStationDock), // Price::BuildWaypointBuoy
	PriceBaseSpec(50, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearStationTruck), // Price::ClearWaypointBuoy
	PriceBaseSpec(1000000, PriceCategory::Construction, GrfSpecFeature::End, Price::BuildIndustry), // Price::TownAction
	PriceBaseSpec(250, PriceCategory::Construction, GrfSpecFeature::End, Price::Terraform), // Price::BuildFoundation
	PriceBaseSpec(8000000, PriceCategory::Construction, GrfSpecFeature::End, Price::BuildIndustry), // Price::BuildIndustryRaw
	PriceBaseSpec(1000000, PriceCategory::Construction, GrfSpecFeature::End, Price::BuildIndustry), // Price::BuildTown
	PriceBaseSpec(5000, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearWater), // Price::BuildCanal
	PriceBaseSpec(5000, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearWater), // Price::ClearCanal
	PriceBaseSpec(10000, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearWater), // Price::BuildAqueduct
	PriceBaseSpec(2000, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearBridge), // Price::ClearAqueduct
	PriceBaseSpec(7500, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearWater), // Price::BuildLock
	PriceBaseSpec(2000, PriceCategory::Construction, GrfSpecFeature::End, Price::ClearWater), // Price::ClearLock
	PriceBaseSpec(10, PriceCategory::Running, GrfSpecFeature::End, Price::BuildRail), // Price::InfrastructureRail
	PriceBaseSpec(10, PriceCategory::Running, GrfSpecFeature::End, Price::BuildRoad), // Price::InfrastructureRoad
	PriceBaseSpec(8, PriceCategory::Running, GrfSpecFeature::End, Price::BuildCanal), // Price::InfrastructureWater
	PriceBaseSpec(100, PriceCategory::Running, GrfSpecFeature::End, Price::StationValue), // Price::InfrastructureStation
	PriceBaseSpec(5000, PriceCategory::Running, GrfSpecFeature::End, Price::BuildStationAirport), // Price::InfrastructureAirport
};
