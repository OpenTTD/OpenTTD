/* $Id$ */
/** @file elrail.cpp
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
#include "viewport_func.h"
#include "settings_type.h"
#include "landscape.h"
#include "rail_type.h"
#include "debug.h"
#include "tunnel_map.h"
#include "road_map.h"
#include "bridge_map.h"
#include "bridge.h"
#include "rail_map.h"
#include "train.h"
#include "rail_gui.h"
#include "transparency.h"
#include "tunnelbridge_map.h"
#include "vehicle_func.h"
#include "player_base.h"
#include "tunnelbridge.h"
#include "engine_func.h"

#include "table/sprites.h"
#include "table/elrail_data.h"

static inline TLG GetTLG(TileIndex t)
{
	return (TLG)((HasBit(TileX(t), 0) << 1) + HasBit(TileY(t), 0));
}

/**
 * Finds which Electrified Rail Bits are present on a given tile.
 * @param t tile to check
 * @param override pointer to PCP override, can be NULL
 * @return trackbits of tile if it is electrified
 */
static TrackBits GetRailTrackBitsUniversal(TileIndex t, byte *override)
{
	switch (GetTileType(t)) {
		case MP_RAILWAY:
			if (!HasCatenary(GetRailType(t))) return TRACK_BIT_NONE;
			switch (GetRailTileType(t)) {
				case RAIL_TILE_NORMAL: case RAIL_TILE_SIGNALS:
					return GetTrackBits(t);
				case RAIL_TILE_WAYPOINT:
					return GetRailWaypointBits(t);
				default:
					return TRACK_BIT_NONE;
			}
			break;

		case MP_TUNNELBRIDGE:
			if (!HasCatenary(GetRailType(t))) return TRACK_BIT_NONE;
			if (override != NULL && (IsTunnel(t) || GetTunnelBridgeLength(t, GetOtherBridgeEnd(t)) > 0)) {
				*override = 1 << GetTunnelBridgeDirection(t);
			}
			return AxisToTrackBits(DiagDirToAxis(GetTunnelBridgeDirection(t)));

		case MP_ROAD:
			if (!IsLevelCrossing(t)) return TRACK_BIT_NONE;
			if (!HasCatenary(GetRailType(t))) return TRACK_BIT_NONE;
			return GetCrossingRailBits(t);

		case MP_STATION:
			if (!IsRailwayStation(t)) return TRACK_BIT_NONE;
			if (!HasCatenary(GetRailType(t))) return TRACK_BIT_NONE;
			if (!IsStationTileElectrifiable(t)) return TRACK_BIT_NONE;
			return TrackToTrackBits(GetRailStationTrack(t));

		default:
			return TRACK_BIT_NONE;
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
			*tileh = SLOPE_STEEP; // XXX - Hack to make tunnel entrances to always have a pylon
		} else if (*tileh != SLOPE_FLAT) {
			*tileh = SLOPE_FLAT;
		} else {
			*tileh = InclinedSlope(GetTunnelBridgeDirection(tile));
		}
	}
}

/**
 * Returns the Z position of a Pylon Control Point.
 *
 * @param tile The tile the pylon should stand on.
 * @param PCPpos The PCP of the tile.
 * @return The Z position of the PCP.
 */
static byte GetPCPElevation(TileIndex tile, DiagDirection PCPpos)
{
	/* The elevation of the "pylon"-sprite should be the elevation at the PCP.
	 * PCPs are always on a tile edge.
	 *
	 * This position can be outside of the tile, i.e. ?_pcp_offset == TILE_SIZE > TILE_SIZE - 1.
	 * So we have to move it inside the tile, because if the neighboured tile has a foundation,
	 * that does not smoothly connect to the current tile, we will get a wrong elevation from GetSlopeZ().
	 *
	 * When we move the position inside the tile, we will get a wrong elevation if we have a slope.
	 * To catch all cases we round the Z position to the next (TILE_HEIGHT / 2).
	 * This will return the correct elevation for slopes and will also detect non-continuous elevation on edges.
	 *
	 * Also note that the result of GetSlopeZ() is very special on bridge-ramps.
	 */

	byte z = GetSlopeZ(TileX(tile) * TILE_SIZE + min(x_pcp_offsets[PCPpos], TILE_SIZE - 1), TileY(tile) * TILE_SIZE + min(y_pcp_offsets[PCPpos], TILE_SIZE - 1));
	return (z + 2) & ~3; // this means z = (z + TILE_HEIGHT / 4) / (TILE_HEIGHT / 2) * (TILE_HEIGHT / 2);
}

/**
 * Draws wires on a tunnel tile
 *
 * DrawTile_TunnelBridge() calls this function to draw the wires as SpriteCombine with the tunnel roof.
 *
 * @param ti The Tileinfo to draw the tile for
 */
void DrawCatenaryOnTunnel(const TileInfo *ti)
{
	/* xmin, ymin, xmax + 1, ymax + 1 of BB */
	static const int _tunnel_wire_BB[4][4] = {
		{ 0, 1, 16, 15 }, // NE
		{ 1, 0, 15, 16 }, // SE
		{ 0, 1, 16, 15 }, // SW
		{ 1, 0, 15, 16 }, // NW
	};

	if (!HasCatenary(GetRailType(ti->tile)) || _patches.disable_elrails) return;

	DiagDirection dir = GetTunnelBridgeDirection(ti->tile);

	const SortableSpriteStruct *sss = &CatenarySpriteData_Tunnel[dir];
	const int *BB_data = _tunnel_wire_BB[dir];
	AddSortableSpriteToDraw(
		sss->image, PAL_NONE, ti->x + sss->x_offset, ti->y + sss->y_offset,
		BB_data[2] - sss->x_offset, BB_data[3] - sss->y_offset, BB_Z_SEPARATOR - sss->z_offset + 1,
		GetTileZ(ti->tile) + sss->z_offset,
		IsTransparencySet(TO_CATENARY),
		BB_data[0] - sss->x_offset, BB_data[1] - sss->y_offset, BB_Z_SEPARATOR - sss->z_offset
	);
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

	/* Half tile slopes coincide only with horizontal/vertical track.
	 * Faking a flat slope results in the correct sprites on positions. */
	if (IsHalftileSlope(tileh[TS_HOME])) tileh[TS_HOME] = SLOPE_FLAT;

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
	isflat[TS_HOME] = ((trackconfig[TS_HOME] & (TRACK_BIT_HORZ | TRACK_BIT_VERT)) != 0);

	AdjustTileh(ti->tile, &tileh[TS_HOME]);

	for (i = DIAGDIR_NE; i < DIAGDIR_END; i++) {
		TileIndex neighbour = ti->tile + TileOffsByDiagDir(i);
		Foundation foundation = FOUNDATION_NONE;
		int k;

		/* Here's one of the main headaches. GetTileSlope does not correct for possibly
		 * existing foundataions, so we do have to do that manually later on.*/
		tileh[TS_NEIGHBOUR] = GetTileSlope(neighbour, NULL);
		trackconfig[TS_NEIGHBOUR] = GetRailTrackBitsUniversal(neighbour, NULL);
		if (IsTunnelTile(neighbour) && i != GetTunnelBridgeDirection(neighbour)) trackconfig[TS_NEIGHBOUR] = TRACK_BIT_NONE;

		/* If the neighboured tile does not smoothly connect to the current tile (because of a foundation),
		 * we have to draw all pillars on the current tile. */
		if (GetPCPElevation(ti->tile, i) != GetPCPElevation(neighbour, ReverseDiagDir(i))) trackconfig[TS_NEIGHBOUR] = TRACK_BIT_NONE;

		isflat[TS_NEIGHBOUR] = ((trackconfig[TS_NEIGHBOUR] & (TRACK_BIT_HORZ | TRACK_BIT_VERT)) != 0);

		PPPpreferred[i] = 0xFF; // We start with preferring everything (end-of-line in any direction)
		PPPallowed[i] = AllowedPPPonPCP[i];

		/* We cycle through all the existing tracks at a PCP and see what
		 * PPPs we want to have, or may not have at all */
		for (k = 0; k < NUM_TRACKS_AT_PCP; k++) {
			/* Next to us, we have a bridge head, don't worry about that one, if it shows away from us */
			if (TrackSourceTile[i][k] == TS_NEIGHBOUR &&
			    IsBridgeTile(neighbour) &&
			    GetTunnelBridgeDirection(neighbour) == ReverseDiagDir(i)) {
				continue;
			}

			/* We check whether the track in question (k) is present in the tile
			 * (TrackSourceTile) */
			if (HasBit(trackconfig[TrackSourceTile[i][k]], TracksAtPCP[i][k])) {
				/* track found, if track is in the neighbour tile, adjust the number
				 * of the PCP for preferred/allowed determination*/
				DiagDirection PCPpos = (TrackSourceTile[i][k] == TS_HOME) ? i : ReverseDiagDir(i);
				SetBit(PCPstatus, i); // This PCP is in use

				PPPpreferred[i] &= PreferredPPPofTrackAtPCP[TracksAtPCP[i][k]][PCPpos];
				PPPallowed[i] &= ~DisallowedPPPofTrackAtPCP[TracksAtPCP[i][k]][PCPpos];
			}
		}

		/* Deactivate all PPPs if PCP is not used */
		PPPpreferred[i] *= HasBit(PCPstatus, i);
		PPPallowed[i] *= HasBit(PCPstatus, i);

		/* A station is always "flat", so adjust the tileh accordingly */
		if (IsTileType(neighbour, MP_STATION)) tileh[TS_NEIGHBOUR] = SLOPE_FLAT;

		/* Read the foundataions if they are present, and adjust the tileh */
		if (trackconfig[TS_NEIGHBOUR] != TRACK_BIT_NONE && IsTileType(neighbour, MP_RAILWAY) && HasCatenary(GetRailType(neighbour))) foundation = GetRailFoundation(tileh[TS_NEIGHBOUR], trackconfig[TS_NEIGHBOUR]);
		if (IsBridgeTile(neighbour)) {
			foundation = GetBridgeFoundation(tileh[TS_NEIGHBOUR], DiagDirToAxis(GetTunnelBridgeDirection(neighbour)));
		}

		ApplyFoundationToSlope(foundation, &tileh[TS_NEIGHBOUR]);

	/* Half tile slopes coincide only with horizontal/vertical track.
	 * Faking a flat slope results in the correct sprites on positions. */
		if (IsHalftileSlope(tileh[TS_NEIGHBOUR])) tileh[TS_NEIGHBOUR] = SLOPE_FLAT;

		AdjustTileh(neighbour, &tileh[TS_NEIGHBOUR]);

		/* If we have a straight (and level) track, we want a pylon only every 2 tiles
		 * Delete the PCP if this is the case. */
		/* Level means that the slope is the same, or the track is flat */
		if (tileh[TS_HOME] == tileh[TS_NEIGHBOUR] || (isflat[TS_HOME] && isflat[TS_NEIGHBOUR])) {
			for (k = 0; k < NUM_IGNORE_GROUPS; k++)
				if (PPPpreferred[i] == IgnoredPCP[k][tlg][i]) ClrBit(PCPstatus, i);
		}

		/* Now decide where we draw our pylons. First try the preferred PPPs, but they may not exist.
		 * In that case, we try the any of the allowed ones. if they don't exist either, don't draw
		 * anything. Note that the preferred PPPs still contain the end-of-line markers.
		 * Remove those (simply by ANDing with allowed, since these markers are never allowed) */
		if ((PPPallowed[i] & PPPpreferred[i]) != 0) PPPallowed[i] &= PPPpreferred[i];

		if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile)) {
			Track bridgetrack = GetBridgeAxis(ti->tile) == AXIS_X ? TRACK_X : TRACK_Y;
			uint height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));

			if ((height <= GetTileMaxZ(ti->tile) + TILE_HEIGHT) &&
					(i == PCPpositions[bridgetrack][0] || i == PCPpositions[bridgetrack][1])) {
				SetBit(OverridePCP, i);
			}
		}

		if (PPPallowed[i] != 0 && HasBit(PCPstatus, i) && !HasBit(OverridePCP, i)) {
			for (k = 0; k < DIR_END; k++) {
				byte temp = PPPorder[i][GetTLG(ti->tile)][k];

				if (HasBit(PPPallowed[i], temp)) {
					uint x  = ti->x + x_pcp_offsets[i] + x_ppp_offsets[temp];
					uint y  = ti->y + y_pcp_offsets[i] + y_ppp_offsets[temp];

					/* Don't build the pylon if it would be outside the tile */
					if (!HasBit(OwnedPPPonPCP[i], temp)) {
						/* We have a neighour that will draw it, bail out */
						if (trackconfig[TS_NEIGHBOUR] != 0) break;
						continue; /* No neighbour, go looking for a better position */
					}

					AddSortableSpriteToDraw(pylon_sprites[temp], PAL_NONE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE,
							GetPCPElevation(ti->tile, i),
							IsTransparencySet(TO_CATENARY), -1, -1);
					break; /* We already have drawn a pylon, bail out */
				}
			}
		}
	}

	/* Don't draw a wire under a low bridge */
	if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile) && !IsTransparencySet(TO_CATENARY)) {
		uint height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));

		if (height <= GetTileMaxZ(ti->tile) + TILE_HEIGHT) return;
	}

	/* Drawing of pylons is finished, now draw the wires */
	for (t = TRACK_BEGIN; t < TRACK_END; t++) {
		if (HasBit(trackconfig[TS_HOME], t)) {
			if (IsTunnelTile(ti->tile)) break; // drawn together with tunnel-roof (see DrawCatenaryOnTunnel())
			byte PCPconfig = HasBit(PCPstatus, PCPpositions[t][0]) +
				(HasBit(PCPstatus, PCPpositions[t][1]) << 1);

			const SortableSpriteStruct *sss;
			int tileh_selector = !(tileh[TS_HOME] % 3) * tileh[TS_HOME] / 3; /* tileh for the slopes, 0 otherwise */

			assert(PCPconfig != 0); /* We have a pylon on neither end of the wire, that doesn't work (since we have no sprites for that) */
			assert(!IsSteepSlope(tileh[TS_HOME]));
			sss = &CatenarySpriteData[Wires[tileh_selector][t][PCPconfig]];

			/*
			 * The "wire"-sprite position is inside the tile, i.e. 0 <= sss->?_offset < TILE_SIZE.
			 * Therefore it is save to use GetSlopeZ() for the elevation.
			 * Also note, that the result of GetSlopeZ() is very special for bridge-ramps.
			 */
			AddSortableSpriteToDraw(sss->image, PAL_NONE, ti->x + sss->x_offset, ti->y + sss->y_offset,
				sss->x_size, sss->y_size, sss->z_size, GetSlopeZ(ti->x + sss->x_offset, ti->y + sss->y_offset) + sss->z_offset,
				IsTransparencySet(TO_CATENARY));
		}
	}
}

void DrawCatenaryOnBridge(const TileInfo *ti)
{
	if (_patches.disable_elrails) return;

	/* Do not draw catenary if it is invisible */
	if (IsInvisibilitySet(TO_CATENARY)) return;

	TileIndex end = GetSouthernBridgeEnd(ti->tile);
	TileIndex start = GetOtherBridgeEnd(end);

	uint length = GetTunnelBridgeLength(start, end);
	uint num = GetTunnelBridgeLength(ti->tile, start) + 1;
	uint height;

	const SortableSpriteStruct *sss;
	Axis axis = GetBridgeAxis(ti->tile);
	TLG tlg = GetTLG(ti->tile);

	CatenarySprite offset = (CatenarySprite)(axis == AXIS_X ? 0 : WIRE_Y_FLAT_BOTH - WIRE_X_FLAT_BOTH);

	if ((length % 2) && num == length) {
		/* Draw the "short" wire on the southern end of the bridge
		 * only needed if the length of the bridge is odd */
		sss = &CatenarySpriteData[WIRE_X_FLAT_BOTH + offset];
	} else {
		/* Draw "long" wires on all other tiles of the bridge (one pylon every two tiles) */
		sss = &CatenarySpriteData[WIRE_X_FLAT_SW + (num % 2) + offset];
	}

	height = GetBridgeHeight(end);

	AddSortableSpriteToDraw(sss->image, PAL_NONE, ti->x + sss->x_offset, ti->y + sss->y_offset,
		sss->x_size, sss->y_size, sss->z_size, height + sss->z_offset,
		IsTransparencySet(TO_CATENARY)
	);

	/* Finished with wires, draw pylons */
	/* every other tile needs a pylon on the northern end */
	if (num % 2) {
		DiagDirection PCPpos = (axis == AXIS_X ? DIAGDIR_NE : DIAGDIR_NW);
		Direction PPPpos = (axis == AXIS_X ? DIR_NW : DIR_NE);
		if (HasBit(tlg, (axis == AXIS_X ? 0 : 1))) PPPpos = ReverseDir(PPPpos);
		uint x = ti->x + x_pcp_offsets[PCPpos] + x_ppp_offsets[PPPpos];
		uint y = ti->y + y_pcp_offsets[PCPpos] + y_ppp_offsets[PPPpos];
		AddSortableSpriteToDraw(pylon_sprites[PPPpos], PAL_NONE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, height, IsTransparencySet(TO_CATENARY), -1, -1);
	}

	/* need a pylon on the southern end of the bridge */
	if (GetTunnelBridgeLength(ti->tile, start) + 1 == length) {
		DiagDirection PCPpos = (axis == AXIS_X ? DIAGDIR_SW : DIAGDIR_SE);
		Direction PPPpos = (axis == AXIS_X ? DIR_NW : DIR_NE);
		if (HasBit(tlg, (axis == AXIS_X ? 0 : 1))) PPPpos = ReverseDir(PPPpos);
		uint x = ti->x + x_pcp_offsets[PCPpos] + x_ppp_offsets[PPPpos];
		uint y = ti->y + y_pcp_offsets[PCPpos] + y_ppp_offsets[PPPpos];
		AddSortableSpriteToDraw(pylon_sprites[PPPpos], PAL_NONE, x, y, 1, 1, BB_HEIGHT_UNDER_BRIDGE, height, IsTransparencySet(TO_CATENARY), -1, -1);
	}
}

void DrawCatenary(const TileInfo *ti)
{
	if (_patches.disable_elrails) return;

	/* Do not draw catenary if it is invisible */
	if (IsInvisibilitySet(TO_CATENARY)) return;

	switch (GetTileType(ti->tile)) {
		case MP_RAILWAY:
			if (IsRailDepot(ti->tile)) {
				const SortableSpriteStruct *sss = &CatenarySpriteData_Depot[GetRailDepotDirection(ti->tile)];

				/* This wire is not visible with the default depot sprites */
				AddSortableSpriteToDraw(
					sss->image, PAL_NONE, ti->x + sss->x_offset, ti->y + sss->y_offset,
					sss->x_size, sss->y_size, sss->z_size,
					GetTileMaxZ(ti->tile) + sss->z_offset,
					IsTransparencySet(TO_CATENARY)
				);
				return;
			}
			break;

		case MP_TUNNELBRIDGE:
		case MP_ROAD:
		case MP_STATION:
			break;

		default: return;
	}
	DrawCatenaryRailway(ti);
}

int32 SettingsDisableElrail(int32 p1)
{
	Vehicle *v;
	Player *p;
	bool disable = (p1 != 0);

	/* we will now walk through all electric train engines and change their railtypes if it is the wrong one*/
	const RailType old_railtype = disable ? RAILTYPE_ELECTRIC : RAILTYPE_RAIL;
	const RailType new_railtype = disable ? RAILTYPE_RAIL : RAILTYPE_ELECTRIC;

	/* walk through all train engines */
	EngineID eid;
	FOR_ALL_ENGINEIDS_OF_TYPE(eid, VEH_TRAIN) {
		RailVehicleInfo *rv_info = &_rail_vehicle_info[eid];
		/* if it is an electric rail engine and its railtype is the wrong one */
		if (rv_info->engclass == 2 && rv_info->railtype == old_railtype) {
			/* change it to the proper one */
			rv_info->railtype = new_railtype;
		}
	}

	/* when disabling elrails, make sure that all existing trains can run on
	*  normal rail too */
	if (disable) {
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN && v->u.rail.railtype == RAILTYPE_ELECTRIC) {
				/* this railroad vehicle is now compatible only with elrail,
				*  so add there also normal rail compatibility */
				v->u.rail.compatible_railtypes |= RAILTYPES_RAIL;
				v->u.rail.railtype = RAILTYPE_RAIL;
				SetBit(v->u.rail.flags, VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL);
			}
		}
	}

	/* setup total power for trains */
	FOR_ALL_VEHICLES(v) {
		/* power is cached only for front engines */
		if (v->type == VEH_TRAIN && IsFrontEngine(v)) TrainPowerChanged(v);
	}

	FOR_ALL_PLAYERS(p) p->avail_railtypes = GetPlayerRailtypes(p->index);

	/* This resets the _last_built_railtype, which will be invalid for electric
	* rails. It may have unintended consequences if that function is ever
	* extended, though. */
	ReinitGuiAfterToggleElrail(disable);
	return 0;
}
