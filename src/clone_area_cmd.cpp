/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file clone_area_cmd.cpp Commands related to clone area. */

#include "stdafx.h"
#include "command_func.h"
#include "viewport_func.h"
#include "company_base.h"
#include "clone_area_cmd.h"
#include "newgrf_station.h"
#include "safeguards.h"
#include "road.h"
#include "road_internal.h"
#include "depot_base.h"
#include "tunnelbridge_map.h"
#include "sound_func.h"
#include "rail_cmd.h"
#include "tunnelbridge_cmd.h"
#include "station_cmd.h"
#include "terraform_cmd.h"

TileIndex selected_tile;
TileIndex selected_start_tile;
bool selected_diagonal;

/**
 * Rotate by an angle using the formula for rotating a point on a plane.
 *
 * @param point Rotating point.
 * @param angle Rotate an angle.
 */
TileIndexDiffC Rotate(TileIndexDiffC point, DiagDirDiff angle) {
	switch (angle) {
		case DIAGDIRDIFF_90LEFT: return {int16_t(-point.y), point.x};
		case DIAGDIRDIFF_REVERSE: return {int16_t(-point.x), int16_t(-point.y)};
		case DIAGDIRDIFF_90RIGHT: return {point.y, int16_t(-point.x)};
		default: return point;
	}
}

/**
 * Adjusting the position of the rotated point, since the tile has four corners.
 *
 * @param rotated Rotated point setting.
 * @param angle Angle of rotation.
 */
TileIndexDiffC FixAfterRotate(TileIndexDiffC rotated, DiagDirDiff angle) {
	switch (angle) {
		case DIAGDIRDIFF_90LEFT:
			rotated.x -= 1;
			break;
		case DIAGDIRDIFF_REVERSE:
			rotated.x -= 1;
			rotated.y -= 1;
			break;
		case DIAGDIRDIFF_90RIGHT:
			rotated.y -= 1;
			break;
		default:
			return rotated;
	}
	return rotated;
}

/**
 * Rotate Track by an angle using the formula for rotating a point on a plane.
 *
 * @param track Rotating track.
 * @param angle Rotate an angle.
 */
Track Rotate(Track track, DiagDirDiff angle) {
	switch (angle) {
		case DIAGDIRDIFF_90LEFT:
			if (track == TRACK_X || track == TRACK_Y) {
				return TrackToOppositeTrack(track);
			}
			switch (track) {
				case TRACK_UPPER: return TRACK_LEFT;
				case TRACK_LOWER: return TRACK_RIGHT;
				case TRACK_LEFT: return TRACK_LOWER;
				case TRACK_RIGHT: return TRACK_UPPER;
				default: break;
			}
			break;
		case DIAGDIRDIFF_REVERSE:
			if (track != TRACK_X && track != TRACK_Y) {
				return TrackToOppositeTrack(track);
			}
			return track;
			break;
		case DIAGDIRDIFF_90RIGHT:
			if (track == TRACK_X || track == TRACK_Y) {
				return TrackToOppositeTrack(track);
			}
			switch (track) {
				case TRACK_UPPER: return TRACK_RIGHT;
				case TRACK_LOWER: return TRACK_LEFT;
				case TRACK_LEFT: return TRACK_UPPER;
				case TRACK_RIGHT: return TRACK_LOWER;
				default: break;
			}
			break;
		default:
			return track;
	}
	return track;
}

/**
 * Rotate DiagDirection by an angle using the formula for rotating a point on a plane.
 *
 * @param dir Rotating DiagDirection.
 * @param angle Rotate an angle.
 */
DiagDirection Rotate(DiagDirection dir, DiagDirDiff angle) {
	switch (angle) {
		case DIAGDIRDIFF_90LEFT:
			dir = (DiagDirection)(dir - 1);
			if (dir == INVALID_DIAGDIR) {
				dir = DIAGDIR_NW;
			}
			break;
		case DIAGDIRDIFF_REVERSE:
			dir = (DiagDirection)(dir ^ 2);
			break;
		case DIAGDIRDIFF_90RIGHT:
			dir = (DiagDirection)(dir + 1);
			if (dir == DIAGDIR_END) {
				dir = DIAGDIR_NE;
			}
			break;
		default:
			return dir;
	}
	return dir;
}

/**
 * Rotate Axis by an angle using the formula for rotating a point on a plane.
 *
 * @param axis Rotating axis.
 * @param angle Rotate an angle.
 */
Axis Rotate(Axis axis, DiagDirDiff angle) {
	if (angle == DIAGDIRDIFF_90LEFT || angle == DIAGDIRDIFF_90RIGHT) {
		return (Axis)(axis ^ 1);
	}
	return axis;
}

/**
 * Finding an angle using the formula for the angle between two straight lines
 *
 * @param first First line vector.
 * @param second Second line vector.
 */
DiagDirDiff AngleBetweenTwoLines(TileIndexDiffC first, TileIndexDiffC second) {
	if (second.x == 0 && second.y == 0) {
		return DIAGDIRDIFF_SAME;
	}

	first.x = first.x > 0 ? 1 : -1;
	first.y = first.y > 0 ? 1 : -1;
	second.x = second.x > 0 ? 1 : -1;
	second.y = second.y > 0 ? 1 : -1;

	int16_t value = first.x * second.x + first.y * second.y;
	if (value > 0) {
		return DIAGDIRDIFF_SAME;
	} else if (value < 0) {
		return DIAGDIRDIFF_REVERSE;
	}
	TileIndexDiffC third = Rotate(first, DIAGDIRDIFF_90LEFT);

	value = third.x * second.x + third.y * second.y;
	if (value > 0) {
		return DIAGDIRDIFF_90LEFT;
	}
	return DIAGDIRDIFF_90RIGHT;
}

/**
 * Command callback. If the region cannot be inserted, an error message will be displayed.
 * @param result Result of the command.
 * @param tile   Tile where the industry is placed.
 */
void CcCloneArea(Commands, const CommandCost &result, Money, TileIndex tile)
{
	if (result.Succeeded()) {
		if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
	} else {
		SetRedErrorSquare(tile);
	}
}

/**
 * Mark the selected area on the map to copy
 * @param flags for this command type
 * @param tile end tile of area-drag
 * @param start_tile start tile of area drag
 * @param diagonal Whether to use the Orthogonal (false) or Diagonal (true) iterator.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaCopy([[maybe_unused]] DoCommandFlag flags, TileIndex tile, TileIndex start_tile, bool diagonal)
{
	if (start_tile >= Map::Size()) return { CMD_ERROR, 0, INVALID_TILE };

	selected_tile = tile;
	selected_start_tile = start_tile;
	selected_diagonal = diagonal;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	return { cost, 0, tile };
}

/**
 * Paste the selected area on the map
 * @param flags for this command type
 * @param tile end tile of area-drag
 * @param start_tile start tile of area drag
 * @param diagonal Whether to use the Orthogonal (false) or Diagonal (true) iterator.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaPaste(DoCommandFlag flags, TileIndex tile, TileIndex start_tile, bool diagonal)
{
	if (start_tile >= Map::Size()) return { CMD_ERROR, 0, INVALID_TILE };

	Money money = GetAvailableMoneyForCommand();
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error(STR_ERROR_ALREADY_BUILT);
	bool had_success = false;
	bool terraform_problem = false;
	TileIndex terraform_problem_tile = INVALID_TILE;
	CommandCost terraform_error(STR_ERROR_ALREADY_BUILT);

	const Company *c = Company::GetIfValid(_current_company);
	int limit = (c == nullptr ? INT32_MAX : GB(c->terraform_limit, 16, 16));
	if (limit == 0) return { CommandCost(STR_ERROR_TERRAFORM_LIMIT_REACHED), 0, INVALID_TILE };

	TileIndexDiffC origin_direction = TileIndexToTileIndexDiffC(selected_start_tile, selected_tile);
	TileIndexDiffC dest_direction = TileIndexToTileIndexDiffC(start_tile, tile);
	DiagDirDiff angle = AngleBetweenTwoLines(origin_direction, dest_direction);

	int16_t position_selected_tile_x = TileX(selected_start_tile);
	int16_t position_selected_tile_y = TileY(selected_start_tile);
	int16_t dest_position_x = TileX(start_tile);
	int16_t dest_position_y = TileY(start_tile);
	uint origin_main_height = TileHeight(selected_start_tile);
	uint dest_main_height = TileHeight(start_tile);
	uint difference_height = dest_main_height - origin_main_height;

	TileIndexDiffC dest_point;

	TileIndex error_tile = INVALID_TILE;
	std::unique_ptr<TileIterator> iter = TileIterator::Create(selected_start_tile, selected_tile, selected_diagonal);
	for (; *iter != INVALID_TILE; ++(*iter)) {
		TileIndex iter_tile = *iter;

		dest_point.x = TileX(iter_tile) - position_selected_tile_x;
		dest_point.y = TileY(iter_tile) - position_selected_tile_y;
		dest_point = Rotate(dest_point, angle);
		dest_point.x += dest_position_x;
		dest_point.y += dest_position_y;

		TileIndex dest_tile = TileXY(dest_point.x, dest_point.y);

		uint origin_height = TileHeight(iter_tile) + difference_height;
		uint dest_height = TileHeight(dest_tile);
		
		while (dest_height != origin_height) {
			CommandCost ret;
			std::tie(ret, std::ignore, error_tile) = Command<CMD_TERRAFORM_LAND>::Do(flags & ~DC_EXEC, dest_tile, SLOPE_N, dest_height <= origin_height);
			if (ret.Failed()) {
				last_error = ret;

				if (!terraform_problem) {
					terraform_problem_tile = error_tile;
					terraform_error = last_error;
					terraform_problem = true;
				}

				/* Did we reach the limit? */
				if (ret.GetErrorMessage() == STR_ERROR_TERRAFORM_LIMIT_REACHED) limit = 0;
				break;
			}

			if (flags & DC_EXEC) {
				money -= ret.GetCost();
				if (money < 0) {
					return { cost, ret.GetCost(), error_tile };
				}
				Command<CMD_TERRAFORM_LAND>::Do(flags, dest_tile, SLOPE_N, dest_height <= origin_height);
			} else {
				/* When we're at the terraform limit we better bail (unneeded) testing as well.
				 * This will probably cause the terraforming cost to be underestimated, but only
				 * when it's near the terraforming limit. Even then, the estimation is
				 * completely off due to it basically counting terraforming double, so it being
				 * cut off earlier might even give a better estimate in some cases. */
				if (--limit <= 0) {
					had_success = true;
					break;
				}
			}

			cost.AddCost(ret);
			dest_height += (dest_height > origin_height) ? -1 : 1;
			had_success = true;
		}
		if (limit <= 0) break;
	}

	if (terraform_problem) {
		return { terraform_error, 0, terraform_problem_tile };
	}

	CommandCost ret;
	std::tie(ret, std::ignore, error_tile) = CmdCloneAreaPasteProperty(flags, tile, start_tile, diagonal);
	if (ret.Failed()) {
		last_error = ret;
	} else {
		had_success = true;
	}
	if (flags & DC_EXEC) {
		money -= ret.GetCost();
		if (money < 0) {
			return { cost, ret.GetCost(), error_tile };
		}
	}

	CommandCost cc_ret = had_success ? cost : last_error;
	return { cc_ret, 0, cc_ret.Succeeded() ? tile : error_tile };
}


/**
 * Insert structures into the selected area on the map.
 * @param flags operation to perform
 * @param tile end tile of rail conversion drag
 * @param area_start start tile of drag
 * @param diagonal build diagonally or not.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaPasteProperty(DoCommandFlag flags, TileIndex tile, TileIndex area_start, [[maybe_unused]] bool diagonal)
{
	if (area_start >= Map::Size()) return { CMD_ERROR, 0, INVALID_TILE };

	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error(INVALID_STRING_ID);
	bool had_success = false;
	bool auto_remove_signals = true;
	bool signals_copy = true;
	CommandCost error = CommandCost(STR_ERROR_CAN_T_BUILD_HERE);

	TileIndexDiffC origin_direction = TileIndexToTileIndexDiffC(selected_start_tile, selected_tile);
	TileIndexDiffC dest_direction = TileIndexToTileIndexDiffC(area_start, tile);

	DiagDirDiff angle = AngleBetweenTwoLines(origin_direction, dest_direction);
	uint position_selected_tile_x = TileX(selected_start_tile);
	uint position_selected_tile_y = TileY(selected_start_tile);
	int16_t dest_position_x = TileX(area_start);
	int16_t dest_position_y = TileY(area_start);

	TileIndexDiffC dest_point;
	TileIndex iter_tile;
	TileIndex error_tile = INVALID_TILE;

	uint x_max = std::max(position_selected_tile_x, TileX(selected_tile)) - 1;
	uint y_max = std::max(position_selected_tile_y, TileY(selected_tile)) - 1;

	std::map<StationID, StationID> station_map;
	std::unique_ptr<TileIterator> iter = TileIterator::Create(selected_start_tile, selected_tile, selected_diagonal);
	for (; (iter_tile = *iter) != INVALID_TILE; ++(*iter)) {
		if (TileX(iter_tile) > x_max || TileY(iter_tile) > y_max) {
			continue;
		}

		TileType iter_tile_type = GetTileType(iter_tile);
		switch (iter_tile_type) {
			case MP_RAILWAY:
				break;
			case MP_STATION:
				if (!HasStationRail(iter_tile)) continue;
				break;
			case MP_ROAD:
				if (!IsLevelCrossing(iter_tile)) continue;
				break;
			case MP_TUNNELBRIDGE:
				if (GetTunnelBridgeTransportType(iter_tile) != TRANSPORT_RAIL) continue;
				break;
			default: continue;
		}

		dest_point.x = TileX(iter_tile) - position_selected_tile_x;
		dest_point.y = TileY(iter_tile) - position_selected_tile_y;
		dest_point = Rotate(dest_point, angle);
		dest_point = FixAfterRotate(dest_point, angle);
		dest_point.x += dest_position_x;
		dest_point.y += dest_position_y;
		TileIndex dest_tile = TileXY(dest_point.x, dest_point.y);

		CommandCost ret = CheckTileOwnership(iter_tile);
		if (ret.Failed()) {
			error = ret;
			continue;
		}

		RailType railtype2;
		DiagDirection entrance_dir;
		if (iter_tile_type == MP_RAILWAY) {
			switch (GetRailTileType(iter_tile)) {
				case RAIL_TILE_DEPOT:
					railtype2 = GetRailType(iter_tile);
					entrance_dir = GetRailDepotDirection(iter_tile);
					entrance_dir = Rotate(entrance_dir, angle);
					ret = Command<CMD_BUILD_TRAIN_DEPOT>::Do(flags, dest_tile, railtype2, entrance_dir);
					if (ret.Failed()) {
						last_error = ret;
					} else {
						had_success = true;						
						cost.AddCost(ret);
					}
					break;
				default: // RAIL_TILE_NORMAL, RAIL_TILE_SIGNALS
					RailType rail_type = GetRailType(iter_tile);
					TrackBits trackBits = GetTrackBits(iter_tile);
					Track track;
					while ((track = RemoveFirstTrack(&trackBits)) != INVALID_TRACK) {
						Track track_dest = Rotate(track, angle);
						ret = Command<CMD_BUILD_SINGLE_RAIL>::Do(flags, dest_tile, rail_type, track_dest, auto_remove_signals);
						if (ret.Failed()) {
							last_error = ret;
						} else {
							had_success = true;
							cost.AddCost(ret);
						}

						if (signals_copy) {
							if (HasSignalOnTrack(iter_tile, track)) {
								SignalType sigtype = GetSignalType(iter_tile, track);
								SignalVariant sigvar = GetSignalVariant(iter_tile, track);
								ret = Command<CMD_BUILD_SINGLE_SIGNAL>::Do(flags, dest_tile, track_dest, sigtype, sigvar, false, false, false, SIGTYPE_BLOCK, SIGTYPE_BLOCK, 0, 0);
								if (ret.Failed()) {
									last_error = ret;
								} else {
									had_success = true;
									cost.AddCost(ret);
								}
							}
						}
					}
					break;
			}
		}

		if (iter_tile_type == MP_TUNNELBRIDGE) {
			if (IsTunnel(iter_tile)) {
				RailType rail_type = GetRailType(iter_tile);
				ret = Command<CMD_BUILD_TUNNEL>::Do(flags, dest_tile, TRANSPORT_RAIL, rail_type);
				if (ret.Failed()) {
					last_error = ret;
				} else {
					had_success = true;
					cost.AddCost(ret);
				}
			} else {
				RailType rail_type = GetRailType(iter_tile);
				BridgeType type =  GetBridgeType(iter_tile);
				TileIndex endIterTile = GetOtherTunnelBridgeEnd(iter_tile);

				TileIndexDiffC end_dest_point;
				end_dest_point.x = TileX(endIterTile) - position_selected_tile_x;
				end_dest_point.y = TileY(endIterTile) - position_selected_tile_y;
				end_dest_point = Rotate(end_dest_point, angle);
				end_dest_point = FixAfterRotate(end_dest_point, angle);
				end_dest_point.x += dest_position_x;
				end_dest_point.y += dest_position_y;
				TileIndex end_dest_tile = TileXY(end_dest_point.x, end_dest_point.y);

				ret = Command<CMD_BUILD_BRIDGE>::Do(flags, end_dest_tile, dest_tile, TRANSPORT_RAIL, type, rail_type);
				if (ret.Failed()) {
					last_error = ret;
				} else {
					had_success = true;
					cost.AddCost(ret);
				}
			}
		}

		if (iter_tile_type == MP_STATION) {
			if (HasStationRail(iter_tile)) {
				RailType rail_type = GetRailType(iter_tile);
				StationID origin_station_id = GetStationIndex(iter_tile);
				StationID dest_station_id;
				if (station_map.contains(origin_station_id)) {
					dest_station_id = station_map[origin_station_id];
				} else {
					dest_station_id = NEW_STATION;
				}
				Axis origin_axis = GetRailStationAxis(iter_tile);
				Axis dest_axis = Rotate(origin_axis, angle);
				ret = Command<CMD_BUILD_RAIL_STATION>::Do(flags, dest_tile, rail_type, dest_axis, 1, 1, STAT_CLASS_DFLT, STATION_RAIL, dest_station_id, false);
				if (ret.Failed()) {
					last_error = ret;
				} else {
					had_success = true;
					cost.AddCost(ret);
					if (flags & DC_EXEC) {
						if (dest_station_id == NEW_STATION) {
							dest_station_id = GetStationIndex(dest_tile);
							station_map[origin_station_id] = dest_station_id;
						}
						StationGfx station_gfx = GetStationGfx(iter_tile);
						if (origin_axis != dest_axis) {
							ToggleBit(station_gfx, 0);
						}
						if (angle == DIAGDIRDIFF_90RIGHT || angle == DIAGDIRDIFF_REVERSE) {
							ToggleBit(station_gfx, 1);
						}
						SetStationGfx(dest_tile, station_gfx);
					}
				}
			}
		}
	}

	CommandCost cc_ret = had_success ? cost : last_error;
	return { cc_ret, 0, cc_ret.Succeeded() ? tile : error_tile };
}
