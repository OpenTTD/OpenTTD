/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file saveload.h Functions/types related to saving and loading games. */

#ifndef SAVELOAD_H
#define SAVELOAD_H

#include "saveload_type.h"
#include "saveload_error.hpp"
#include "saveload_func.h"
#include "../core/label_type.hpp"
#include "../fileio_type.h"
#include "../fios.h"

/** SaveLoad versions
 * Previous savegame versions, the trunk revision where they were
 * introduced and the released version that had that particular
 * savegame version.
 * Up to savegame version 18 there is a minor version as well.
 *
 * Older entries keep their original numbering.
 *
 * Newer entries should use a descriptive labels, numeric version
 * and PR can be added to comment.
 *
 * Note that this list must not be reordered.
 */
enum class SaveLoadVersion : uint16_t {
	MinVersion, ///< First savegame version.

	BigCurrency, ///< Saveload version: 1.0, releases: 0.1.x, 0.2.x\n Change currency from 32 to 64 bits.
	VehicleCurrencyStationChanges, ///< Saveload version: 2.0, release: 0.3.0\n Adding vehicle state, larger currency size for statistics, station size revamped.
	///< <p>Saveload version: 2.1, releases: 0.3.1, 0.3.2\n Unify way of storing town owner.
	///< <p>Saveload version: 2.2\n New airports.
	BiggerStationVariables, ///< Saveload version: 3.0\n Increase size of airport blocks/station build date.
	TownTolerancePauseMode, ///< Saveload version: 4.0, SVN revision: 1\n Town council tolerance and pause mode.
	///< <p>Saveload version: 4.1, SVN revision: 122, releases: 0.3.3, 0.3.4\n Store exclusive rights in towns.
	///< <p>Saveload version: 4.2, SVN revision: 1222, release: 0.3.5\n Currencies are reordered.
	///< <p>Saveload version: 4.3, SVN revision: 1417\n Make water owned by OWNER_NONE.
	///< <p>Saveload version: 4.4, SVN revision: 1426\n Make vehicle references same as other references.

	BigMap, ///< Saveload version: 5.0, SVN revision: 1429\n Making maps a different size than 256x256.
	///< <p>Saveload version: 5.1, SVN revision: 1440\n Flexible airport layouts.
	///< <p>Saveload version: 5.2, SVN revision: 1525, release: 0.3.6\n Dynamic order array.
	MultipleRoadStops, ///< Saveload version: 6.0, SVN revision: 1721\n Multi tile road stops, and some map size related fixes.
	///< <p>Saveload version: 6.1, SVN revision: 1768\n Town index in m2.
	LargerCargoSource, ///< Saveload version: 7.0, SVN revision: 1770\n With more stations, the size of the cargo source needed to be increased.
	LargerUnitNumber, ///< Saveload version: 8.0, SVN revision: 1786\n Increase size of (vehicle) unit numbers.
	LargerTownCargoStatistics, ///< Saveload version: 9.0, SVN revision: 1909\n Increase size of passenger/mail production of this and previous months.

	LargerTownCounter, ///< Saveload version: 10.0, SVN revision: 2030\n Increase size of the town counter.
	LargerTownIterator, ///< Saveload version: 11.0, SVN revision: 2033\n Increase size of the town iterator.
	///< <p>Saveload version: 11.1, SVN revision: 2041\n Fix vehicle counters.
	LinkWaypointToTown, ///< Saveload version: 12.1, SVN revision: 2046\n Link waypoints to towns and remove some bit stuffing.
	LargerAIStateCounter, ///< Saveload version: 13.1, SVN revision: 2080, releases: 0.4.0, 0.4.0.1\n AI state counter increased due it storing tile indices.
	TransferOrder, ///< Saveload version: 14.0, SVN revision: 2441\n Transfer orders for feeder systems.

	MoveSemaphoreBits, ///< Saveload version: 15.0, SVN revision: 2499\n Move rail signal bit for semaphores.
	EngineRenew, ///< Saveload version: 16.0, SVN revision: 2817\n Automatic replacing/renewing of vehicles.
	///< <p>Saveload version: 16.1, SVN revision: 3155\n Keep vehicle length during autoreplace.
	StoreWaypointIdInMap, ///< Saveload version: 17.0, SVN revision: 3212\n Store the ID of waypoints in m2 of the map.
	///< <p>Saveload version: 17.1, SVN revision: 3218\n Make train subtype a bitmask.
	RemoveMinorVersion, ///< Saveload version: 18, SVN revision: 3227\n Remove the minor versions from savegames.
	EngineRenewPool, ///< Saveload version: 19, SVN revision: 3396\n Engine renews are now stored in a pool.

	NoMultiheadReference, ///< Saveload version: 20, SVN revision: 3403\n Remove reference from one multihead to the other one.
	RemoveOldPbs, ///< Saveload version: 21, SVN revision: 3472, release: 0.4.x\n Remove old implementation of path based signals.
	SavePatches, ///< Saveload version: 22, SVN revision: 3726\n Save state of patches (precursor of settings) in the savegame.
	RemoveAutosaveInterval, ///< Saveload version: 23, SVN revision: 3915\n Store autosave interval locally, instead of in savegame.
	Elrail, ///< Saveload version: 24, SVN revision: 4150\n Electrified railways.

	ImproveMultistop, ///< Saveload version: 25, SVN revision: 4259\n Improve the behaviour of RVs going to road stops.
	LastVehicleType, ///< Saveload version: 26, SVN revision: 4466\n Store the last vehicle type at stations instead of the vehicle ID.
	NewGRFStations, ///< Saveload version: 27, SVN revision: 4757\n NewGRF graphics for stations.
	Yapf, ///< Saveload version: 28, SVN revision: 4987\n Yet another path finder.
	MoreUnderBridges, ///< Saveload version: 29, SVN revision: 5070\n Support crossings, fields and bridge/tunnel heads under bridges.

	Tgp, ///< Saveload version: 30, SVN revision: 5946\n TerraGenesis Perlin.
	BigDates, ///< Saveload version: 31, SVN revision: 5999\n Change date from 1920 - 2090 to 0 - 5 000 000.
	LinkFarmFieldToIndustry, ///< Saveload version: 32, SVN revision: 6001\n Link farm fields to the industry, so it gets removed when the industry goes away.
	SaveYapfSettings, ///< Saveload version: 33, SVN revision: 6440\n Some YAPF settings were not saved properly.
	Liveries, ///< Saveload version: 34, SVN revision: 6455\n Liveries and two company colours (2cc).

	LiveryRefit, ///< Saveload version: 35, SVN revision: 6602\n NewGRF livery refits.
	RefitOrders, ///< Saveload version: 36, SVN revision: 6624\n Vehicles can be refitted as part of an order.
	Utf8, ///< Saveload version: 37, SVN revision: 7182\n UTF-8 strings.
	DisableElrailSetting, ///< Saveload version: 38, SVN revision: 7195\n Add setting to disable electrified rails.
	FreightWeight, ///< Saveload version: 39, SVN revision: 7269\n Setting to increase the weight of cargo on freight trains.

	GradualLoading, ///< Saveload version: 40, SVN revision: 7326\n Gradual (un)loading of cargo.
	NewGRFSettings, ///< Saveload version: 41, SVN revision: 7348, release: 0.5.x\n Save what NewGRFs are used in the game and their settings.
	BridgeWormhole, ///< Saveload version: 42, SVN revision: 7573\n Bridges become wormholes, so more things can be built under them (e.g. signals).
	UnifyAnimationState, ///< Saveload version: 43, SVN revision: 7642\n Put all animation state information in same map bits.
	CargoSourceTile, ///< Saveload version: 44, SVN revision: 8144\n Store the source tile of the cargo, so accurate payment can happen when the source station is removed.

	CountPaidForCargo, ///< Saveload version: 45, SVN revision: 8501\n Count the amount of cargo that was paid for.
	MoreAirportBlocks, ///< Saveload version: 46, SVN revision: 8705\n Increase number of blocks an airport can have.
	DriveThroughRoadStops, ///< Saveload version: 47, SVN revision: 8735\n Drive through road stops.
	RailTrackTypeUnification, ///< Saveload version: 48, SVN revision: 8935\n Put all the rail track type information in same map bits.
	SimplifyPlayerFace, ///< Saveload version: 49, SVN revision: 8969\n Simplify the storage of player face information.

	AircraftSpeedHolding, ///< Saveload version: 50, SVN revision: 8973\n Aircraft speed in km-ish/h and reduced speed in holding patterns.
	FeederShare, ///< Saveload version: 51, SVN revision: 8978\n Rewrite of transfers to retain knowledge about the already paid amount for transfered cargo.
	StatueOwner, ///< Saveload version: 52, SVN revision: 9066\n Store the owner of the statue, so the town can be informed of their removal.
	NewGRFHouses, ///< Saveload version: 53, SVN revision: 9316\n NewGRF controlled houses.
	TownGrowthControl, ///< Saveload version: 54, SVN revision: 9613\n Give the player control over the town growth.

	NewGRFCargo, ///< Saveload version: 55, SVN revision: 9638\n Increase number of cargos and NewGRF control of cargos.
	Cities, ///< Saveload version: 56, SVN revision: 9667\n Cities that start bigger and grow faster.
	FifoLoading, ///< Saveload version: 57, SVN revision: 9691\n First-in-first-out loading of vehicles.
	VeryLowTownIndustryNumber, ///< Saveload version: 58, SVN revision: 9762\n Difficulty settings for very low number of industries and towns.
	TownLayout, ///< Saveload version: 59, SVN revision: 9779\n More layout options for towns.

	VehicleGroups, ///< Saveload version: 60, SVN revision: 9874\n Arbitrary grouping, by the player, of vehicles.
	MultipleRoadTypes, ///< Saveload version: 61, SVN revision: 9892\n Multiple road types for the same tile.
	AdjacentStations, ///< Saveload version: 62, SVN revision: 9905\n Allow building multiple stations directly next to eachother.
	TramLivery, ///< Saveload version: 63, SVN revision: 9956\n Add separate livery for trams.
	MultipleSignalTypes, ///< Saveload version: 64, SVN revision: 10006\n Multiple different signal types on the same (diagonal) tile, instead of the same for both directions.

	UnifyCurrency, ///< Saveload version: 65, SVN revision: 10210\n Make all variables related to currency 64 bits.
	NewGRFTownNames, ///< Saveload version: 66, SVN revision: 10211\n NewGRF provided town names.
	Timetables, ///< Saveload version: 67, SVN revision: 10236\n Introduce timetables for vehicles.
	CargoPackets, ///< Saveload version: 68, SVN revision: 10266\n Account for individual units of cargo, i.e. there can be cargo from multiple sources/ages in one vehicle.
	MoreCargoPackets, ///< Saveload version: 69, SVN revision: 10319\n Allow more than ~65k cargo packets.

	CargoPaymentOverflow, ///< Saveload version: 70, SVN revision: 10541\n Fix overflow of cargo payment rates, plus preparation for player founded industries.
	UngroupedVehicles, ///< Saveload version: 71, SVN revision: 10567\n Add a group with vehicles that aren't in any other group.
	SplitStationTypeFromGfxid, ///< Saveload version: 72, SVN revision: 10601\n Splits the encoding of station type from the graphics identifer.
	NewGRFIndustryLayout, ///< Saveload version: 73, SVN revision: 10903\n NewGRF provided layouts for industries.
	FixStationPickupAccounting, ///< Saveload version: 74, SVN revision: 11030\n Accounting of which cargos a station would pick up was done incorrectly.

	Autoslope, ///< Saveload version: 75, SVN revision: 11107\n Terraforming under buildings/track/anything that supports foundations.
	NewGRFPersistentStorage, ///< Saveload version: 76, SVN revision: 11139\n Persistently store some state of NewGRF objects/entities.
	CleanupUnconnectedRoads, ///< Saveload version: 77, SVN revision: 11172\n Option to remove unconnected roads during a town's road reconstruction.
	StoreIndustryCargo, ///< Saveload version: 78, SVN revision: 11176\n Store an industry's cargo, so it can be customised upon building.
	FairPlaySettings, ///< Saveload version: 79, SVN revision: 11188\n Add setting to disable exclusive rights in a town and giving money.

	NewGRFMoreAnimation, ///< Saveload version: 80, SVN revision: 11228\n Support more types of animation for NewGRF industries.
	FixTreeGround, ///< Saveload version: 81, SVN revision: 11244\n Various fixes to improve the visuals of the ground under trees.
	NewGRFIndustryRandomTriggers, ///< Saveload version: 82, SVN revision: 11410\n NewGRF random triggers for industries.
	DepotWaterOwners, ///< Saveload version: 83, SVN revision: 11589\n Store the owner of the water under depots, so removing of the depot doesn't disown the original owner.
	ReplaceCustomNameArray, ///< Saveload version: 84, SVN revision: 11822\n Replace single fixed size array of custom names, by moving the name into the appropriate objects.

	MaglevMonorailPaxWagonLivery, ///< Saveload version: 85, SVN revision: 11874\n Add livery for maglev/monorail passenger wagons.
	WaterClass, ///< Saveload version: 86, SVN revision: 12042\n Store the type of water (sea/ocean, canal, river) for buoys, docks, locks and depots.
	SimplifyPathfinderSettings, ///< Saveload version: 87, SVN revision: 12129\n Make it easier to select the pathfinder to use.
	FractionProfitRunningTicks, ///< Saveload version: 88, SVN revision: 12134\n Store vehicle profits as a (fixed point) fraction, and store the number of ticks a vehicle ran in a day.
	MoreWaypointsPerTown, ///< Saveload version: 89, SVN revision: 12160\n Support more than 64 waypoints per town.

	PlaneSpeedFactor, ///< Saveload version: 90, SVN revision: 12293\n Setting to increase aircraft speed to be on par with the other vehicles.
	MoreHouseAnimationFrames, ///< Saveload version: 91, SVN revision: 12347\n Increase number of animation frames for NewGRF houses.
	RemoveHouseCount, ///< Saveload version: 92, SVN revision: 12381, release 0.6.x\n Remove number of houses in a town from the save.
	ImprovedOrders, ///< Saveload version: 93, SVN revision: 12648\n Orders support all full load/non stop types at the same time now.
	FixCompanyCargoTypes, ///< Saveload version: 94, SVN revision: 12816\n The company's cargo types should have increased in since with NewGRFCargo.

	MoreEngineTypes, ///< Saveload version: 95, SVN revision: 12924\n Allow more than the original 255 engine types.
	AirportNoise, ///< Saveload version: 96, SVN revision: 13226\n Introduce noise for airports, to allow more than two airports per town.
	MergeOptsPats, ///< Saveload version: 97, SVN revision: 13256\n Merge the OPTS and PATS chunks, i.e. all settings in one chunk.
	Gamelog, ///< Saveload version: 98, SVN revision: 13375\n Logging of important actions/situations in the save.
	IndustryTileWaterClass, ///< Saveload version: 99, SVN revision: 13838\n Add water classes to industry tiles.

	Yapp, ///< Saveload version: 100, SVN revision: 13952\n New version of path based signals.
	NewGRFPalette, ///< Saveload version: 101, SVN revision: 14233\n Store palette used by each of the NewGRFs.
	SpreadIndustryProductionChanges, ///< Saveload version: 102, SVN revision: 14332\n Spread the industry production changes over the month, instead of doing all on the same day.
	NewGRFSuppliedStationName, ///< Saveload version: 103, SVN revision: 14598\n NewGRF industry supplying default names for nearby stations.
	MoreCompanies, ///< Saveload version: 104, SVN revision: 14735\n Increase maximum number of companies to 15.

	OrderList, ///< Saveload version: 105, SVN revision: 14803\n Create separate order list objects for maintaining orders.
	DistantStationJoining, ///< Saveload version: 106, SVN revision: 14919\n Distant joining of stations.
	NoAI, ///< Saveload version: 107, SVN revision: 15027\n Replace built in cheating AI with framework for externally developed (scripted) AIs.
	StoreAIVersion, ///< Saveload version: 108, SVN revision: 15045\n Store the version of the AI script.
	NextCompetitorStartOverflow, ///< Saveload version: 109, SVN revision: 15075\n Prevent overflow in the next competitor start counter.

	RemoveOldAISettings, ///< Saveload version: 110, SVN revision: 15148\n Remove remnants of the old AI's configuration.
	FreeformEdges, ///< Saveload version: 111, SVN revision: 15190\n Allow terraforming along the edge of the map.
	SplitHQ, ///< Saveload version: 112, SVN revision: 15290\n Split the behaviour of headquarters from the other unmovable objects.
	RoadLayoutPerTown, ///< Saveload version: 113, SVN revision: 15340\n Allow for different road layouts for each of the towns.
	SeparateRoadOwners, ///< Saveload version: 114, SVN revision: 15601\n Separate owners for road bits, tram bits and the road stop.

	CustomTownNumber, ///< Saveload version: 115, SVN revision: 15695\n Configuration for specific number of towns to build.
	GamelogEmergency, ///< Saveload version: 116, SVN revision: 15893, release: 0.7.x\n Gamelog event for emergency/crash saves.
	PlatformStopLocation, ///< Saveload version: 117, SVN revision: 16037\n Set the platform stop location via train orders.
	DigitGroupSeparator, ///< Saveload version: 118, SVN revision: 16129\n Configurable digit group separator.
	PauseModes, ///< Saveload version: 119, SVN revision: 16242\n Use bitmask of reason to pause, so manual/auto pausing do not conflict.

	CompanyServiceIntervals, ///< Saveload version: 120, SVN revision: 16439\n Make service intervals configurable per company.
	CargoPayments, ///< Saveload version: 121, SVN revision: 16694\n Perform payment of cargo after unloading.
	WaypointMoreLikeStation, ///< Saveload version: 122, SVN revision: 16855\n Make waypoint data look more like stations.
	UnifyWaypointAndStation, ///< Saveload version: 123, SVN revision: 16909\n Unify stations and waypoints.
	MultiTileWaypoints, ///< Saveload version: 124, SVN revision: 16993\n Waypoints can be bigger than a single tile.

	RemoveSubsidyStationBinding, ///< Saveload version: 125, SVN revision: 17113\n Awarded subsidies are not bound to stations, but to their actual source/destination.
	CumulatedInflation, ///< Saveload version: 126, SVN revision: 17433\n Store cumulated inflation, and recalculate prices/payments upon load.
	TownAcceptance, ///< Saveload version: 127, SVN revision: 17439\n Store mask of cargos accepted by town houses and head quarters.
	FoundTown, ///< Saveload version: 128, SVN revision: 18281\n Founding of new towns.
	TimetableStart, ///< Saveload version: 129, SVN revision: 18292\n Allow setting the start date of a timetable.

	RoadStopOccupancyPenalty, ///< Saveload version: 130, SVN revision: 18404\n Add configurable pathfinder penalty for an occupied road stop.
	MaximumDepotPenalty, ///< Saveload version: 131, SVN revision: 18481\n Add configurable maximum pathfinder penalty for finding a depot.
	DisallowTreeBuilding, ///< Saveload version: 132, SVN revision: 18522\n Setting to partially disable building of trees.
	TrainSlopeSteepness, ///< Saveload version: 133, SVN revision: 18674\n Setting to increase steepness of slopes for trains under realistic acceleration.
	VirtualFeederSharePayment, ///< Saveload version: 134, SVN revision: 18703\n Pay a part of the virtual profit during a transfer to the intermediate vehicle.

	RocksStayUnderSnow, ///< Saveload version: 135, SVN revision: 18719\n Rocks stay under snow, i.e. they return when the snow goes away.
	SplitLoadWaitCounters, ///< Saveload version: 136, SVN revision: 18764\n Split counters for (un)loading and signal waiting/turning as otherwise they interfere.
	AirportAnimationFrames, ///< Saveload version: 137, SVN revision: 18912\n Use animation frames instead of many airport tile ids for animation.
	ReducePlaneCrashes, ///< Saveload version: 138, SVN revision: 18942, release: 1.0.x\n Setting to reduce/disable crashing of planes.
	RvRealisticAcceleration, ///< Saveload version: 139, SVN revision: 19346\n Realistic acceleration of road vehicles.

	StoreAirportSize, ///< Saveload version: 140, SVN revision: 19382\n Store the size of the airport in the station.
	UniqueDepotNames, ///< Saveload version: 141, SVN revision: 19799\n Give depots unique names.
	NewGRFDepotBuildDate, ///< Saveload version: 142, SVN revision: 20003\n Depot build date for NewGRFs.
	DisableTownLevelCrossing, ///< Saveload version: 143, SVN revision: 20048\n Setting to be able to disable building rail/road crossings by towns.
	ReorderUnmovableRemoveReserved, ///< Saveload version: 144, SVN revision: 20334\n Reorder map bits of unmovable tiles and remove unused reserved zero bytes.

	NewGRFAirportSmoke, ///< Saveload version: 145, SVN revision: 20376\n NewGRF support for airport and configurable amount of smoke for vehicles.
	UnifyWaterClass, ///< Saveload version: 146, SVN revision: 20446\n Unify location for storing water class in the map.
	UnifyAnimationFrame, ///< Saveload version: 147, SVN revision: 20621\n Unify location of animation frame.
	IndustryPlatform, ///< Saveload version: 148, SVN revision: 20659\n Setting to make a flat area around (new) industries.
	CustomSeaLevel, ///< Saveload version: 149, SVN revision: 20832\n Setting to influence the sea level (amount of water).

	FractionalCargoDelivery, ///< Saveload version: 150, SVN revision: 20857\n When spreading cargo over stations, spread fractional amounts for fairness.
	StoreNewGRFVersion, ///< Saveload version: 151, SVN revision: 20918\n Store the version of the used NewGRFs.
	IndustryManagement, ///< Saveload version: 152, SVN revision: 21171\n Manage the amount of industries that ought to be spawned per type.
	LeaveRoadStopSeparately, ///< Saveload version: 153, SVN revision: 21263\n Fix issue where multiple vehicles could leave a road stop at the same time.
	PauseLevel, ///< Saveload version: 154, SVN revision: 21426\n Setting to determine what commands are allowed when paused.

	NewGRFObjectView, ///< Saveload version: 155, SVN revision: 21453\n Support for views in NewGRF objects.
	TerraformLimits, ///< Saveload version: 156, SVN revision: 21728\n Introduce limits for terraforming and clearing times.
	UnifyGroundVehicles, ///< Saveload version: 157, SVN revision: 21862\n Unify the way ground vehicles are handled (articulated parts, etc).
	TrackRealAndAutoOrders, ///< Saveload version: 158, SVN revision: 21933\n Track which real and auto order is the current order.
	MaxLengthAndReverseSignals, ///< Saveload version: 159, SVN revision: 21962\n Settings for reversing at signals, and maximum train, bridge and tunnel length.

	DisallowRoadReconstruction, ///< Saveload version: 160, SVN revision: 21974, release: 1.1.x\n Setting to disallow road reconstruction.
	PersistentStoragePool, ///< Saveload version: 161, SVN revision: 22567\n Store persistent storage in a pool.
	NewGRFCustomCargoAging, ///< Saveload version: 162, SVN revision: 22713\n NewGRF influence on aging of cargo in vehicles.
	Rivers, ///< Saveload version: 163, SVN revision: 22767\n Rivers.
	VehicleCentreAndZPos, ///< Saveload version: 164, SVN revision: 23290\n Vehicle centres are not fixed at 4/8 of the vehicle; change type of z-positions to prepare for higher maps.

	ScriptTownGrowth, ///< Saveload version: 165, SVN revision: 23304\n Storage of cargo statistics for use by game scripts.
	InfrastructureMaintenanceCosts, ///< Saveload version: 166, SVN revision: 23415\n Infrastructure can now cost some periodic fee.
	NewGRFAircraftRange, ///< Saveload version: 167, SVN revision: 23504\n NewGRF provided maximum aircraft range.
	ScriptTownText, ///< Saveload version: 168, SVN revision: 23637\n Game scripts can put a text in the town window.
	MoveSccEncoded, ///< Saveload version: 169, SVN revision: 23816\n Move SCC_ENCODED to the first StringControlCode.

	CountIndividualCargoes, ///< Saveload version: 170, SVN revision: 23826\n Store the count of individual cargo delivery for a period.
	ScenarioDeitySigns, ///< Saveload version: 171, SVN revision: 23835\n Signs made in scenarios become of OWNER_DEITY, so they are always shown.
	OrderMaxSpeed, ///< Saveload version: 172, SVN revision: 23947\n Set maximum speed for orders.
	FixRoadOwnership, ///< Saveload version: 173, SVN revision: 23967, release: 1.2.0-RC1\n Seemingly unneeded bump supposed to fix something with road ownership.
	CurrentOrderMaxSpeed, ///< Saveload version: 174, SVN revision: 23973, release: 1.2.x\n Save maximum speed of current order.

	AutoreplaceWhenOldTreeLimit, ///< Saveload version: 175, SVN revision: 24136\n Autoreplace vehicle only when they are old, and putting limit on amount of trees to build (at once).
	BackupOrderState, ///< Saveload version: 176, SVN revision: 24446\n Put more of the state of a vehicle's orders (like lateness, start point) in the order backup.
	MonthlyBankruptcyCheck, ///< Saveload version: 177, SVN revision: 24619\n Check for bankruptcy on a monthly cycle.
	ScriptSettingsProfile, ///< Saveload version: 178, SVN revision: 24789\n Setting for the difficulty profile of AIs.
	RobustEnginePreview, ///< Saveload version: 179, SVN revision: 24810\n Make engine preview offers robust when company ranking changes.

	ServiceIntervalPercent, ///< Saveload version: 180, SVN revision: 24998, release: 1.3.x\n Service interval in percent or days stored per vehicle.
	CargoReservation, ///< Saveload version: 181, SVN revision: 25012\n Persist the reservation of cargo for vehicles instead of recalculating it each time.
	GoalProgressPlaneAcceleration, ///< Saveload version: 182, SVN revision: 25115, r25259, r25296\n Goal status and plane acceleration fixes.
	Cargodist, ///< Saveload version: 183, SVN revision: 25363\n Cargodist.
	SeparateLocaleUnits, ///< Saveload version: 184, SVN revision: 25508\n Unit localisation split.

	Storybooks, ///< Saveload version: 185, SVN revision: 25620\n Storybooks.
	ObjectTypeToPool, ///< Saveload version: 186, SVN revision: 25833\n Move object type from map to pool object.
	LinkgraphRestrictedFlow, ///< Saveload version: 187, SVN revision: 25899\n Linkgraph - restricted flows.
	UnifyRvTravelTime, ///< Saveload version: 188, SVN revision: 26169, release: 1.4\n Unify RV travel time.
	GroupHierarchy, ///< Saveload version: 189, SVN revision: 26450\n Hierarchical vehicle subgroups.

	SeparateOrderTravelWaitTime, ///< Saveload version: 190, SVN revision: 26547\n Separate order travel and wait times.
	LinkgraphLocationDisasterStore, ///< Saveload version: 191, SVN revision: 26636, 26646\n Fix disaster vehicle storage, Linkgraph - store locations.
	FixOrderBackup, ///< Saveload version: 192, SVN revision: 26700\n Fix saving of order backups.
	HideEnginesForCompany, ///< Saveload version: 193, SVN revision: 26802\n Hiding of engines for a company.
	MaxBridgeMapHeight, ///< Saveload version: 194, SVN revision: 26881, release: 1.5\n Setting for maximum bridge and map height.

	Distinguish16, ///< Saveload version: 195, SVN revision: 27572, release: 1.6.1\n Convenience bump to distinguish 1.6 from 1.5 saves.
	Distinguish17, ///< Saveload version: 196, SVN revision: 27778, release: 1.7\n Convenience bump to distinguish 1.7 from 1.6 saves.
	StoreMapVariety, ///< Saveload version: 197, SVN revision: 27978, release: 1.8\n Store map variety.
	TownGrowthInGameTicks, ///< Saveload version: 198, GitHub pull request: 6763\n Switch town growth rate and counter to actual game ticks.
	ExtendCargotypes, ///< Saveload version: 199, GitHub pull request: 6802\n Extend cargotypes to 64.

	ExtendRailtypes, ///< Saveload version: 200, GitHub pull request: 6805\n Extend railtypes to 64, adding uint16_t to map array.
	ExtendPersistentStorage, ///< Saveload version: 201, GitHub pull request: 6885\n Extend NewGRF persistent storages.
	ExtendIndustryCargoSlots, ///< Saveload version: 202, GitHub pull request: 6867\n Increase industry cargo slots to 16 in, 16 out.
	ShipPathCache, ///< Saveload version: 203, GitHub pull request: 7072\n Add path cache for ships.
	ShipRotation, ///< Saveload version: 204, GitHub pull request: 7065\n Add extra rotation stages for ships.

	GroupLiveries, ///< Saveload version: 205, GitHub pull request: 7108\n Livery storage change and group liveries.
	ShipsStopInLocks, ///< Saveload version: 206, GitHub pull request: 7150\n Ship/lock movement changes.
	FixCargoMonitor, ///< Saveload version: 207, GitHub pull request: 7175, release: 1.9\n Cargo monitor data packing fix to support 64 cargotypes.
	TownCargogen, ///< Saveload version: 208, GitHub pull request: 6965\n New algorithms for town building cargo generation.
	ShipCurvePenalty, ///< Saveload version: 209, GitHub pull request: 7289\n Configurable ship curve penalties.

	ServeNeutralIndustries, ///< Saveload version: 210, GitHub pull request: 7234\n Company stations can serve industries with attached neutral stations.
	RoadvehPathCache, ///< Saveload version: 211, GitHub pull request: 7261\n Add path cache for road vehicles.
	RemoveOldPathfinder, ///< Saveload version: 212, GitHub pull request: 7245\n Remove OPF.
	TreesWaterClass, ///< Saveload version: 213, GitHub pull request: 7405\n WaterClass update for tree tiles.
	RoadTypes, ///< Saveload version: 214, GitHub pull request: 6811\n NewGRF road types.

	ScriptMemlimit, ///< Saveload version: 215, GitHub pull request: 7516\n Limit on AI/GS memory consumption.
	MultitileDocks, ///< Saveload version: 216, GitHub pull request: 7380\n Multiple docks per station.
	TradingAge, ///< Saveload version: 217, GitHub pull request: 7780\n Configurable company trading age.
	EndingYear, ///< Saveload version: 218, GitHub pull request: 7747, release: 1.10\n Configurable ending year.
	RemoveTownCargoCache, ///< Saveload version: 219, GitHub pull request: 8258\n Remove town cargo acceptance and production caches.

	/* Patchpacks for a while considered it a good idea to jump a few versions
	 * above our version for their savegames. But as time continued, this gap
	 * has been closing, up to the point we would start to reuse versions from
	 * their patchpacks. This is not a problem from our perspective: the
	 * savegame will simply fail to load because they all contain chunks we
	 * cannot digest. But, this gives for ugly errors. As we have plenty of
	 * versions anyway, we simply skip the versions we know belong to
	 * patchpacks. This way we can present the user with a clean error
	 * indicate they are loading a savegame from a patchpack.
	 * For future patchpack creators: please follow a system like JGRPP, where
	 * the version is masked with 0x8000, and the true version is stored in
	 * its own chunk with feature toggles.
	 */
	StartPatchpacks, ///< Saveload version: 220\n First known patchpack to use a version just above ours.
	EndPatchpacks = 286, ///< Saveload version: 286\n Last known patchpack to use a version just above ours.

	GSIndustryControl, ///< Saveload version: 287, GitHub pull request: 7912 and 8115\n GS industry control.
	VehMotionCounter, ///< Saveload version: 288, GitHub pull request: 8591\n Desync safe motion counter.
	IndustryText, ///< Saveload version: 289, GitHub pull request: 8576, release: 1.11.0-RC1\n Additional GS text for industries.

	MapgenSettingsRevamp, ///< Saveload version: 290, GitHub pull request: 8891, release: 1.11\n Revamp of some mapgen settings (snow coverage, desert coverage, heightmap height, custom terrain type).
	GroupReplaceWagonRemoval, ///< Saveload version: 291, GitHub pull request: 7441\n Per-group wagon removal flag.
	CustomSubsidyDuration, ///< Saveload version: 292, GitHub pull request: 9081\n Configurable subsidy duration.
	SaveloadListLength, ///< Saveload version: 293, GitHub pull request: 9374\n Consistency in list length with SaveLoadType::Struct / SaveLoadType::StructList / SaveLoadType::ReferenceList.
	RiffToArray, ///< Saveload version: 294, GitHub pull request: 9375\n Changed many ChunkType::Riff chunks to ChunkType::Array chunks.

	TableChunks, ///< Saveload version: 295, GitHub pull request: 9322\n Introduction of ChunkType::Table and ChunkType::SparseTable.
	ScriptInt64, ///< Saveload version: 296, GitHub pull request: 9415\n SQInteger is 64bit but was saved as 32bit.
	LinkgraphTravelTime, ///< Saveload version: 297, GitHub pull request: 9457, release: 12.0-RC1\n Store travel time in the linkgraph.
	DockDockingtiles, ///< Saveload version: 298, GitHub pull request: 9578\n All tiles around docks may be docking tiles.
	RepairObjectDockingTiles, ///< Saveload version: 299, GitHub pull request: 9594, release: 12.0\n Fixing issue with docking tiles overlapping objects.

	U64TickCounter, ///< Saveload version: 300, GitHub pull request: 10035\n Make tick counter 64bit to avoid wrapping.
	LastLoadingTick, ///< Saveload version: 301, GitHub pull request: 9693\n Store tick of last loading for vehicles.
	MultitrackLevelCrossings, ///< Saveload version: 302, GitHub pull request: 9931, release: 13.0\n Multi-track level crossings.
	NewGRFRoadStops, ///< Saveload version: 303, GitHub pull request: 10144\n NewGRF road stops.
	LinkgraphEdges, ///< Saveload version: 304, GitHub pull request: 10314\n Explicitly store link graph edges destination, PR#10471 int64_t instead of uint64_t league rating.

	VelocityNautical, ///< Saveload version: 305, GitHub pull request: 10594\n Separation of land and nautical velocity (knots!).
	ConsistentPartialZ, ///< Saveload version: 306, GitHub pull request: 10570\n Conversion from an inconsistent partial Z calculation for slopes, to one that is (more) consistent.
	MoreCargoAge, ///< Saveload version: 307, GitHub pull request: 10596\n Track cargo age for a longer period.
	LinkgraphSeconds, ///< Saveload version: 308, GitHub pull request: 10610\n Store linkgraph update intervals in seconds instead of days.
	AIStartDate, ///< Saveload version: 309, GitHub pull request: 10653\n Removal of individual AI start dates and added a generic one.

	ExtendVehicleRandom, ///< Saveload version: 310, GitHub pull request: 10701\n Extend vehicle random bits.
	ExtendEntityMapping, ///< Saveload version: 311, GitHub pull request: 10672\n Extend entity mapping range.
	DisasterVehState, ///< Saveload version: 312, GitHub pull request: 10798\n Explicit storage of disaster vehicle state.
	SavegameId, ///< Saveload version: 313, GitHub pull request: 10719\n Add an unique ID to every savegame (used to deduplicate surveys).
	StringGamelog, ///< Saveload version: 314, GitHub pull request: 10801\n Use std::string in gamelog.

	IndustryCargoReorganise, ///< Saveload version: 315, GitHub pull request: 10853\n Industry accepts/produced data reorganised.
	PeriodsInTransitRename, ///< Saveload version: 316, GitHub pull request: 11112\n Rename days in transit to (cargo) periods in transit.
	NewGRFLastService, ///< Saveload version: 317, GitHub pull request: 11124\n Added stable date_of_last_service to avoid NewGRF trouble.
	RemoveLoadedAtXY, ///< Saveload version: 318, GitHub pull request: 11276\n Remove loaded_at_xy variable from CargoPacket.
	CargoTravelled, ///< Saveload version: 319, GitHub pull request: 11283\n CargoPacket now tracks how far it travelled inside a vehicle.

	StationRatingCheat, ///< Saveload version: 320, GitHub pull request: 11346\n Add cheat to fix station ratings at 100%.
	TimetableStartTicks, ///< Saveload version: 321, GitHub pull request: 11468\n Convert timetable start from a date to ticks.
	TimetableStartTicksFix, ///< Saveload version: 322, GitHub pull request: 11557\n Fix for missing convert timetable start from a date to ticks.
	TimetableTicksType, ///< Saveload version: 323, GitHub pull request: 11435\n Convert timetable current order time to ticks.
	WaterRegions, ///< Saveload version: 324, GitHub pull request: 10543\n Water Regions for ship pathfinder.

	WaterRegionEvalSimplified, ///< Saveload version: 325, GitHub pull request: 11750\n Simplified Water Region evaluation.
	EconomyDate, ///< Saveload version: 326, GitHub pull request: 10700\n Split calendar and economy timers and dates.
	EconomyModeTimekeepingUnits, ///< Saveload version: 327, GitHub pull request: 11341\n Mode to display economy measurements in wallclock units.
	CalendarSubDateFract, ///< Saveload version: 328, GitHub pull request: 11428\n Add sub_date_fract to measure calendar days.
	ShipAcceleration, ///< Saveload version: 329, GitHub pull request: 10734\n Start using Vehicle's acceleration field for ships too.

	MaxLoanForCompany, ///< Saveload version: 330, GitHub pull request: 11224\n Separate max loan for each company.
	DepotUnbunching, ///< Saveload version: 331, GitHub pull request: 11945\n Allow unbunching shared order vehicles at a depot.
	AILocalConfig, ///< Saveload version: 332, GitHub pull request: 12003\n Config of running AI is stored inside Company.
	ScriptRandomizer, ///< Saveload version: 333, GitHub pull request: 12063, release: 14.0-RC1\n Save script randomizers.
	VehicleEconomyAge, ///< Saveload version: 334, GitHub pull request: 12141, release: 14.0\n Add vehicle age in economy year, for profit stats minimum age.

	CompanyAllowList, ///< Saveload version: 335, GitHub pull request: 12337\n Saving of list of client keys that are allowed to join this company.
	GroupNumbers, ///< Saveload version: 336, GitHub pull request: 12297\n Add per-company group numbers.
	IncreaseStationTypeFieldSize, ///< Saveload version: 337, GitHub pull request: 12572\n Increase size of StationType field in map array.
	RoadWaypoints, ///< Saveload version: 338, GitHub pull request: 12572\n Road waypoints.
	CompanyInauguratedPeriod, ///< Saveload version: 339, GitHub pull request: 12798\n Companies show the period inaugurated in wallclock mode.

	RoadStopTileData, ///< Saveload version: 340, GitHub pull request: 12883\n Move storage of road stop tile data, also save for road waypoints.
	CompanyAllowListV2, ///< Saveload version: 341, GitHub pull request: 12908\n Fixed savegame format for saving of list of client keys that are allowed to join this company.
	WaterTileType, ///< Saveload version: 342, GitHub pull request: 13030\n Simplify water tile type.
	ProductionHistory, ///< Saveload version: 343, GitHub pull request: 10541\n Industry production history.
	RoadTypeLabelMap, ///< Saveload version: 344, GitHub pull request: 13021\n Add road type label map to allow upgrade/conversion of road types.

	NonfloodingWaterTiles, ///< Saveload version: 345, GitHub pull request: 13013\n Store water tile non-flooding state.
	PathCacheFormat, ///< Saveload version: 346, GitHub pull request: 12345\n Vehicle path cache format changed.
	AnimatedTileStateInMap, ///< Saveload version: 347, GitHub pull request: 13082\n Animated tile state saved for improved performance.
	IncreaseHouseLimit, ///< Saveload version: 348, GitHub pull request: 12288\n Increase house limit to 4096.
	CompanyInauguratedPeriodV2, ///< Saveload version: 349, GitHub pull request: 13448\n Fix savegame storage for company inaugurated year in wallclock mode.

	EncodedStringFormat, ///< Saveload version: 350, GitHub pull request: 13499\n Encoded String format changed.
	ProtectPlacedHouses, ///< Saveload version: 351, GitHub pull request: 13270\n Houses individually placed by players can be protected from town/AI removal.
	ScriptSaveInstances, ///< Saveload version: 352, GitHub pull request: 13556\n Scripts are allowed to save instances.
	FixSccEncodedNegative, ///< Saveload version: 353, GitHub pull request: 14049\n Fix encoding of negative parameters.
	OrdersOwnedByOrderlist, ///< Saveload version: 354, GitHub pull request: 13948\n Orders stored in OrderList, pool removed.

	FaceStyles, ///< Saveload version: 355, GitHub pull request: 14319\n Addition of face styles, replacing gender and ethnicity.
	IndustryNumValidHistory, ///< Saveload version: 356, GitHub pull request: 14416\n Store number of valid history records for industries.
	IndustryAcceptedHistory, ///< Saveload version: 357, GitHub pull request: 14321\n Add per-industry history of cargo delivered and waiting.
	TownSupplyHistory, ///< Saveload version: 358, GitHub pull request: 14461\n Town supply history.
	StationsUnderBridges, ///< Saveload version: 359, GitHub pull request: 14477\n Allow stations under bridges.

	DocksUnderBridges, ///< Saveload version: 360, GitHub pull request: 14594\n Allow docks under bridges.
	LocksUnderBridges, ///< Saveload version: 361, GitHub pull request: 14595\n Allow locks under bridges.
	EngineMultiRailtype, ///< Saveload version: 362, GitHub pull request: 14357, release: 15.0\n Train engines can have multiple railtypes.
	SignTextColours, ///< Saveload version: 363, GitHub pull request: 14743\n Configurable sign text colors in scenario editor.
	BuoysAt0_0, ///< Saveload version: 364, GitHub pull request: 14983\n Allow to build buoys at (0x0).

	DriveBackwards, ///< Saveload version: 365, GitHub pull request: 15379\n Trains can drive backwards.
	DepotsUnderBridges, ///< Saveload version: 366, GitHub pull request: 15836\n Allow depots under bridges.

	MaxVersion, ///< Highest possible saveload version.
};

/** Types of save games. */
enum SavegameType : uint8_t {
	TTD, ///< TTD savegame (can be detected incorrectly)
	TTDP1, ///< TTDP savegame ( -//- ) (data at NW border)
	TTDP2, ///< TTDP savegame in new format (data at SE border)
	OTTD, ///< OTTD savegame
	TTO, ///< TTO savegame
	Invalid = 0xFF, ///< broken savegame (used internally)
};
void SetSaveLoadError(StringID str);

typedef void AutolengthProc(int);

/** Type of a chunk. */
enum class ChunkType : uint8_t {
	Riff = 0, ///< 4 bits store the chunk type, 28 bits the number of bytes.
	Array = 1, ///< Contiguous array of elements starting at index 0.
	SparseArray = 2, ///< Array of elements with index for each element.
	Table = 3, ///< An \c Array with a header describing the elements.
	SparseTable = 4, ///< A \c SparseArray with a header describing the elements.

	FileTypeMask = 0xf, ///< All ChunkType values that are saved in the file have to be within this mask.
	ReadOnly, ///< Chunk is never saved.
};

/** Label/unique identifier for each of the chunks in the savegame. */
using ChunkId = Label<struct ChunkIdTag>;;

/** Handlers and description of chunk. */
struct ChunkHandler {
	ChunkId id; ///< Unique ID (4 letters).
	ChunkType type; ///< Type of the chunk. @see ChunkType

	/**
	 * Create this ChunkHandler.
	 * @param id The unique identifier/name of this chunk.
	 * @param type The type of chunk
	 */
	ChunkHandler(ChunkId id, ChunkType type) : id(id), type(type) {}

	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~ChunkHandler() = default;

	/**
	 * Save the chunk.
	 * Must be overridden, unless Chunk type is ChunkType::ReadOnly.
	 */
	virtual void Save() const { NOT_REACHED(); }

	/**
	 * Load the chunk.
	 * Must be overridden.
	 */
	virtual void Load() const = 0;

	/**
	 * Fix the pointers.
	 * Pointers are saved using the index of the pointed object.
	 * On load, pointers are filled with indices and need to be fixed to point to the real object.
	 * Must be overridden if the chunk saves any pointer.
	 */
	virtual void FixPointers() const {}

	/**
	 * Load the chunk for game preview.
	 * Default implementation just skips the data.
	 * @param len Number of bytes to skip.
	 */
	virtual void LoadCheck(size_t len = 0) const;

	/**
	 * Get the name of this chunk.
	 * @return The chunks 4 letter name/unique identifier.
	 */
	std::string GetName() const
	{
		return this->id.AsString();
	}
};

/** A reference to ChunkHandler. */
using ChunkHandlerRef = std::reference_wrapper<const ChunkHandler>;

/** A table of ChunkHandler entries. */
using ChunkHandlerTable = std::span<const ChunkHandlerRef>;

/** A table of SaveLoadCompat entries. */
using SaveLoadCompatTable = std::span<const struct SaveLoadCompat>;

/** Handler for saving/loading an object to/from disk. */
class SaveLoadHandler {
public:
	std::optional<std::vector<SaveLoad>> load_description; ///< Description derived from savegame being loaded.

	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~SaveLoadHandler() = default;

	/**
	 * Save the object to disk.
	 * @param object The object to store.
	 */
	virtual void Save([[maybe_unused]] void *object) const {}

	/**
	 * Load the object from disk.
	 * @param object The object to load.
	 */
	virtual void Load([[maybe_unused]] void *object) const {}

	/**
	 * Similar to load, but used only to validate savegames.
	 * @param object The object to load.
	 */
	virtual void LoadCheck([[maybe_unused]] void *object) const {}

	/**
	 * A post-load callback to fix #SaveLoadType::Reference integers into pointers.
	 * @param object The object to fix.
	 */
	virtual void FixPointers([[maybe_unused]] void *object) const {}

	/**
	 * Get the description of the fields in the savegame.
	 * @return Save load description.
	 */
	virtual SaveLoadTable GetDescription() const = 0;

	/**
	 * Get the pre-header description of the fields in the savegame.
	 * @return Compatibility save load description.
	 */
	virtual SaveLoadCompatTable GetCompatDescription() const = 0;

	/**
	 * Get the description for how to load the chunk. Depending on the
	 * savegame version this can either use the headers in the savegame or
	 * fall back to backwards compatibility and uses hard-coded headers.
	 * @return The description to load the complete chunk.
	 */
	SaveLoadTable GetLoadDescription() const;
};

/**
 * Default handler for saving/loading an object to/from disk.
 *
 * This handles a few common things for handlers, meaning the actual handler
 * needs less code.
 *
 * Usage: class SlMine : public DefaultSaveLoadHandler<SlMine, MyObject> {}
 *
 * @tparam TImpl The class initializing this template.
 * @tparam TObject The class of the object using this SaveLoadHandler.
 */
template <class TImpl, class TObject>
class DefaultSaveLoadHandler : public SaveLoadHandler {
public:
	SaveLoadTable GetDescription() const override { return static_cast<const TImpl *>(this)->description; }

	SaveLoadCompatTable GetCompatDescription() const override { return static_cast<const TImpl *>(this)->compat_description; }

	/**
	 * Save the object to disk.
	 * @param object The object to store.
	 */
	virtual void Save([[maybe_unused]] TObject *object) const {}
	void Save(void *object) const override { this->Save(static_cast<TObject *>(object)); }

	/**
	 * Load the object from disk.
	 * @param object The object to load.
	 */
	virtual void Load([[maybe_unused]] TObject *object) const {}
	void Load(void *object) const override { this->Load(static_cast<TObject *>(object)); }

	/**
	 * Similar to load, but used only to validate savegames.
	 * @param object The object to load.
	 */
	virtual void LoadCheck([[maybe_unused]] TObject *object) const {}
	void LoadCheck(void *object) const override { this->LoadCheck(static_cast<TObject *>(object)); }

	/**
	 * A post-load callback to fix #SaveLoadType::Reference integers into pointers.
	 * @param object The object to fix.
	 */
	virtual void FixPointers([[maybe_unused]] TObject *object) const {}
	void FixPointers(void *object) const override { this->FixPointers(static_cast<TObject *>(object)); }
};

/** Type of reference (#SLE_REF, #SLE_CONDREF). */
enum class SLRefType : uint8_t {
	Vehicle = 1, ///< Load/save a reference to a vehicle.
	Station = 2, ///< Load/save a reference to a station.
	Town = 3, ///< Load/save a reference to a town.
	OldVehicle = 4, ///< Load/save an old-style reference to a vehicle (for pre-4.4 savegames).
	RoadStop = 5, ///< Load/save a reference to a bus/truck stop.
	EngineRenew = 6, ///< Load/save a reference to an engine renewal (autoreplace).
	CargoPacket = 7, ///< Load/save a reference to a cargo packet.
	OrderList = 8, ///< Load/save a reference to an orderlist.
	Storage = 9, ///< Load/save a reference to a persistent storage.
	LinkGraph = 10, ///< Load/save a reference to a link graph.
	LinkGraphJob = 11, ///< Load/save a reference to a link graph job.
};

/** The types/structures of data that can be stored in the file. */
enum class VarFileType : uint8_t {
	/* 4 bits allocated a maximum of 16 types for NumberType.
	 * NOTE: the SLE_FILE_NNN values are stored in the savegame! */
	/* Value 0 is used to mark end-of-header in tables. Do not use here! */
	I8 = 1, ///< A 8 bit signed int.
	U8 = 2, ///< A 8 bit unsigned int.
	I16 = 3, ///< A 16 bit signed int.
	U16 = 4, ///< A 16 bit unsigned int.
	I32 = 5, ///< A 32 bit signed int.
	U32 = 6, ///< A 32 bit unsigned int.
	I64 = 7, ///< A 64 bit signed int.
	U64 = 8, ///< A 64 bit unsigned int.
	StringID = 9, ///< StringID offset into strings-array.
	String = 10, ///< A string.
	Struct = 11, ///< An arbitrary structure.
	/* 4 more possible file-primitives */
};

/** The types/structures of data we have in memory. */
enum class VarMemType : uint8_t {
	Bool = 0, ///< A boolean value.
	I8 = 1, ///< A 8 bit signed int.
	U8 = 2, ///< A 8 bit unsigned int.
	I16 = 3, ///< A 16 bit signed int.
	U16 = 4, ///< A 16 bit unsigned int.
	I32 = 5, ///< A 32 bit signed int.
	U32 = 6, ///< A 32 bit unsigned int.
	I64 = 7, ///< A 64 bit signed int.
	U64 = 8, ///< A 64 bit unsigned int.
	Null = 9, ///< useful to write zeros in savegame.
	Str = 12, ///< string pointer
	StrQ = 13, ///< string pointer enclosed in quotes
	Name = 14, ///< old custom name to be converted to a string pointer
	LabelReverse = 15, ///< A 4 character \c Label, stored in reverse.
	LabelForward = 16, ///< A 4 character \c Label, stored as-is.
};

/** Container of a variable's characteristics about a variable's storage. */
struct VarType {
	VarFileType file{}; ///< The way of storing data in the file.
	VarMemType mem{}; ///< The way of storing data in memory.
	StringValidationSettings string_validation_settings{}; ///< Any settings related to validation of the strings.
	SLRefType ref{}; ///< The reference type.

	/** Create an empty \c VarType. */
	constexpr VarType() {}

	/**
	 * Create a \c VarType with the given file and memory configurations.
	 * @param file The file storage configuration.
	 * @param mem The memory storage configuration.
	 */
	constexpr VarType(VarFileType file, VarMemType mem) : file(file), mem(mem) {}

	/**
	 * Create a `VarType` linking to a reference.
	 * @param ref The reference.
	 */
	constexpr VarType(SLRefType ref) : ref(ref) {}

	/**
	 * Equality operator.
	 * @param other The element to compare to.
	 * @return \c true iff all elements of this and other are the same.
	 */
	constexpr bool operator==(const VarType &other) const = default;

	/**
	 * Transitional helper function to add a \c SaveLoadFlag to this type.
	 * @param string_validation_setting The string_validation_setting to set.
	 * @return A copy of this with the string_validation_setting set.
	 */
	constexpr VarType operator|(StringValidationSetting string_validation_setting) const
	{
		VarType copy = *this;
		copy.string_validation_settings.Set(string_validation_setting);
		return copy;
	}
};

/**
 * Transitional helper function to combine a file and memory storage configuration.
 * @param file The file configuration.
 * @param mem The memory configuration.
 * @return The created \c VarType.
 */
constexpr VarType operator|(VarFileType file, VarMemType mem)
{
	return {file, mem};
}

/** Container for holding some default \c VarType instances. */
struct VarTypes {
	static constexpr VarType BOOL{ VarFileType::I8, VarMemType::Bool }; ///< Store a boolean (as int8).
	static constexpr VarType I8{ VarFileType::I8, VarMemType::I8 }; ///< Store a 8 bits signed int.
	static constexpr VarType U8{ VarFileType::U8, VarMemType::U8 }; ///< Store a 8 bits unsigned int.
	static constexpr VarType I16{ VarFileType::I16, VarMemType::I16 }; ///< Store a 16 bits signed int.
	static constexpr VarType U16{ VarFileType::U16, VarMemType::U16 }; ///< Store a 16 bits unsigned int.
	static constexpr VarType I32{ VarFileType::I32, VarMemType::I32 }; ///< Store a 32 bits signed int.
	static constexpr VarType U32{ VarFileType::U32, VarMemType::U32 }; ///< Store a 32 bits unsigned int.
	static constexpr VarType I64{ VarFileType::I64, VarMemType::I64 }; ///< Store a 64 bits signed int.
	static constexpr VarType U64{ VarFileType::U64, VarMemType::U64 }; ///< Store a 64 bits unsigned int.
	static constexpr VarType STRINGID{ VarFileType::StringID, VarMemType::U32 }; ///< Store a StringID.
	static constexpr VarType STR{ VarFileType::String, VarMemType::Str }; ///< Store string.
	static constexpr VarType STRQ{ VarFileType::String, VarMemType::StrQ }; ///< Store a string with quotes.
	static constexpr VarType NAME{ VarFileType::StringID, VarMemType::Name }; ///< A string stored in the custom string array.
	static constexpr VarType LABEL_REVERSE{ VarFileType::U32, VarMemType::LabelReverse }; ///< Store a \c Label in reverse.
	static constexpr VarType LABEL_FORWARD{ VarFileType::U32, VarMemType::LabelForward }; ///< Store a \c Label as-is.
};

/** Type of data saved. */
enum class SaveLoadType : uint8_t {
	Variable = 0, ///< Save/load a variable.
	Reference = 1, ///< Save/load a reference.
	Struct = 2, ///< Save/load a struct.

	String = 4, ///< Save/load a \c std::string.

	Array = 5, ///< Save/load a fixed-size array of #SaveLoadType::Variable elements.
	Vector = 7, ///< Save/load a vector of #SaveLoadType::Variable elements.
	ReferenceList = 8, ///< Save/load a list of #SaveLoadType::Reference elements.
	StructList = 9, ///< Save/load a list of structs.

	SaveByte = 10, ///< Save (but not load) a byte.
	Null = 11, ///< Save null-bytes and load to nowhere.

	ReferenceVector = 12, ///< Save/load a vector of #SaveLoadType::Reference elements.
};

typedef void *SaveLoadAddrProc(void *base, size_t extra);

/** SaveLoad type struct. Do NOT use this directly but use the SLE_ macros defined just below! */
struct SaveLoad {
	std::string name;    ///< Name of this field (optional, used for tables).
	SaveLoadType cmd;    ///< The action to take with the saved/loaded type, All types need different action.
	VarType conv;        ///< Type of the variable to be saved; this field combines both FileVarType and MemVarType.
	uint16_t length;       ///< (Conditional) length of the variable (eg. arrays) (max array size is 65536 elements).
	SaveLoadVersion version_from;   ///< Save/load the variable starting from this savegame version.
	SaveLoadVersion version_to;     ///< Save/load the variable before this savegame version.
	SaveLoadAddrProc *address_proc; ///< Callback proc the get the actual variable address in memory.
	size_t extra_data;              ///< Extra data for the callback proc.
	std::shared_ptr<SaveLoadHandler> handler; ///< Custom handler for Save/Load procs.
};

/**
 * SaveLoad information for backwards compatibility.
 *
 * At SaveLoadVersion::TableChunks a new method of keeping track of fields in a savegame
 * was added, where the order of fields is no longer important. For older
 * savegames we still need to know the correct order. This struct is the glue
 * to make that happen.
 */
struct SaveLoadCompat {
	std::string_view name; ///< Name of the field.
	uint16_t null_length; ///< Length of the NULL field.
	SaveLoadVersion version_from; ///< Save/load the variable starting from this savegame version.
	SaveLoadVersion version_to; ///< Save/load the variable before this savegame version.
};

/**
 * Return expect size in bytes of a VarType
 * @param type VarType to get size of.
 * @return size of type in bytes.
 */
inline constexpr size_t SlVarSize(VarMemType type)
{
	switch (type) {
		case VarMemType::Bool: return sizeof(bool);
		case VarMemType::I8: return sizeof(int8_t);
		case VarMemType::U8: return sizeof(uint8_t);
		case VarMemType::I16: return sizeof(int16_t);
		case VarMemType::U16: return sizeof(uint16_t);
		case VarMemType::I32: return sizeof(int32_t);
		case VarMemType::U32: return sizeof(uint32_t);
		case VarMemType::I64: return sizeof(int64_t);
		case VarMemType::U64: return sizeof(uint64_t);
		case VarMemType::Null: return sizeof(void *);
		case VarMemType::Str: return sizeof(std::string);
		case VarMemType::StrQ: return sizeof(std::string);
		case VarMemType::Name: return sizeof(std::string);
		case VarMemType::LabelReverse: return sizeof(BaseLabel);
		case VarMemType::LabelForward: return sizeof(BaseLabel);
		default: NOT_REACHED();
	}
}

/**
 * Check if a saveload cmd/type/length entry matches the size of the variable.
 * @param cmd SaveLoadType of entry.
 * @param type VarType of entry.
 * @param length Array length of entry.
 * @param size Actual size of variable.
 * @return true iff the sizes match.
 */
inline constexpr bool SlCheckVarSize(SaveLoadType cmd, VarType type, size_t length, size_t size)
{
	switch (cmd) {
		case SaveLoadType::Variable: return SlVarSize(type.mem) == size;
		case SaveLoadType::Reference: return sizeof(void *) == size;
		case SaveLoadType::String: return SlVarSize(type.mem) == size;
		case SaveLoadType::Array: return SlVarSize(type.mem) * length <= size; // Partial load of array is permitted.
		case SaveLoadType::Vector: return sizeof(std::vector<void *>) == size;
		case SaveLoadType::ReferenceList: return sizeof(std::list<void *>) == size;
		case SaveLoadType::ReferenceVector: return sizeof(std::vector<void *>) == size;
		case SaveLoadType::SaveByte: return true;
		default: NOT_REACHED();
	}
}

/**
 * Storage of simple variables, references (pointers), and arrays.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param name     Field name for table chunks.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLE_* macros below.
 */
#define SLE_GENERAL_NAME(cmd, name, base, variable, type, length, from, to, extra) \
	SaveLoad {name, cmd, type, length, from, to, [] (void *b, size_t) -> void * { \
		static_assert(SlCheckVarSize(cmd, type, length, sizeof(static_cast<base *>(b)->variable))); \
		static_assert((VarType{type}.mem != VarMemType::LabelReverse && VarType{type}.mem != VarMemType::LabelForward) || std::is_base_of_v<BaseLabel, decltype(base::variable)>); \
		assert(b != nullptr); \
		return const_cast<void *>(static_cast<const void *>(std::addressof(static_cast<base *>(b)->variable))); \
	}, extra, nullptr}

/**
 * Storage of simple variables, references (pointers), and arrays with a custom name.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLE_* macros below.
 */
#define SLE_GENERAL(cmd, base, variable, type, length, from, to, extra) SLE_GENERAL_NAME(cmd, #variable, base, variable, type, length, from, to, extra)

/**
 * Storage of a variable in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDVAR(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::Variable, base, variable, type, 0, from, to, 0)

/**
 * Storage of a variable in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDVARNAME(base, variable, name, type, from, to) SLE_GENERAL_NAME(SaveLoadType::Variable, name, base, variable, type, 0, from, to, 0)

/**
 * Storage of a reference in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDREF(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::Reference, base, variable, type, 0, from, to, 0)

/**
 * Storage of a fixed-size array of #SaveLoadType::Variable elements in some savegame versions.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLE_CONDARR(base, variable, type, length, from, to) SLE_GENERAL(SaveLoadType::Array, base, variable, type, length, from, to, 0)

/**
 * Storage of a fixed-size array of #SaveLoadType::Variable elements in some savegame versions.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLE_CONDARRNAME(base, variable, name, type, length, from, to) SLE_GENERAL_NAME(SaveLoadType::Array, name, base, variable, type, length, from, to, 0)

/**
 * Storage of a \c std::string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSSTR(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::String, base, variable, type, 0, from, to, 0)

/**
 * Storage of a \c std::string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSSTRNAME(base, variable, name, type, from, to) SLE_GENERAL_NAME(SaveLoadType::String, name, base, variable, type, 0, from, to, 0)

/**
 * Storage of a list of #SaveLoadType::Reference elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDREFLIST(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::ReferenceList, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SaveLoadType::Reference elements in some savegame versions.
 * @param base     Name of the class or struct containing the vector.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the vector.
 * @param to       Last savegame version that has the vector.
 */
#define SLE_CONDREFVECTOR(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::ReferenceVector, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SaveLoadType::Variable elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDVECTOR(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::Vector, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SaveLoadType::Variable elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDVECTOR(base, variable, type, from, to) SLE_GENERAL(SaveLoadType::Vector, base, variable, type, 0, from, to, 0)

/**
 * Storage of a variable in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_VAR(base, variable, type) SLE_CONDVAR(base, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a variable in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_VARNAME(base, variable, name, type) SLE_CONDVARNAME(base, variable, name, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a reference in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 */
#define SLE_REF(base, variable, type) SLE_CONDREF(base, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of fixed-size array of #SaveLoadType::Variable elements in every version of a savegame.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLE_ARR(base, variable, type, length) SLE_CONDARR(base, variable, type, length, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of fixed-size array of #SaveLoadType::Variable elements in every version of a savegame.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLE_ARRNAME(base, variable, name, type, length) SLE_CONDARRNAME(base, variable, name, type, length, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a \c std::string in every savegame version.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_SSTR(base, variable, type) SLE_CONDSSTR(base, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a \c std::string in every savegame version.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_SSTRNAME(base, variable, name, type) SLE_CONDSSTRNAME(base, variable, name, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a list of #SaveLoadType::Reference elements in every savegame version.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_REFLIST(base, variable, type) SLE_CONDREFLIST(base, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a vector of #SaveLoadType::Reference elements in every savegame version.
 * @param base     Name of the class or struct containing the vector.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_REFVECTOR(base, variable, type) SLE_CONDREFVECTOR(base, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Only write byte during saving; never read it during loading.
 * When using SLE_SAVEBYTE you will have to read this byte before the table
 * this is in is read. This also means SLE_SAVEBYTE can only be used at the
 * top of a chunk.
 * This is intended to be used to indicate what type of entry this is in a
 * list of entries.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 */
#define SLE_SAVEBYTE(base, variable) SLE_GENERAL(SaveLoadType::SaveByte, base, variable, {}, 0, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion, 0)

/**
 * Storage of global simple variables, references (pointers), and arrays.
 * @param name     The name of the field.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLEG_* macros below.
 */
#define SLEG_GENERAL(name, cmd, variable, type, length, from, to, extra) \
	SaveLoad {name, cmd, type, length, from, to, [] (void *, size_t) -> void * { \
		static_assert(SlCheckVarSize(cmd, type, length, sizeof(variable))); \
		return static_cast<void *>(std::addressof(variable)); }, extra, nullptr}

/**
 * Storage of a global variable in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDVAR(name, variable, type, from, to) SLEG_GENERAL(name, SaveLoadType::Variable, variable, type, 0, from, to, 0)

/**
 * Storage of a global reference in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDREF(name, variable, type, from, to) SLEG_GENERAL(name, SaveLoadType::Reference, variable, type, 0, from, to, 0)

/**
 * Storage of a global fixed-size array of #SaveLoadType::Variable elements in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLEG_CONDARR(name, variable, type, length, from, to) SLEG_GENERAL(name, SaveLoadType::Array, variable, type, length, from, to, 0)

/**
 * Storage of a global \c std::string in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLEG_CONDSSTR(name, variable, type, from, to) SLEG_GENERAL(name, SaveLoadType::String, variable, type, 0, from, to, 0)

/**
 * Storage of a structs in some savegame versions.
 * @param name     The name of the field.
 * @param handler  SaveLoadHandler for the structs.
 * @param from     First savegame version that has the struct.
 * @param to       Last savegame version that has the struct.
 */
#define SLEG_CONDSTRUCT(name, handler, from, to) SaveLoad {name, SaveLoadType::Struct, {}, 0, from, to, nullptr, 0, std::make_shared<handler>()}

/**
 * Storage of a global reference list in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDREFLIST(name, variable, type, from, to) SLEG_GENERAL(name, SaveLoadType::ReferenceList, variable, type, 0, from, to, 0)

/**
 * Storage of a global vector of #SaveLoadType::Variable elements in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDVECTOR(name, variable, type, from, to) SLEG_GENERAL(name, SaveLoadType::Vector, variable, type, 0, from, to, 0)

/**
 * Storage of a list of structs in some savegame versions.
 * @param name     The name of the field.
 * @param handler  SaveLoadHandler for the list of structs.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDSTRUCTLIST(name, handler, from, to) SaveLoad {name, SaveLoadType::StructList, {}, 0, from, to, nullptr, 0, std::make_shared<handler>()}

/**
 * Storage of a global variable in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_VAR(name, variable, type) SLEG_CONDVAR(name, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a global reference in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_REF(name, variable, type) SLEG_CONDREF(name, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a global fixed-size array of #SaveLoadType::Variable elements in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLEG_ARR(name, variable, type, length) SLEG_CONDARR(name, variable, type, length, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a global \c std::string in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_SSTR(name, variable, type) SLEG_CONDSSTR(name, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a structs in every savegame version.
 * @param name     The name of the field.
 * @param handler SaveLoadHandler for the structs.
 */
#define SLEG_STRUCT(name, handler) SLEG_CONDSTRUCT(name, handler, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a global reference list in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_REFLIST(name, variable, type) SLEG_CONDREFLIST(name, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a global vector of #SaveLoadType::Variable elements in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_VECTOR(name, variable, type) SLEG_CONDVECTOR(name, variable, type, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Storage of a list of structs in every savegame version.
 * @param name    The name of the field.
 * @param handler SaveLoadHandler for the list of structs.
 */
#define SLEG_STRUCTLIST(name, handler) SLEG_CONDSTRUCTLIST(name, handler, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion)

/**
 * Field name where the real SaveLoad can be located.
 * @param name The name of the field.
 */
#define SLC_VAR(name) {name, 0, SaveLoadVersion::MinVersion, SaveLoadVersion::MaxVersion}

/**
 * Empty space in every savegame version.
 * @param length Length of the empty space in bytes.
 * @param from   First savegame version that has the empty space.
 * @param to     Last savegame version that has the empty space.
 */
#define SLC_NULL(length, from, to) {{}, length, from, to}

/**
 * Checks whether the savegame is below \a major.\a minor.
 * @param major Major number of the version to check against.
 * @param minor Minor number of the version to check against. If \a minor is 0 or not specified, only the major number is checked.
 * @return Savegame version is earlier than the specified version.
 */
inline bool IsSavegameVersionBefore(SaveLoadVersion major, uint8_t minor = 0)
{
	extern SaveLoadVersion _sl_version;
	extern uint8_t            _sl_minor_version;
	return _sl_version < major || (minor > 0 && _sl_version == major && _sl_minor_version < minor);
}

/**
 * Checks whether the savegame is below or at \a major. This should be used to repair data from existing
 * savegames which is no longer corrupted in new savegames, but for which otherwise no savegame
 * bump is required.
 * @param major Major number of the version to check against.
 * @return Savegame version is at most the specified version.
 */
inline bool IsSavegameVersionBeforeOrAt(SaveLoadVersion major)
{
	extern SaveLoadVersion _sl_version;
	return _sl_version <= major;
}

/**
 * Checks if some version from/to combination falls within the range of the
 * active savegame version.
 * @param version_from Inclusive savegame version lower bound.
 * @param version_to Exclusive savegame version upper bound. MaxVersion if no upper bound.
 * @return Active savegame version falls within the given range.
 */
inline bool SlIsObjectCurrentlyValid(SaveLoadVersion version_from, SaveLoadVersion version_to)
{
	extern const SaveLoadVersion SAVEGAME_VERSION;
	return version_from <= SAVEGAME_VERSION && SAVEGAME_VERSION < version_to;
}

/**
 * Get the address of the variable. Null-variables don't have an address,
 * everything else has a callback function that returns the address based
 * on the saveload data and the current object for non-globals.
 * @param object The object to get a relative address from, or \c nullptr for global objects.
 * @param sld The save-load configuration for a single variable.
 * @return The address where to store the given variable into.
 */
inline void *GetVariableAddress(const void *object, const SaveLoad &sld)
{
	/* Entry is a null-variable, mostly used to read old savegames etc. */
	if (sld.conv.mem == VarMemType::Null) {
		assert(sld.address_proc == nullptr);
		return nullptr;
	}

	/* Everything else should be a non-null pointer. */
	assert(sld.address_proc != nullptr);
	return sld.address_proc(const_cast<void *>(object), sld.extra_data);
}

int64_t ReadValue(const void *ptr, VarMemType conv);
void WriteValue(void *ptr, VarMemType conv, int64_t val);

void SlSetArrayIndex(uint index);
static void SlSetArrayIndex(const ConvertibleThroughBase auto &index) { SlSetArrayIndex(index.base()); }
int SlIterateArray();

void SlSetStructListLength(size_t length);
size_t SlGetStructListLength(size_t limit);

void SlAutolength(AutolengthProc *proc, int arg);
size_t SlGetFieldLength();
void SlSetLength(size_t length);
size_t SlCalcObjMemberLength(const void *object, const SaveLoad &sld);
size_t SlCalcObjLength(const void *object, const SaveLoadTable &slt);

uint8_t SlReadByte();
void SlReadString(std::string &str, size_t length);
void SlWriteByte(uint8_t b);

void SlGlobList(const SaveLoadTable &slt);
void SlCopy(void *object, size_t length, VarType conv);
std::vector<SaveLoad> SlTableHeader(const SaveLoadTable &slt);
std::vector<SaveLoad> SlCompatTableHeader(const SaveLoadTable &slt, const SaveLoadCompatTable &slct);
void SlObject(void *object, const SaveLoadTable &slt);

/**
 * Read in bytes from the file/data structure but don't do
 * anything with them, discarding them in effect
 * @param length The amount of bytes that is being treated this way
 */
inline void SlSkipBytes(size_t length)
{
	for (; length != 0; length--) SlReadByte();
}

/**
 * Default handler for saving/loading a vector to/from disk.
 *
 * This handles a few common things for handlers, meaning the actual handler
 * needs less code.
 *
 * @tparam TImpl The class initializing this template.
 * @tparam TObject The class of the object using this SaveLoadHandler.
 * @tparam TElementType The type of the elements contained within the vector.
 * @tparam MAX_LENGTH maximum number of elements to load.
 */
template <class TImpl, class TObject, class TElementType, size_t MAX_LENGTH = UINT32_MAX>
class VectorSaveLoadHandler : public DefaultSaveLoadHandler<TImpl, TObject> {
public:
	/**
	 * Get instance of vector to load/save.
	 * @param object Object containing vector.
	 * @returns Vector to load/save.
	 */
	virtual std::vector<TElementType> &GetVector(TObject *object) const = 0;

	/**
	 * Get number of elements to load into vector.
	 * @returns Number of elements to load into the vector.
	 * @note This is only overridden if the number of elements comes from a different location due to savegame changes.
	 */
	virtual size_t GetLength() const { return SlGetStructListLength(MAX_LENGTH); }

	void Save(TObject *object) const override
	{
		auto &vector = this->GetVector(object);
		SlSetStructListLength(vector.size());

		for (auto &item : vector) {
			SlObject(&item, this->GetDescription());
		}
	}

	void Load(TObject *object) const override
	{
		auto &vector = this->GetVector(object);
		size_t count = this->GetLength();

		vector.reserve(count);
		while (count-- > 0) {
			auto &item = vector.emplace_back();
			SlObject(&item, this->GetLoadDescription());
		}
	}
};

#endif /* SAVELOAD_H */
