/* $Id$ */
/** @file elrail.c
  * This file deals with displaying wires and pylons for electric railway systems.
<h2>Basics</h2>

<h3>Tile Types</h3>

We have two different types of tiles in the drawing code:
Normal Railway Tiles (NRTs) which can have more than one track on it, and
Special Railways tiles (SRTs) which have only one track (like crossings, depots
stations, etc).

<h3>Location Categories</h3>

All tiles are categorized into three location groups (TLG):
Group 0: Tiles with both an even X coordinate and an even Y coordinate
Group 1: Tiles with an even X and an odd Y coordinate
Group 2: Tiles with an odd X and an even Y coordinate
Group 3: Tiles with both an odd X and Y coordnate.

<h3>Pylon Points</h3>
<h4>Control Points</h4>
A Pylon Control Point (PCP) is a position where a wire (or rather two)
is mounted onto a pylon.
Each NRT does contain 4 PCPs which are mapped to a byte
variable and are represented by the DiagDirection enum:

A wire that ends on the PCP has a dark ending, otherwise the end is bright.<p>

Now on each edge there are two PCPs: One from each adjacent tile. Both PCPs are merged
using an OR matrix (i. e. if one tile needs a PCP at the postion in question, both
tiles get it).

<h4>Position Points</h4>
A Pylon Position Point (PPP) is a position where a pylon is located on the ground.
Each PCP owns 8 in (45 degree steps) PPPs that are located around it. PPPs are numbered
0 to 7 with 0 starting north and numbering in clockwise direction. Each track bit has PPPs
that are impossible (because the pylon would be situated on the track), preferred (because
the pylon would be rectangular to the track). PPPs are represented by the Direction enum.

<img src="../../elrail_tile.png">
<img src="../../elrail_track.png">

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
				case RAIL_TYPE_NORMAL: case RAIL_TYPE_SIGNALS:
					return GetTrackBits(t);
				default:
					return 0;
			}
			break;
		case MP_TUNNELBRIDGE:
			if (IsTunnel(t)) {
				if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
				if (override != NULL) *override = 1 << GetTunnelDirection(t);
				return DiagDirToAxis(GetTunnelDirection(t)) == AXIS_X ? TRACK_BIT_X : TRACK_BIT_Y;
			} else {
				if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
				if (
					IsBridgeMiddle(t) &&
					IsTransportUnderBridge(t) &&
					GetTransportTypeUnderBridge(t) == TRANSPORT_RAIL) {
					return GetRailBitsUnderBridge(t);
				} else {
					if (override != NULL && DistanceMax(t, GetOtherBridgeEnd(t)) > 1) *override = 1 << GetBridgeRampDirection(t);

					return GetBridgeAxis(t) == AXIS_X ? TRACK_BIT_X : TRACK_BIT_Y;
				}
			}
		case MP_STREET:
			if (GetRailTypeCrossing(t) != RAILTYPE_ELECTRIC) return 0;
			return GetCrossingRailBits(t);
		case MP_STATION:
			if (GetRailType(t) != RAILTYPE_ELECTRIC) return 0;
			return GetRailStationTrack(t);
		default:
			return 0;
	}
}

/** Draws wires and, if required, pylons on a given tile
  * @param ti The Tileinfo to draw the tile for
  * @todo Currently, each pylon is drawn twice (once for each neighbouring tiles use OwnedPPPonPCP for this)
  */
static void DrawCatenaryRailway(const TileInfo *ti)
{
	/* Pylons are placed on a tile edge, so we need to take into account
	   the track configuration of 2 adjacent tiles. trackconfig[0] stores the
	   current tile (home tile) while [1] holds the neighbour */
	TrackBits trackconfig[TS_END];
	bool isflat[TS_END];
	/* Note that ti->tileh has already been adjusted for Foundations */
	uint tileh[TS_END] = {ti->tileh, 0};

	TLG tlg = GetTLG(ti->tile);
	byte PCPstatus = 0;
	byte OverridePCP = 0;
	byte PPPpreferred[DIAGDIR_END] = {0xFF, 0xFF, 0xFF, 0xFF};
	byte PPPallowed[DIAGDIR_END] = {AllowedPPPonPCP[0], AllowedPPPonPCP[1], AllowedPPPonPCP[2], AllowedPPPonPCP[3]};
	byte PPPbuffer[DIAGDIR_END];
	DiagDirection i;
	Track t;

	/* Find which rail bits are present, and select the override points.
	   We don't draw a pylon:
	   1) INSIDE a tunnel (we wouldn't see it anyway)
	   2) on the "far" end of a bridge head (the one that connects to bridge middle),
	      because that one is drawn on the bridge. Exception is for length 0 bridges
	      which have no middle tiles */
	trackconfig[TS_HOME] = GetRailTrackBitsUniversal(ti->tile, &OverridePCP);
	/* If a track bit is present that is not in the main direction, the track is level */
	isflat[TS_HOME] = trackconfig[TS_HOME] & (TRACK_BIT_UPPER | TRACK_BIT_LOWER | TRACK_BIT_LEFT | TRACK_BIT_RIGHT);

	if (IsTunnelTile(ti->tile)) tileh[TS_HOME] = 0;
	if (IsBridgeTile(ti->tile) && IsBridgeRamp(ti->tile)) {
		if (tileh[TS_HOME] != 0) {
			tileh[TS_HOME] = 0;
		} else {
			switch (GetBridgeRampDirection(ti->tile)) {
				case DIAGDIR_NE: tileh[TS_HOME] = 12; break;
				case DIAGDIR_SE: tileh[TS_HOME] =  6; break;
				case DIAGDIR_SW: tileh[TS_HOME] =  3; break;
				case DIAGDIR_NW: tileh[TS_HOME] =  9; break;
				default: break;
			}
		}
	}

	for (i = DIAGDIR_NE; i < DIAGDIR_END; i++) {
		extern const TileIndexDiffC _tileoffs_by_dir[];
		TileIndex neighbour = ti->tile + TileOffsByDir(i);
		uint foundation = 0;
		int k;

		/* Here's one of the main headaches. GetTileSlope does not correct for possibly
		   existing foundataions, so we do have to do that manually later on.*/
		tileh[TS_NEIGHBOUR] = GetTileSlope(neighbour, NULL);
		trackconfig[TS_NEIGHBOUR] = GetRailTrackBitsUniversal(neighbour, NULL);
		isflat[TS_NEIGHBOUR] = trackconfig[TS_NEIGHBOUR] & (TRACK_BIT_UPPER | TRACK_BIT_LOWER | TRACK_BIT_LEFT | TRACK_BIT_RIGHT);

		/* We cycle through all the existing tracks at a PCP and see what
		   PPPs we want to have, or may not have at all */
		for (k = 0; k < TRACKS_AT_PCP; k++) {
			/* Next to us, we have a bridge head, don't worry about that one, if it shows away from us */
			if (
					trackorigin[i][k] == TS_NEIGHBOUR &&
					IsBridgeTile(neighbour) && IsBridgeRamp(neighbour) &&
					GetBridgeRampDirection(neighbour) == ReverseDiagDir(i)
			   ) continue;

			if (HASBIT(trackconfig[trackorigin[i][k]], PPPtracks[i][k])) {
				DiagDirection PCPpos = (trackorigin[i][k] == 0) ? i : ReverseDiagDir(i);
				PCPstatus |= 1 << i; /* This PCP is in use */
				PPPpreferred[i] &= PreferredPPPofTrackBitAtPCP[PPPtracks[i][k]][PCPpos];
				PPPallowed[i] &= ~DisallowedPPPofTrackBitAtPCP[PPPtracks[i][k]][PCPpos];
			}
		}

		/* Deactivate all PPPs if PCP is not used */
		PPPpreferred[i] *= HASBIT(PCPstatus, i);
		PPPallowed[i] *= HASBIT(PCPstatus, i);

		/* Station on a non-flat tile means foundation. add one height level and adjust tileh */
		if (IsTileType(neighbour, MP_STATION) && tileh[TS_NEIGHBOUR] != 0) tileh[TS_NEIGHBOUR] = 0;

		/* Read the foundataions if they are present, and adjust the tileh */
		if (IsTileType(neighbour, MP_RAILWAY)) foundation = GetRailFoundation(tileh[TS_NEIGHBOUR], trackconfig[TS_NEIGHBOUR]);
		if (IsBridgeTile(neighbour) && IsBridgeRamp(neighbour)) foundation = GetBridgeFoundation(tileh[TS_NEIGHBOUR], GetBridgeAxis(neighbour));
		if (foundation != 0) {
			if (foundation < 15) {
				tileh[TS_NEIGHBOUR] = 0;
			} else {
				tileh[TS_NEIGHBOUR] = _inclined_tileh[foundation - 15];
			}
		}

		/* Convert the real tileh into a pseudo-tileh for the track */
		if (IsTunnelTile(neighbour)) tileh[TS_NEIGHBOUR] = 0;
		if (IsBridgeTile(neighbour) && IsBridgeRamp(neighbour)) {
			if (tileh[TS_NEIGHBOUR] != 0) {
				tileh[TS_NEIGHBOUR] = 0;
			} else {
				switch (GetBridgeRampDirection(neighbour)) {
					case DIAGDIR_NE: tileh[TS_NEIGHBOUR] = 12; break;
					case DIAGDIR_SE: tileh[TS_NEIGHBOUR] =  6; break;
					case DIAGDIR_SW: tileh[TS_NEIGHBOUR] =  3; break;
					case DIAGDIR_NW: tileh[TS_NEIGHBOUR] =  9; break;
					default: break;
				}
			}
		}

		/* If we have a straight (and level) track, we want a pylon only every 2 tiles
		   Delete the PCP if this is the case. */
		/* Level means that the slope is the same, or the track is flat */
		if (tileh[TS_HOME] == tileh[TS_NEIGHBOUR] || (isflat[TS_HOME] && isflat[TS_NEIGHBOUR])) {
			for (k = 0; k < NUM_IGNORE_GROUPS; k++)
				if (PPPpreferred[i] == IgnoredPCP[k][tlg][i]) PCPstatus &= ~(1 << i);
		}

		/* Now decide where we draw our tiles. First try the preferred PPPs, but they may not exist.
		   In that case, we try the any of the allowed ones. if they don't exist either, don't draw
		   anything */
		if (PPPpreferred[i] != 0) {
			/* Some of the preferred PPPs (the ones in direct extension of the track bit)
			   have been used as an "end of line" marker. As these are not ALLOWED, this operation
			   cancles them out */
			PPPbuffer[i] = PPPpreferred[i] & PPPallowed[i];
			/* We haven't any buffer yet, so try something else. Fixes 90° curves */
			if (PPPbuffer[i] == 0) PPPbuffer[i] = PPPallowed[i];
		} else {
			PPPbuffer[i] = PPPallowed[i];
		}

		if (PPPbuffer[i] != 0 && HASBIT(PCPstatus, i) && !HASBIT(OverridePCP, i)) {
			for (k = 0; k < DIR_END; k++) {
				byte temp = PPPorder[i][GetTLG(ti->tile)][k];
				if (HASBIT(PPPbuffer[i], temp)) {
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

	/* Drawing of pylons is finished, now draw the wires */
	for (t = 0; t < TRACK_END; t++) {
		if (HASBIT(trackconfig[TS_HOME], t)) {

			byte PCPconfig = HASBIT(PCPstatus, PCPpositions[t][0]) +
				(HASBIT(PCPstatus, PCPpositions[t][1]) << 1);

			const SortableSpriteStruct *sss;
			int tileh_selector = !(tileh[TS_HOME] % 3) * tileh[TS_HOME] / 3; /* tileh for the slopes, 0 otherwise */

			if ( /* We are not drawing a wire under a low bridge */
					IsBridgeTile(ti->tile) &&
					IsBridgeMiddle(ti->tile) &&
					!(_display_opt & DO_TRANS_BUILDINGS) &&
					GetBridgeHeight(t) <= TilePixelHeight(t)
			   ) return;

			assert(PCPconfig != 0); /* We have a pylon on neither end of the wire, that doesn't work (since we have no sprites for that) */
			assert(!IsSteepTileh(tileh[TS_HOME]));
			sss = &CatenarySpriteData[Wires[tileh_selector][t][PCPconfig]];

			AddSortableSpriteToDraw( sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
				sss->x_size, sss->y_size, sss->z_size, GetSlopeZ(ti->x + min(sss->x_offset, 15), ti->y + min(sss->y_offset, 15)) + sss->z_offset);
		}
	}
}

static void DrawCatenaryOnBridge(const TileInfo *ti)
{
	TileIndex start = GetOtherBridgeEnd(GetSouthernBridgeEnd(ti->tile));
	uint length = GetBridgeLength(GetSouthernBridgeEnd(ti->tile), GetOtherBridgeEnd(GetSouthernBridgeEnd(ti->tile)));
	uint num = DistanceMax(ti->tile, start);
	const SortableSpriteStruct *sss;
	Axis axis = GetBridgeAxis(ti->tile);
	TLG tlg = GetTLG(ti->tile);

	CatenarySprite offset = axis == AXIS_X ? 0 : WIRE_Y_FLAT_BOTH - WIRE_X_FLAT_BOTH;

	if ((length % 2) && num == length) {
		sss = &CatenarySpriteData[WIRE_X_FLAT_BOTH + offset];
	} else {
		sss = &CatenarySpriteData[WIRE_X_FLAT_SW + (num % 2) + offset];
	}

	if (num % 2) {
		if (axis == AXIS_X) {
			AddSortableSpriteToDraw( pylons_bridge[0 + HASBIT(tlg, 0)], ti->x, ti->y + 4 + 8 * HASBIT(tlg, 0), 1, 1, 10, GetBridgeHeight(ti->tile) + 8);
		} else {
			AddSortableSpriteToDraw( pylons_bridge[2 + HASBIT(tlg, 1)], ti->x + 4 + 8 * HASBIT(tlg, 1), ti->y, 1, 1, 10, GetBridgeHeight(ti->tile) + 8);
		}
	}

	if (DistanceMax(ti->tile, start) == length) { /* need a pylon here (the southern end) */
		if (axis == AXIS_X) {
			AddSortableSpriteToDraw( pylons_bridge[0 + HASBIT(tlg, 0)], ti->x + 16, ti->y + 4 + 8 * HASBIT(tlg, 0), 1, 1, 10, GetBridgeHeight(ti->tile) + 8);
		} else {
			AddSortableSpriteToDraw( pylons_bridge[2 + HASBIT(tlg, 1)], ti->x + 4 + 8 * HASBIT(tlg, 1), ti->y + 16, 1, 1, 10, GetBridgeHeight(ti->tile) + 8);
		}
	}

	AddSortableSpriteToDraw( sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
			sss->x_size, sss->y_size, sss->z_size, GetBridgeHeight(ti->tile) + sss->z_offset + 8);
}

void DrawCatenary(const TileInfo *ti)
{
	switch (GetTileType(ti->tile)) {
		case MP_RAILWAY:
			if (GetRailTileType(ti->tile) == RAIL_TYPE_DEPOT_WAYPOINT && GetRailTileSubtype(ti->tile) == RAIL_SUBTYPE_DEPOT) {
				const SortableSpriteStruct *sss = &CatenarySpriteData[WIRE_DEPOT_SW + ReverseDiagDir(GetRailDepotDirection(ti->tile))];
				AddSortableSpriteToDraw( sss->image, ti->x + sss->x_offset, ti->y + sss->y_offset,
					sss->x_size, sss->y_size, sss->z_size, GetSlopeZ(ti->x, ti->y) + sss->z_offset);
				return;
			}
			/* Fall through */
		case MP_TUNNELBRIDGE:
			if (IsBridgeTile(ti->tile) && IsBridgeMiddle(ti->tile) && GetRailTypeOnBridge(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenaryOnBridge(ti);
			/* Fall further */
		case MP_STREET: case MP_STATION:
			DrawCatenaryRailway(ti);
			break;
		default:
			break;
	}
}

