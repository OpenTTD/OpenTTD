/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_type.h Types related to commands. */

#ifndef COMMAND_TYPE_H
#define COMMAND_TYPE_H

#include "company_type.h"
#include "economy_type.h"
#include "strings_type.h"
#include "tile_type.h"

struct GRFFile;

/**
 * Common return value for all commands. Wraps the cost and
 * a possible error message/state together.
 */
class CommandCost {
	Money cost;                                 ///< The cost of this action
	StringID message;                           ///< Warning message for when success is unset
	ExpensesType expense_type;                  ///< the type of expense as shown on the finances view
	bool success;                               ///< Whether the command went fine up to this moment
	Owner owner = CompanyID::Invalid(); ///< Originator owner of error.
	StringID extra_message = INVALID_STRING_ID; ///< Additional warning message for when success is unset
	EncodedString encoded_message{}; ///< Encoded error message, used if the error message includes parameters.

public:
	/**
	 * Creates a command cost return with no cost and no error
	 */
	CommandCost() : cost(0), message(INVALID_STRING_ID), expense_type(INVALID_EXPENSES), success(true) {}

	/**
	 * Creates a command return value with one, or optionally two, error message strings.
	 */
	explicit CommandCost(StringID msg, StringID extra_msg = INVALID_STRING_ID) : cost(0), message(msg), expense_type(INVALID_EXPENSES), success(false), extra_message(extra_msg) {}

	/**
	 * Creates a command cost with given expense type and start cost of 0
	 * @param ex_t the expense type
	 */
	explicit CommandCost(ExpensesType ex_t) : cost(0), message(INVALID_STRING_ID), expense_type(ex_t), success(true) {}

	/**
	 * Creates a command return value with the given start cost and expense type
	 * @param ex_t the expense type
	 * @param cst the initial cost of this command
	 */
	CommandCost(ExpensesType ex_t, const Money &cst) : cost(cst), message(INVALID_STRING_ID), expense_type(ex_t), success(true) {}

	/**
	 * Set the 'owner' (the originator) of this error message. This is used to show a company owner's face if you
	 * attempt an action on something owned by other company.
	 */
	inline void SetErrorOwner(Owner owner)
	{
		this->owner = owner;
	}

	/**
	 * Set the encoded message string. If set, this is used by the error message window instead of the error StringID,
	 * to allow more information to be displayed to the local player.
	 * @note Do not set an encoded message if the error is not for the local player, as it will never be seen.
	 * @param message EncodedString message to set.
	 */
	void SetEncodedMessage(EncodedString &&message)
	{
		this->encoded_message = std::move(message);
	}

	/**
	 * Get the last encoded error message.
	 * @returns Reference to the encoded message.
	 */
	EncodedString &GetEncodedMessage()
	{
		return this->encoded_message;
	}

	/**
	 * Get the originator owner for this error.
	 */
	inline CompanyID GetErrorOwner() const
	{
		return this->owner;
	}

	/**
	 * Adds the given cost to the cost of the command.
	 * @param cost the cost to add
	 */
	inline void AddCost(const Money &cost)
	{
		this->cost += cost;
	}

	void AddCost(CommandCost &&cmd_cost);

	/**
	 * Multiplies the cost of the command by the given factor.
	 * @param factor factor to multiply the costs with
	 */
	inline void MultiplyCost(int factor)
	{
		this->cost *= factor;
	}

	/**
	 * The costs as made up to this moment
	 * @return the costs
	 */
	inline Money GetCost() const
	{
		return this->cost;
	}

	/**
	 * The expense type of the cost
	 * @return the expense type
	 */
	inline ExpensesType GetExpensesType() const
	{
		return this->expense_type;
	}

	/**
	 * Makes this #CommandCost behave like an error command.
	 * @param message The error message.
	 */
	void MakeError(StringID message)
	{
		assert(message != INVALID_STRING_ID);
		this->success = false;
		this->message = message;
		this->extra_message = INVALID_STRING_ID;
	}

	/**
	 * Returns the error message of a command
	 * @return the error message, if succeeded #INVALID_STRING_ID
	 */
	StringID GetErrorMessage() const
	{
		if (this->success) return INVALID_STRING_ID;
		return this->message;
	}

	/**
	 * Returns the extra error message of a command
	 * @return the extra error message, if succeeded #INVALID_STRING_ID
	 */
	StringID GetExtraErrorMessage() const
	{
		if (this->success) return INVALID_STRING_ID;
		return this->extra_message;
	}

	/**
	 * Did this command succeed?
	 * @return true if and only if it succeeded
	 */
	inline bool Succeeded() const
	{
		return this->success;
	}

	/**
	 * Did this command fail?
	 * @return true if and only if it failed
	 */
	inline bool Failed() const
	{
		return !this->success;
	}
};

CommandCost CommandCostWithParam(StringID str, uint64_t value);
CommandCost CommandCostWithParam(StringID str, ConvertibleThroughBase auto value) { return CommandCostWithParam(str, value.base()); }

/**
 * List of commands.
 *
 * This enum defines all possible commands which can be executed to the game
 * engine. Observing the game like the query-tool or checking the profit of a
 * vehicle don't result in a command which should be executed in the engine
 * nor send to the server in a network game.
 *
 * @see _command_proc_table
 */
enum class Commands : uint8_t {
	BuildRailLong, ///< build a rail track
	RemoveRailLong, ///< remove a rail track
	BuildRail, ///< build a single rail track
	RemoveRail, ///< remove a single rail track
	LandscapeClear, ///< demolish a tile
	BuildBridge, ///< build a bridge
	BuildRailStation, ///< build a rail station
	BuildRailDepot, ///< build a train depot
	BuildSignal, ///< build a signal
	RemoveSignal, ///< remove a signal
	TerraformLand, ///< terraform a tile
	BuildObject, ///< build an object
	BuildObjectArea, ///< build an area of objects
	BuildTunnel, ///< build a tunnel

	RemoveFromRailStation, ///< remove a (rectangle of) tiles from a rail station
	ConvertRail, ///< convert a rail type

	BuildRailWaypoint, ///< build a waypoint
	RenameWaypoint, ///< rename a waypoint
	MoveWaypointNAme, ///< move a waypoint name
	RemoveFromRailWaypoint, ///< remove a (rectangle of) tiles from a rail waypoint

	BuildRoadWaypoint, ///< build a road waypoint
	RemoveFromRoadWaypoint, ///< remove a (rectangle of) tiles from a road waypoint

	BuildRoadStop, ///< build a road stop
	RemoveRoadStop, ///< remove a road stop
	BuildRoadLong, ///< build a complete road (not a "half" one)
	RemoveRoadLong, ///< remove a complete road (not a "half" one)
	BuildRoad, ///< build a "half" road
	BuildRoadDepot, ///< build a road depot
	ConvertRoad, ///< convert a road type

	BuildAirport, ///< build an airport

	BuildDock, ///< build a dock

	BuildShipDepot, ///< build a ship depot
	BuildBuoy, ///< build a buoy

	PlantTree, ///< plant a tree

	BuildVehicle, ///< build a vehicle
	SellVehicle, ///< sell a vehicle
	RefitVehicle, ///< refit the cargo space of a vehicle
	SendVehicleToDepot, ///< send a vehicle to a depot
	SetVehicleVisibility, ///< hide or unhide a vehicle in the build vehicle and autoreplace GUIs

	MoveRailVehicle, ///< move a rail vehicle (in the depot)
	ForceTrainProceed, ///< proceed a train to pass a red signal
	ReverseTrainDirection, ///< turn a train around

	ClearOrderBackup, ///< clear the order backup of a given user/tile
	ModifyOrder, ///< modify an order (like set full-load)
	SkipToOrder, ///< skip an order to the next of specific one
	DeleteOrder, ///< delete an order
	InsertOrder, ///< insert a new order

	ChangeServiceInterval, ///< change the server interval of a vehicle

	BuildIndustry, ///< build a new industry
	IndustrySetFlags, ///< change industry control flags
	IndustrySetExclusivity, ///< change industry exclusive consumer/supplier
	IndustrySetText, ///< change additional text for the industry
	IndustrySetProduction, ///< change industry production

	SetCompanyManagerFace, ///< set the manager's face of the company
	SetCompanyColour, ///< set the colour of the company

	IncreaseLoan, ///< increase the loan from the bank
	DecreaseLoan, ///< decrease the loan from the bank
	SetCompanyMaxLoan, ///< sets the max loan for the company

	WantEnginePreview, ///< confirm the preview of an engine
	EngineControl, ///< control availability of the engine for companies

	RenameVehicle, ///< rename a whole vehicle
	RenameEngine, ///< rename a engine (in the engine list)
	RenameCompany, ///< change the company name
	RenamePresident, ///< change the president name
	RenameStation, ///< rename a station
	MoveStationName, ///< move a station name
	RenameDepot, ///< rename a depot

	PlaceSign, ///< place a sign
	RenameSign, ///< rename a sign
	MoveSign, ///< move a sign

	TurnRoadVehicle, ///< turn a road vehicle around

	Pause, ///< pause the game

	BuyCompany, ///< buy a company which is bankrupt

	FoundTown, ///< found a town
	RenameTown, ///< rename a town
	TownAction, ///< do a action from the town detail window (like advertises or bribe)
	TownCargoGoal, ///< set the goal of a cargo for a town
	TownGrowthRate, ///< set the town growth rate
	TownRating, ///< set rating of a company in a town
	TownSetText, ///< set the custom text of a town
	ExpandTown, ///< expand a town
	DeleteTown, ///< delete a town
	PlaceHouse, ///< place a house
	PlaceHouseArea, ///< place an area of houses

	OrderRefit, ///< change the refit information of an order (for "goto depot" )
	CloneOrder, ///< clone (and share) an order
	ClearArea, ///< clear an area

	MoneyCheat, ///< do the money cheat
	ChangeBankBalance, ///< change bank balance to charge costs or give money from a GS
	BuildCanal, ///< build a canal

	CreateSubsidy, ///< create a new subsidy
	CompanyControl, ///< used in multiplayer to create a new companies etc.
	CompanyAllowListControl, ///< Used in multiplayer to add/remove a client's public key to/from the company's allow list.
	CreateCustomNewsItem, ///< create a custom news message
	CreateGoal, ///< create a new goal
	RemoveGoal, ///< remove a goal
	SetGoalDestination, ///< update goal destination of a goal
	SetGoalText, ///< update goal text of a goal
	SetGoalProgress, ///< update goal progress text of a goal
	SetGoalCompleted, ///< update goal completed status of a goal
	GoalQuestion, ///< ask a goal related question
	GoalQuestionAnswer, ///< answer(s) to Commands::GoalQuestion
	CreateStoryPage, ///< create a new story page
	CreateStoryPageElement, ///< create a new story page element
	UpdateStoryPageElement, ///< update a story page element
	SetStoryPageTitle, ///< update title of a story page
	SetStoryPageDate, ///< update date of a story page
	ShowStoryPage, ///< show a story page
	RemoveStoryPage, ///< remove a story page
	RemoveStoryPageElement, ///< remove a story page element
	ScrollViewport, ///< scroll main viewport of players
	StoryPageButton, ///< selection via story page button

	LevelLand, ///< level land

	BuildLock, ///< build a lock

	BuildSignalLong, ///< add signals along a track (by dragging)
	RemoveSignalLong, ///< remove signals along a track (by dragging)

	GiveMoney, ///< give money to another company
	ChangeSetting, ///< change a setting
	ChangeCompanySetting, ///< change a company setting

	SetAutoreplace, ///< set an autoreplace entry

	CloneVehicle, ///< clone a vehicle
	StartStopVehicle, ///< start or stop a vehicle
	MassStartStop, ///< start/stop all vehicles (in a depot)
	AutoreplaceVehicle, ///< replace/renew a vehicle while it is in a depot
	DepotMassSell, ///< sell all vehicles which are in a given depot
	DepotMassAutoreplace, ///< force the autoreplace to take action in a given depot

	CreateGroup, ///< create a new group
	DeleteGroup, ///< delete a group
	AlterGroup, ///< alter a group
	AddVehicleToGroup, ///< add a vehicle to a group
	AddSharedVehiclesToGroup, ///< add all other shared vehicles to a group which are missing
	RemoveAllVehiclesGroup, ///< remove all vehicles from a group
	SetGroupFlag, ///< set/clear a flag for a group
	SetGroupLivery, ///< set the livery for a group

	MoveOrder, ///< move an order
	ChangeTimetable, ///< change the timetable for a vehicle
	ChangeTimetableBulk, ///< change the timetable for all orders of a vehicle
	SetVehicleOnTime, ///< set the vehicle on time feature (timetable)
	AutofillTimetable, ///< autofill the timetable
	SetTimetableStart, ///< set the date that a timetable should start

	OpenCloseAirport, ///< open/close an airport to incoming aircraft

	CreateLeagueTable, ///< create a new league table
	CreateLeagueTableElement, ///< create a new element in a league table
	UpdateLeagueTableElementData, ///< update the data fields of a league table element
	UpdateLeagueTableElementScore, ///< update the score of a league table element
	RemoveLeagueTableElement, ///< remove a league table element

	End, ///< @important Must ALWAYS be on the end of this list!! (period)
};

/**
 * List of flags for a command.
 *
 * This enums defines some flags which can be used for the commands.
 */
enum class DoCommandFlag : uint8_t {
	Execute, ///< execute the given command
	Auto, ///< don't allow building on structures
	QueryCost, ///< query cost only,  don't build.
	NoWater, ///< don't allow building on water
	NoTestTownRating, ///< town rating does not disallow you from building
	Bankrupt, ///< company bankrupts, skip money check, skip vehicle on tile check in some cases
	AutoReplace, ///< autoreplace/autorenew is in progress, this shall disable vehicle limits when building, and ignore certain restrictions when undoing things (like vehicle attach callback)
	NoCargoCapacityCheck, ///< when autoreplace/autorenew is in progress, this shall prevent truncating the amount of cargo in the vehicle to prevent testing the command to remove cargo
	AllTiles, ///< allow this command also on TileType::Void tiles
	NoModifyTownRating, ///< do not change town rating
	ForceClearTile, ///< do not only remove the object on the tile, but also clear any water left on it
};
using DoCommandFlags = EnumBitSet<DoCommandFlag, uint16_t>;

/**
 * Command flags for the command table _command_proc_table.
 *
 * This enumeration defines flags for the _command_proc_table.
 */
enum class CommandFlag : uint8_t {
	Server, ///< the command can only be initiated by the server
	Spectator, ///< the command may be initiated by a spectator
	Offline, ///< the command cannot be executed in a multiplayer game; single-player only
	Auto, ///< set the DoCommandFlag::Auto flag on this command
	AllTiles, ///< allow this command also on TileType::Void tiles
	NoTest, ///< the command's output may differ between test and execute due to town rating changes etc.
	NoWater, ///< set the DoCommandFlag::NoWater flag on this command
	ClientID, ///< set p2 with the ClientID of the sending client.
	Deity, ///< the command may be executed by COMPANY_DEITY
	StrCtrl, ///< the command's string may contain control strings
	NoEst, ///< the command is never estimated.
	Location, ///< the command has implicit location argument.
};
using CommandFlags = EnumBitSet<CommandFlag, uint16_t>;

/** Types of commands we have. */
enum class CommandType : uint8_t {
	LandscapeConstruction, ///< Construction and destruction of objects on the map.
	VehicleConstruction, ///< Construction, modification (incl. refit) and destruction of vehicles.
	MoneyManagement, ///< Management of money, i.e. loans.
	VehicleManagement, ///< Stopping, starting, sending to depot, turning around, replace orders etc.
	RouteManagement, ///< Modifications to route management (orders, groups, etc).
	OtherManagement, ///< Renaming stuff, changing company colours, placing signs, etc.
	CompanySetting, ///< Changing settings related to a company.
	ServerSetting, ///< Pausing/removing companies/server settings.
	Cheat, ///< A cheat of some sorts.

	End, ///< End marker.
};

/** Different command pause levels. */

enum class CommandPauseLevel : uint8_t {
	NoActions, ///< No user actions may be executed.
	NoConstruction, ///< No construction actions may be executed.
	NoLandscaping, ///< No landscaping actions may be executed.
	AllActions, ///< All actions may be executed.
};


template <typename T> struct CommandFunctionTraitHelper;
template <typename... Targs>
struct CommandFunctionTraitHelper<CommandCost(*)(DoCommandFlags, Targs...)> {
	using Args = std::tuple<std::decay_t<Targs>...>;
	using RetTypes = void;
	using CbArgs = Args;
	using CbProcType = void(*)(Commands, const CommandCost &);
};
template <template <typename...> typename Tret, typename... Tretargs, typename... Targs>
struct CommandFunctionTraitHelper<Tret<CommandCost, Tretargs...>(*)(DoCommandFlags, Targs...)> {
	using Args = std::tuple<std::decay_t<Targs>...>;
	using RetTypes = std::tuple<std::decay_t<Tretargs>...>;
	using CbArgs = std::tuple<std::decay_t<Tretargs>..., std::decay_t<Targs>...>;
	using CbProcType = void(*)(Commands, const CommandCost &, Tretargs...);
};

/** Defines the traits of a command. */
template <Commands Tcmd> struct CommandTraits;

#define DEF_CMD_TRAIT(cmd_, proc_, flags_, type_) \
	template <> struct CommandTraits<cmd_> { \
		using ProcType = decltype(&proc_); \
		using Args = typename CommandFunctionTraitHelper<ProcType>::Args; \
		using RetTypes = typename CommandFunctionTraitHelper<ProcType>::RetTypes; \
		using CbArgs = typename CommandFunctionTraitHelper<ProcType>::CbArgs; \
		using RetCallbackProc = typename CommandFunctionTraitHelper<ProcType>::CbProcType; \
		static constexpr Commands cmd = cmd_; \
		static constexpr auto &proc = proc_; \
		static constexpr CommandFlags flags = flags_; \
		static constexpr CommandType type = type_; \
		static inline constexpr std::string_view name = #proc_; \
	};

/** Storage buffer for serialized command data. */
typedef std::vector<uint8_t> CommandDataBuffer;

/**
 * Define a callback function for the client, after the command is finished.
 *
 * Functions of this type are called after the command is finished.
 *
 * @param cmd The command that was executed
 * @param result The result of the executed command
 * @param tile The tile of the command action
 */
typedef void CommandCallback(Commands cmd, const CommandCost &result, TileIndex tile);

/**
 * Define a callback function for the client, after the command is finished.
 *
 * Functions of this type are called after the command is finished.
 *
 * @param cmd The command that was executed
 * @param result The result of the executed command
 * @param tile The tile of the command action
 * @param data Additional data of the command
 * @param result_data Additional returned data from the command
 */
typedef void CommandCallbackData(Commands cmd, const CommandCost &result, const CommandDataBuffer &data, CommandDataBuffer result_data);

#endif /* COMMAND_TYPE_H */
