/* $Id$ */
/** @file elrail.c
 * This file deals with displaying wires and pylons for electric railways.
 * <h2>Basics</h2>
 *
 * <h3>Tile Types</h3>
 *
 * We have two different types of tiles in the drawing code:
 * Normal Railway Tiles (NRTs) which can have more than one track on it, and
 * Special Railways tiles (SRTs) which have only one track (like crossings, depots
 * stations, etc).
 *
 * <h3>Location Categories</h3>
 *
 * All tiles are categorized into three location groups (TLG):
 * Group 0: Tiles with both an even X coordinate and an even Y coordinate
 * Group 1: Tiles with an even X and an odd Y coordinate
 * Group 2: Tiles with an odd X and an even Y coordinate
 * Group 3: Tiles with both an odd X and Y coordnate.
 *
 * <h3>Pylon Points</h3>
 * <h4>Control Points</h4>
 * A Pylon Control Point (PCP) is a position where a wire (or rather two)
 * is mounted onto a pylon.
 * Each NRT does contain 4 PCPs which are bitmapped to a byte
 * variable and are represented by the DiagDirection enum
 *
 * Each track ends on two PCPs and thus requires one pylon on each end. However,
 * there is one exception: Straight-and-level tracks only have one pylon every
 * other tile.
 *
 * Now on each edge there are two PCPs: One from each adjacent tile. Both PCPs
 * are merged using an OR operation (i. e. if one tile needs a PCP at the postion
 * in question, both tiles get it).
 *
 * <h4>Position Points</h4>
 * A Pylon Position Point (PPP) is a position where a pylon is located on the
 * ground.  Each PCP owns 8 in (45 degree steps) PPPs that are located around
 * it. PPPs are represented using the Direction enum. Each track bit has PPPs
 * that are impossible (because the pylon would be situated on the track) and
 * some that are preferred (because the pylon would be rectangular to the track).
 *
 * <img src="../../elrail_tile.png">
 * <img src="../../elrail_track.png">
 *
 */

#include "stdafx.h"
#include "openttd.h"
#include "station_map.h"
#include "tile.h"
#include "viewport.h"
#include "functions.h" /* We should REALLY get rid of this goddamn file, as it is butt-ugly */
#include "variables.h" /* ... same here */
#include "rail.h"
#include "debug.h"
#include "tunnel_map.h"
#include "road_map.h"
#include "bridge_map.h"
#include "bridge.h"
#include "rail_map.h"
#include "table/sprites.h"
#include "table/elrail_data.h"
#include "vehicle.h"
#include "train.h"
#include "gui.h"

static inline TLG GetTLG(TileIndex t)
{
	return (HASBIT(TileX(t), 0) << 1) + HASBIT(TileY(t), 0);
}

/** Finds which Rail Bits are present on a given tile. For bridge tiles,
 * returns track bits under the bridge
 */
static TrackBits GetRailTrackBitsUniversal(TileIndex t, byte *override)
{
	switch (GetTileType(t)) {
		case MP_RAILWAY:
			if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
			switch (GetRailTileType(t)) {
				case RAIL_TILE_NORMAL: case RAIL_TILE_SIGNALS:
					return GetTrackBits(t);
				case RAIL_TILE_DEPOT_WAYPOINT:
					if (GetRailTileSubtype(t) == RAIL_SUBTYPE_WAYPOINT) return GetRailWaypointBits(t);
				default:
					return 0;
			}
			break;

		case MP_TUNNELBRIDGE:
			if (IsTunnel(t)) {
				if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
				if (override != NULL) *override = 1 << GetTunnelDirection(t);
				return AxisToTrackBits(DiagDirToAxis(GetTunnelDirection(t)));
			} else {
				if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
				if (IsBridgeMiddle(t)) {
					if (IsTransportUnderBridge(t) &&
						GetTransportTypeUnderBridge(t) == TRANSPORT_RAIL) {
						return GetRailBitsUnderBridge(t);
					} else {
						return 0;
					}
				} else {
					if (override != NULL && DistanceMax(t, GetOtherBridgeEnd(t)) > 1) *override = 1 << GetBridgeRampDirection(t);

					return AxisToTrackBits(DiagDirToAxis(GetBridgeRampDirection(t)));
				}
			}

		case MP_STREET:
			if (GetRoadTileType(t) != ROAD_TILE_CROSSING) return 0;
			if (GetRailTypeCrossing(t) != RAILTYPE_ELECTRIC) return 0;
			return GetCrossingRailBits(t);

		case MP_STATION:
			if (!IsRailwayStation(t)) return 0;
			if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
			if (!IsStationTileElectrifiable(t)) return 0;
			return TrackToTrackBits(GetRailStationTrack(t));

		default:
			return 0;
	}
}

/** Corrects the tileh for certain tile types. Returns an effective tileh for the track on the tile.
 * @param tile The tile to analyse
 * @param *tileh the tileh
 */
static void AdjustTileh(TileIndex tile, Slope *tileh)
{
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (IsTunnel(tile)) {
			*tileh = SLOPE_STEEP; /* XXX - Hack to make tunnel entrances to always have a pylon */
		} else {
			if (IsBridgeRamp(tile)) {
				if (*tileh != SLOPE_FLAT) {
					*tileh = SLOPE_FLAT;
				} else {
					switch (GetBridgeRampDirection(tile)) {
						case DIAGDIR_NE: *tileh = SLOPE_NE; break;
						case DIAGDIR_SE: *tileh = SLOPE_SE; break;
						case DIAGDIR_SW: *tileh = SLOPE_SW; break;
						case DIAGDIR_NW: *tileh = SLOPE_NW; break;
						default: break;
					}
				}
			}
		}
	}
}

/** Draws wires and, if required, pylons on a given tile
 * @param ti The Tileinfo to draw the tile for
 */
static void DrawCatenaryRailway(const TileInfo *ti)
{
	/* Pylons are placed on a tile edge, so we need to take into account
	 * the track configuration of 2 adjacent tiles. trackconfig[0] stores the
	 * current tile (home tile) while [1] holds the neighbour */
	TrackBits trackconfig[TS_END];
	bool isflat[TS_END];
	/* Note that ti->tileh has already been adjusted for Foundations */
	Slope tileh[TS_END] = { ti->tileh, SLOPE_FLAT };

	TLG tlg = GetTLG(ti->tile);
	byte PCPstatus = 0;
	byte OverridePCP = 0;
	byte PPPpreferred[DIAGDIR_END];
	byte PPPallowed[DIAGDIR_END];
	DiagDirection i;
	Track t;

	/* Find which rail bits are present, and select the override points.
	 * We don't draw a pylon:
	 * 1) INSIDE a tunnel (we wouldn't see it anyway)
	 * 2) on the "far" end of a bridge head (the one that connects to bridge middle),
	 *    because that one is drawn on the bridge. Exception is for length 0 bridges
	 *    which have no middle tiles */
	trackconfig[TS_HOME] = GetRailTrackBitsUniversal(ti->tile, &OverridePCP);
	/* If a track bit is present that is not in the main direction, the track is level */
	isflat[TS_HOME] = trackconfig[TS_HOME] & (TRACK_BIT_HORZ | TRACK_BIT_VERT);

	AdjustTileh(ti->tile, &tileh[TS_HOME]);

	for (i = DIAGDIR_NE; i < DIAGDIR_END; i++) {
		TileIndex neighbour = ti->tile + TileOffsByDiagDir(i);
		uint foundation = 0;
		int k;

		/* Here's one of the main headaches. GetTileSlope does not correct for possibly
		 * existing foundataions, so we do have to do that manually later on.*/
		tileh[TS_NEIGHBOUR] = GetTileSlope(neighbour, NULL);
		trackconfig[TS_NEIGHBOUR] = GetRailTrackBitsUniversal(neighbour, NULL);
		if (IsTunnelTile(neighbour) && i != GetTunnelDirection(neighbour)) trackconfig[TS_NEIGHBOUR] = 0;
		isflat[TS_NEIGHBOUR] = trackconfig[TS_NEIGHBOUR] & (TRACK_BIT_HORZ | TRACK_BIT_VERT);

		PPPpreferred[i] = 0xFF; /* We start with preferring everything (end-of-line in any direction) */
		PPPallowed[i] = AllowedPPPonPCP[i];

		/* We cycle through all the existing tracks at a PCP and see what
		 * PPPs we want to have, or may not have at all */
		for (k = 0; k < NUM_TRACKS_AT_PCP; k++) {
			/* Next to us, we have a bridge head, don't worry about that one, if it shows away from us */
			if (TrackSourceTile[i][k] == TS_NEIGHBOUR &&
			    IsBridgeTile(neighbour) && IsBridgeRamp(neighbour) &&
			    GetBridgeRampDirection(neighbour) == ReverseDiagDir(i)) {
				continue;
			}

			/* We check whether the track in question (k) is present in the tile
			 * (TrackSourceTile) */
			if (HASBIT(trackconfig[TrackSourceTile[i][k]], TracksAtPCP[i][k])) {
				/* track found, if track is in the neighbour tile, adjust the number
				 * of the PCP for preferred/allowed determination*/
				DiagDirection PCPpos = (TrackSourceTile[i][k] == TS_HOME) ? i : ReverseDiagDir(i);
				SETBIT(PCPstatus, i); /* This PCP is in use */

				PPPpreferred[i] &= PreferredPPPofTrackAtPCP[TracksAtPCP[i][k]][PCPpos];
				PPPallowed[i] &= ~DisallowedPPPofTrackAtPCP[TracksAtPCP[i][k]][PCPpos];
			}
		}

		/* Deactivate all PPPs if PCP is not used */
		PPPpreferred[i] *= HASBIT(PCPstatus, i);
		PPPallowed[i] *= HASBIT(PCPstatus, i);

		/* A station is always "flat", so adjust the tileh accordingly */
		if (IsTileType(neighbour, MP_STATION)) tileh[TS_NEIGHBOUR] = SLOPE_FLAT;

		/* Read the foundataions if they are present, and adjust the tileh */
		if (IsTileType(neighbour, MP_RAILWAY) && GetRailType(neighbour) == RAILTYPE_ELECTRIC) foundation = GetRailFoundation(tileh[TS_NEIGHBOUR], trackconfig[TS_NEIGHBOUR]);
		if (IsBridgeTile(neighbour) && IsBridgeRamp(neighbour)) {
			foundation = GetBridgeFoundation(tileh[TS_NEIGHBOUR], DiagDirToAxis(GetBridgeRampDirection(neighbour)));
		}

		if (foundation != 0) {
			if (foundation < 15) {
				tileh[TS_NEIGHBOUR] = SLOPE_FLAT;
			} else {
				tileh[TS_NEIGHBOUR] = _inclined_tileh[foundation - 15];
			}
		}

		AdjustTileh(neighbour, &tileh[TS_NEIGHBOUR]);

		/* If we have a straight (and level) track, we want a pylon only every 2 tiles
		 * Delete the PCP if this is the case. */
		/* Level means that the slope is the same, or the track is flat */
		if (tileh[TS_HOME] == tileh[TS_NEIGHBOUR] || (isflat[TS_HOME] && isflat[TS_NEIGHBOUR])) {
			for (k = 0; k < NUM_IGNORE_GROUPS; k++)
				if (PPPpreferred[i] == IgnoredPCP[k][tlg][i]) CLRBIT(PCPstatus, i);
		}

		/* Now decide where we draw our pylons. First try the preferred PPPs, but they may not exist.
		 * In that case, we try the any of the allowed ones. if they don't exist either, don't draw
		 * anything. Note that the preferred PPPs still contain the end-of-line markers.
		 * Remove those (simply by ANDing with allowed, since these markers are never allowed) */
		if ((PPPallowed[i] & PPPpreferred[i]) != 0) PPPallowed[i] &= PPPpreferred[i];

		if (PPPallowed[i] != 0 && HASBIT(PCPstatus, i) && !HASBIT(OverridePCP, i)) {
			for (k = 0; k < DIR_END; k++) {
				byte temp = PPPorder[i][GetTLG(ti->tile)][k];

				if (HASBIT(PPPallowed[i], temp)) {
					uint x  = ti->x + x_pcp_offsets[i] + x_ppp_offsets[temp];
					uint y  = ti->y + y_pcp_offsets[i] + y_ppp_offsets[temp];

					/* Don't build the pylon if it would be outside the tile */
					if (!HASBIT(OwnedPPPonPCP[i], temp)) {
						/* We have a neighour that will draw it, bail out */
						if (trackconfig[TS_NEIGHBOUR] != 0) break;
						continue; /* No neighbour, go looking for a better position */
					}

					AddSortableSpriteToDraw(pylons_normal[temp], x, y, 1, 1, 10,
							GetSlopeZ(ti->x + x_pcp_offsets[i], ti->y + y_pcp_offsets[i]));
					break; /* We already have drawn a pylon, bail out */
				}
			}
		}
	}

	/* Don't draw a wire under a low bridge */
	if (IsBridgeTile(ti->tile) &&
			IsBridgeMiddle(ti->tile) &&
			!(_display_opt & DO_TRANS_BUILDINGS) &&
			GetBridgeHeight(ti->tile) <= TilePixelHeight(ti->tile) + TILE_HEIGHT) {
		return;
	}

	/* Drawing of pylons is finished, now draw the wires */
	for (t = 0; t < TRACK_END; t++) {
		if (HASBIT(trackconfig[TS_HOME], t)) {
			byte PCPconfig;
			const SortableSpriteStruct *sss;
			int tileh_selector;

			if (IsTunnelTile(ti->tile)) {
				const SortableSpriteStruct* sss = &CatenarySpriteData_Tunnel[GetTunnelDirection(ti->tile)];

				AddSortableSpriteToDraw(
					sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
					sss->x_size, sss->y_size, sss->z_size,
					GetTileZ(ti->tile) + sss->z_offset
				);
				break;
			}
			PCPconfig = HASBIT(PCPstatus, PCPpositions[t][0]) +
				(HASBIT(PCPstatus, PCPpositions[t][1]) << 1);

			tileh_selector = !(tileh[TS_HOME] % 3) * tileh[TS_HOME] / 3; /* tileh for the slopes, 0 otherwise */

			assert(PCPconfig != 0); /* We have a pylon on neither end of the wire, that doesn't work (since we have no sprites for that) */
			assert(!IsSteepSlope(tileh[TS_HOME]));
			sss = &CatenarySpriteData[Wires[tileh_selector][t][PCPconfig]];

			AddSortableSpriteToDraw( sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
				sss->x_size, sss->y_size, sss->z_size, GetSlopeZ(ti->x + min(sss->x_offset, TILE_SIZE - 1), ti->y + min(sss->y_offset, TILE_SIZE - 1)) + sss->z_offset);
		}
	}
}

static void DrawCatenaryOnBridge(const TileInfo *ti)
{
	TileIndex end = GetSouthernBridgeEnd(ti->tile);
	TileIndex start = GetOtherBridgeEnd(end);

	uint length = GetBridgeLength(start, end);
	uint num = DistanceMax(ti->tile, start);
	uint height;

	const SortableSpriteStruct *sss;
	Axis axis = GetBridgeAxis(ti->tile);
	TLG tlg = GetTLG(ti->tile);

	CatenarySprite offset = axis == AXIS_X ? 0 : WIRE_Y_FLAT_BOTH - WIRE_X_FLAT_BOTH;

	if ((length % 2) && num == length) {
		/* Draw the "short" wire on the southern end of the bridge
		 * only needed if the length of the bridge is odd */
		sss = &CatenarySpriteData[WIRE_X_FLAT_BOTH + offset];
	} else {
		/* Draw "long" wires on all other tiles of the bridge (one pylon every two tiles) */
		sss = &CatenarySpriteData[WIRE_X_FLAT_SW + (num % 2) + offset];
	}

	height = GetBridgeHeight(ti->tile);

	AddSortableSpriteToDraw( sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
		sss->x_size, sss->y_size, sss->z_size, height + sss->z_offset
	);

	/* Finished with wires, draw pylons */
	/* every other tile needs a pylon on the northern end */
	if (num % 2) {
		if (axis == AXIS_X) {
			AddSortableSpriteToDraw(pylons_bridge[0 + HASBIT(tlg, 0)], ti->x, ti->y + 4 + 8 * HASBIT(tlg, 0), 1, 1, 10, height);
		} else {
			AddSortableSpriteToDraw(pylons_bridge[2 + HASBIT(tlg, 1)], ti->x + 4 + 8 * HASBIT(tlg, 1), ti->y, 1, 1, 10, height);
		}
	}

	/* need a pylon on the southern end of the bridge */
	if (DistanceMax(ti->tile, start) == length) {
		if (axis == AXIS_X) {
			AddSortableSpriteToDraw(pylons_bridge[0 + HASBIT(tlg, 0)], ti->x + 16, ti->y + 4 + 8 * HASBIT(tlg, 0), 1, 1, 10, height);
		} else {
			AddSortableSpriteToDraw(pylons_bridge[2 + HASBIT(tlg, 1)], ti->x + 4 + 8 * HASBIT(tlg, 1), ti->y + 16, 1, 1, 10, height);
		}
	}
}

void DrawCatenary(const TileInfo *ti)
{
	if (_patches.disable_elrails) return;

	switch (GetTileType(ti->tile)) {
		case MP_RAILWAY:
			if ( IsRailDepot(ti->tile)) {
				const SortableSpriteStruct* sss = &CatenarySpriteData_Depot[GetRailDepotDirection(ti->tile)];

				AddSortableSpriteToDraw(
					sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
					sss->x_size, sss->y_size, sss->z_size,
					GetTileMaxZ(ti->tile) + sss->z_offset
				);
				return;
			}
			break;

		case MP_TUNNELBRIDGE:
			if (IsBridge(ti->tile) && IsBridgeMiddle(ti->tile) && GetRailTypeOnBridge(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenaryOnBridge(ti);
			break;

		case MP_STREET:  break;
		case MP_STATION: break;

		default: return;
	}
	DrawCatenaryRailway(ti);
}

int32 SettingsDisableElrail(int32 p1)
{
	EngineID e_id;
	Vehicle* v;
	Player *p;
	bool disable = (p1 != 0);

	/* we will now walk through all electric train engines and change their railtypes if it is the wrong one*/
	const RailType old_railtype = disable ? RAILTYPE_ELECTRIC : RAILTYPE_RAIL;
	const RailType new_railtype = disable ? RAILTYPE_RAIL : RAILTYPE_ELECTRIC;

	/* walk through all train engines */
	for (e_id = 0; e_id < NUM_TRAIN_ENGINES; e_id++) {
		const RailVehicleInfo *rv_info = RailVehInfo(e_id);
		Engine *e = GetEngine(e_id);
		/* if it is an electric rail engine and its railtype is the wrong one */
		if (rv_info->engclass == 2 && e->railtype == old_railtype) {
			/* change it to the proper one */
			e->railtype = new_railtype;
			_engine_info[e_id].railtype = new_railtype;
		}
	}

	/* when disabling elrails, make sure that all existing trains can run on
	*  normal rail too */
	if (disable) {
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Train && v->u.rail.railtype == RAILTYPE_ELECTRIC) {
				/* this railroad vehicle is now compatible only with elrail,
				*  so add there also normal rail compatibility */
				v->u.rail.compatible_railtypes |= (1 << RAILTYPE_RAIL);
				v->u.rail.railtype = RAILTYPE_RAIL;
				SETBIT(v->u.rail.flags, VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL);
			}
		}
	}

	/* setup total power for trains */
	FOR_ALL_VEHICLES(v) {
		/* power is cached only for front engines */
		if (v->type == VEH_Train && IsFrontEngine(v)) TrainPowerChanged(v);
	}

	FOR_ALL_PLAYERS(p) p->avail_railtypes = GetPlayerRailtypes(p->index);

	/* This resets the _last_built_railtype, which will be invalid for electric
	* rails. It may have unintended consequences if that function is ever
	* extended, though. */
	ReinitGuiAfterToggleElrail(disable);
	return 0;
}
