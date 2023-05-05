/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_group.hpp Everything to put vehicles into groups. */

#ifndef SCRIPT_GROUP_HPP
#define SCRIPT_GROUP_HPP

#include "script_vehicle.hpp"
#include "../../group_type.h"

/**
 * Class that handles all group related functions.
 * @api ai game
 */
class ScriptGroup : public ScriptObject {
public:
	/**
	 * The group IDs of some special groups.
	 */
	enum GroupID {
		/* Note: these values represent part of the in-game static values */
		GROUP_ALL     = ::ALL_GROUP,     ///< All vehicles are in this group.
		GROUP_DEFAULT = ::DEFAULT_GROUP, ///< Vehicles not put in any other group are in this one.
		GROUP_INVALID = ::INVALID_GROUP, ///< An invalid group id.
	};

	/**
	 * Checks whether the given group is valid.
	 * @param group_id The group to check.
	 * @pre group_id != GROUP_DEFAULT && group_id != GROUP_ALL.
	 * @return True if and only if the group is valid.
	 */
	static bool IsValidGroup(GroupID group_id);

	/**
	 * Create a new group.
	 * @param vehicle_type The type of vehicle to create a group for.
	 * @param parent_group_id The parent group id to create this group under, INVALID_GROUP for top-level.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return The GroupID of the new group, or an invalid GroupID when
	 *  it failed. Check the return value using IsValidGroup(). In test-mode
	 *  0 is returned if it was successful; any other value indicates failure.
	 */
	static GroupID CreateGroup(ScriptVehicle::VehicleType vehicle_type, GroupID parent_group_id);

	/**
	 * Delete the given group. When the deletion succeeds all vehicles in the
	 *  given group will move to the GROUP_DEFAULT.
	 * @param group_id The group to delete.
	 * @pre IsValidGroup(group_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the group was successfully deleted.
	 */
	static bool DeleteGroup(GroupID group_id);

	/**
	 * Get the vehicle type of a group.
	 * @param group_id The group to get the type from.
	 * @pre IsValidGroup(group_id).
	 * @return The vehicletype of the given group.
	 */
	static ScriptVehicle::VehicleType GetVehicleType(GroupID group_id);

	/**
	 * Set the name of a group.
	 * @param group_id The group to set the name for.
	 * @param name The name for the group (can be either a raw string, or a ScriptText object).
	 * @pre IsValidGroup(group_id).
	 * @pre name != null && len(name) != 0
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if and only if the name was changed.
	 */
	static bool SetName(GroupID group_id, Text *name);

	/**
	 * Get the name of a group.
	 * @param group_id The group to get the name of.
	 * @pre IsValidGroup(group_id).
	 * @return The name the group has.
	 */
	static std::optional<std::string> GetName(GroupID group_id);

	/**
	 * Set parent group of a group.
	 * @param group_id The group to set the parent for.
	 * @param parent_group_id The parent group to set.
	 * @pre IsValidGroup(group_id).
	 * @pre IsValidGroup(parent_group_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the parent group was changed.
	 */
	static bool SetParent(GroupID group_id, GroupID parent_group_id);

	/**
	 * Get parent group of a group.
	 * @param group_id The group to get the parent of.
	 * @pre IsValidGroup(group_id).
	 * @return The group id of the parent group.
	 */
	static GroupID GetParent(GroupID group_id);

	/**
	 * Enable or disable autoreplace protected. If the protection is
	 *  enabled, global autoreplace won't affect vehicles in this group.
	 * @param group_id The group to change the protection for.
	 * @param enable True if protection should be enabled.
	 * @pre IsValidGroup(group_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the protection was successfully changed.
	 */
	static bool EnableAutoReplaceProtection(GroupID group_id, bool enable);

	/**
	 * Get the autoreplace protection status.
	 * @param group_id The group to get the protection status for.
	 * @pre IsValidGroup(group_id).
	 * @return The autoreplace protection status for the given group.
	 */
	static bool GetAutoReplaceProtection(GroupID group_id);

	/**
	 * Get the number of engines in a given group.
	 * @param group_id The group to get the number of engines in.
	 * @param engine_id The engine id to count.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_ALL || group_id == GROUP_DEFAULT.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return The number of engines with id engine_id in the group with id group_id.
	 */
	static SQInteger GetNumEngines(GroupID group_id, EngineID engine_id);

	/**
	 * Get the total number of vehicles in a given group and its sub-groups.
	 * @param group_id The group to get the number of vehicles in.
	 * @param vehicle_type The type of vehicle of the group.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_ALL || group_id == GROUP_DEFAULT.
	 * @pre IsValidGroup(group_id) || vehicle_type == ScriptVehicle::VT_ROAD || vehicle_type == ScriptVehicle::VT_RAIL ||
	 *   vehicle_type == ScriptVehicle::VT_WATER || vehicle_type == ScriptVehicle::VT_AIR
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return The total number of vehicles in the group with id group_id and it's sub-groups.
	 * @note If the group is valid (neither GROUP_ALL nor GROUP_DEFAULT), the value of
	 *  vehicle_type is retrieved from the group itself and not from the input value.
	 *  But if the group is GROUP_ALL or GROUP_DEFAULT, then vehicle_type must be valid.
	 */
	static SQInteger GetNumVehicles(GroupID group_id, ScriptVehicle::VehicleType vehicle_type);

	/**
	 * Move a vehicle to a group.
	 * @param group_id The group to move the vehicle to.
	 * @param vehicle_id The vehicle to move to the group.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_DEFAULT.
	 * @pre ScriptVehicle::IsPrimaryVehicle(vehicle_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the vehicle was successfully moved to the group.
	 * @note A vehicle can be in only one group at the same time. To remove it from
	 *  a group, move it to another or to GROUP_DEFAULT. Moving the vehicle to the
	 *  given group means removing it from another group.
	 */
	static bool MoveVehicle(GroupID group_id, VehicleID vehicle_id);

	/**
	 * Enable or disable the removal of wagons when a (part of a) vehicle is
	 *  (auto)replaced with a longer variant (longer wagons or longer engines)
	 *  If enabled, wagons are removed from the end of the vehicle until it
	 *  fits in the same number of tiles as it did before.
	 * @param keep_length If true, wagons will be removed if the new engine is longer.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and only if the value was successfully changed.
	 */
	static bool EnableWagonRemoval(bool keep_length);

	/**
	 * Get the current status of wagon removal.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return Whether or not wagon removal is enabled.
	 */
	static bool HasWagonRemoval();

	/**
	 * Start replacing all vehicles with a specified engine with another engine.
	 * @param group_id The group to replace vehicles from. Use ALL_GROUP to replace
	 *  vehicles from all groups that haven't set autoreplace protection.
	 * @param engine_id_old The engine id to start replacing.
	 * @param engine_id_new The engine id to replace with.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_DEFAULT || group_id == GROUP_ALL.
	 * @pre ScriptEngine.IsBuildable(engine_id_new).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and if the replacing was successfully started.
	 * @note To stop autoreplacing engine_id_old, call StopAutoReplace(group_id, engine_id_old).
	 */
	static bool SetAutoReplace(GroupID group_id, EngineID engine_id_old, EngineID engine_id_new);

	/**
	 * Get the EngineID the given EngineID is replaced with.
	 * @param group_id The group to get the replacement from.
	 * @param engine_id The engine that is being replaced.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_DEFAULT || group_id == GROUP_ALL.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return The EngineID that is replacing engine_id or an invalid EngineID
	 *   in case engine_id is not begin replaced.
	 */
	static EngineID GetEngineReplacement(GroupID group_id, EngineID engine_id);

	/**
	 * Stop replacing a certain engine in the specified group.
	 * @param group_id The group to stop replacing the engine in.
	 * @param engine_id The engine id to stop replacing with another engine.
	 * @pre IsValidGroup(group_id) || group_id == GROUP_DEFAULT || group_id == GROUP_ALL.
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True if and if the replacing was successfully stopped.
	 */
	static bool StopAutoReplace(GroupID group_id, EngineID engine_id);

	/**
	 * Get the current profit of a group.
	 * @param group_id The group to get the profit of.
	 * @pre IsValidGroup(group_id).
	 * @return The current profit the group has.
	 */
	static Money GetProfitThisYear(GroupID group_id);

	/**
	 * Get the profit of last year of a group.
	 * @param group_id The group to get the profit of.
	 * @pre IsValidGroup(group_id).
	 * @return The current profit the group had last year.
	 */
	static Money GetProfitLastYear(GroupID group_id);

	/**
	 * Get the current vehicle usage of a group.
	 * @param group_id The group to get the current usage of.
	 * @pre IsValidGroup(group_id).
	 * @return The current usage of the group.
	 */
	static SQInteger GetCurrentUsage(GroupID group_id);

	/**
	 * Set primary colour for a group.
	 * @param group_id The group id to set the colour of.
	 * @param colour Colour to set.
	 * @pre IsValidGroup(group_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True iff the colour was set successfully.
	 */
	static bool SetPrimaryColour(GroupID group_id, ScriptCompany::Colours colour);

	/**
	 * Set secondary colour for a group.
	 * @param group_id The group id to set the colour of.
	 * @param colour Colour to set.
	 * @pre IsValidGroup(group_id).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @return True iff the colour was set successfully.
	 */
	static bool SetSecondaryColour(GroupID group_id, ScriptCompany::Colours colour);

	/**
	 * Get primary colour of a group.
	 * @param group_id The group id to get the colour of.
	 * @pre IsValidGroup(group_id).
	 * @return The primary colour of the group.
	 */
	static ScriptCompany::Colours GetPrimaryColour(GroupID group_id);

	/**
	 * Get secondary colour for a group.
	 * @param group_id The group id to get the colour of.
	 * @pre IsValidGroup(group_id).
	 * @return The secondary colour of the group.
	 */
	static ScriptCompany::Colours GetSecondaryColour(GroupID group_id);
};

#endif /* SCRIPT_GROUP_HPP */
