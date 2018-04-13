/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file sprites.h
 * This file contains all sprite-related enums and defines. These consist mainly of
 * the sprite numbers and a bunch of masks and macros to handle sprites and to get
 * rid of all the magic numbers in the code.
 *
 * @note
 * ALL SPRITE NUMBERS BELOW 5126 are in the main files
 *
 * All elements which consist of two elements should
 * have the same name and then suffixes
 *   _GROUND and _BUILD for building-type sprites
 *   _REAR and _FRONT for transport-type sprites (tiles where vehicles are on)
 * These sprites are split because of the Z order of the elements
 *  (like some parts of a bridge are behind the vehicle, while others are before)
 *
 *
 * All sprites which are described here are referenced only one to a handful of times
 * throughout the code. When introducing new sprite enums, use meaningful names.
 * Don't be lazy and typing, and only use abbreviations when their meaning is clear or
 * the length of the enum would get out of hand. In that case EXPLAIN THE ABBREVATION
 * IN THIS FILE, and perhaps add some comments in the code where it is used.
 * Now, don't whine about this being too much typing work if the enums are like
 * 30 characters in length. If your editor doen't help you simplifying your work,
 * get a proper editor. If your Operating Systems don't have any decent editors,
 * get a proper Operating System.
 *
 * @todo Split the "Sprites" enum into smaller chunks and document them
 */

#ifndef SPRITES_H
#define SPRITES_H

#include "../gfx_type.h"

static const SpriteID SPR_SELECT_TILE  = 752;
static const SpriteID SPR_DOT          = 774; // corner marker for lower/raise land
static const SpriteID SPR_DOT_SMALL    = 4078;
static const SpriteID SPR_WHITE_POINT  = 4079;

/* ASCII */
static const SpriteID SPR_ASCII_SPACE       = 2;
static const SpriteID SPR_ASCII_SPACE_SMALL = 226;
static const SpriteID SPR_ASCII_SPACE_BIG   = 450;

static const SpriteID SPR_LARGE_SMALL_WINDOW = 682;

/** Extra graphic spritenumbers */
static const SpriteID SPR_OPENTTD_BASE   = 4896;
static const uint16 OPENTTD_SPRITE_COUNT = 180;

/* Halftile-selection sprites */
static const SpriteID SPR_HALFTILE_SELECTION_FLAT = SPR_OPENTTD_BASE;
static const SpriteID SPR_HALFTILE_SELECTION_DOWN = SPR_OPENTTD_BASE + 4;
static const SpriteID SPR_HALFTILE_SELECTION_UP   = SPR_OPENTTD_BASE + 8;

static const SpriteID SPR_SQUARE             = SPR_OPENTTD_BASE + 38;  // coloured square (used for newgrf compatibility)
static const SpriteID SPR_BLOT               = SPR_OPENTTD_BASE + 39;  // coloured circle (used for server compatibility and installed content)
static const SpriteID SPR_LOCK               = SPR_OPENTTD_BASE + 40;  // lock icon (for password protected servers)
static const SpriteID SPR_BOX_EMPTY          = SPR_OPENTTD_BASE + 41;
static const SpriteID SPR_BOX_CHECKED        = SPR_OPENTTD_BASE + 42;
static const SpriteID SPR_WARNING_SIGN       = SPR_OPENTTD_BASE + 43;  // warning sign (shown if there are any newgrf errors)
static const SpriteID SPR_WINDOW_RESIZE_RIGHT= SPR_OPENTTD_BASE + 44;  // resize icon to the right
static const SpriteID SPR_WINDOW_RESIZE_LEFT = SPR_OPENTTD_BASE + 149; // resize icon to the left
static const SpriteID SPR_WINDOW_SHADE       = SPR_OPENTTD_BASE + 151; // shade the window icon
static const SpriteID SPR_WINDOW_UNSHADE     = SPR_OPENTTD_BASE + 152; // unshade the window icon
static const SpriteID SPR_WINDOW_DEBUG       = SPR_OPENTTD_BASE + 153; // NewGRF debug window icon
static const SpriteID SPR_IMG_PLAY_MUSIC_RTL = SPR_OPENTTD_BASE + 150; // play music button, but then for RTL users
/* Arrow icons pointing in all 4 directions */
static const SpriteID SPR_ARROW_DOWN         = SPR_OPENTTD_BASE + 45;
static const SpriteID SPR_ARROW_UP           = SPR_OPENTTD_BASE + 46;
static const SpriteID SPR_ARROW_LEFT         = SPR_OPENTTD_BASE + 47;
static const SpriteID SPR_ARROW_RIGHT        = SPR_OPENTTD_BASE + 48;
static const SpriteID SPR_HOUSE_ICON         = SPR_OPENTTD_BASE + 49;
static const SpriteID SPR_SHARED_ORDERS_ICON = SPR_OPENTTD_BASE + 50;
static const SpriteID SPR_PIN_UP             = SPR_OPENTTD_BASE + 51;  // pin icon
static const SpriteID SPR_PIN_DOWN           = SPR_OPENTTD_BASE + 52;

static const SpriteID SPR_CLOSEBOX           = 143;

static const SpriteID SPR_CIRCLE_FOLDED      = SPR_OPENTTD_BASE + 147; // (+) icon
static const SpriteID SPR_CIRCLE_UNFOLDED    = SPR_OPENTTD_BASE + 148; // (-) icon

/* on screen keyboard icons */
static const SpriteID SPR_OSK_LEFT           = SPR_OPENTTD_BASE + 138;
static const SpriteID SPR_OSK_RIGHT          = SPR_OPENTTD_BASE + 139;
static const SpriteID SPR_OSK_CAPS           = SPR_OPENTTD_BASE + 140;
static const SpriteID SPR_OSK_SHIFT          = SPR_OPENTTD_BASE + 141;
static const SpriteID SPR_OSK_BACKSPACE      = SPR_OPENTTD_BASE + 142;
static const SpriteID SPR_OSK_SPECIAL        = SPR_OPENTTD_BASE + 143;

/** Clone vehicles stuff */
static const SpriteID SPR_CLONE_TRAIN    = SPR_OPENTTD_BASE + 106;
static const SpriteID SPR_CLONE_ROADVEH  = SPR_OPENTTD_BASE + 107;
static const SpriteID SPR_CLONE_SHIP     = SPR_OPENTTD_BASE + 108;
static const SpriteID SPR_CLONE_AIRCRAFT = SPR_OPENTTD_BASE + 109;

static const SpriteID SPR_SELL_TRAIN        = SPR_OPENTTD_BASE +  93;
static const SpriteID SPR_SELL_ROADVEH      = SPR_OPENTTD_BASE +  94;
static const SpriteID SPR_SELL_SHIP         = SPR_OPENTTD_BASE +  95;
static const SpriteID SPR_SELL_AIRCRAFT     = SPR_OPENTTD_BASE +  96;
static const SpriteID SPR_SELL_ALL_TRAIN    = SPR_OPENTTD_BASE +  97;
static const SpriteID SPR_SELL_ALL_ROADVEH  = SPR_OPENTTD_BASE +  98;
static const SpriteID SPR_SELL_ALL_SHIP     = SPR_OPENTTD_BASE +  99;
static const SpriteID SPR_SELL_ALL_AIRCRAFT = SPR_OPENTTD_BASE + 100;
static const SpriteID SPR_REPLACE_TRAIN     = SPR_OPENTTD_BASE + 101;
static const SpriteID SPR_REPLACE_ROADVEH   = SPR_OPENTTD_BASE + 102;
static const SpriteID SPR_REPLACE_SHIP      = SPR_OPENTTD_BASE + 103;
static const SpriteID SPR_REPLACE_AIRCRAFT  = SPR_OPENTTD_BASE + 104;
static const SpriteID SPR_SELL_CHAIN_TRAIN  = SPR_OPENTTD_BASE + 105;

static const SpriteID SPR_PROFIT_NA         = SPR_OPENTTD_BASE + 154;
static const SpriteID SPR_PROFIT_NEGATIVE   = SPR_OPENTTD_BASE + 155;
static const SpriteID SPR_PROFIT_SOME       = SPR_OPENTTD_BASE + 156;
static const SpriteID SPR_PROFIT_LOT        = SPR_OPENTTD_BASE + 157;

static const SpriteID SPR_UNREAD_NEWS                = SPR_OPENTTD_BASE + 158;
static const SpriteID SPR_EXCLUSIVE_TRANSPORT        = SPR_OPENTTD_BASE + 159;
static const SpriteID SPR_GROUP_REPLACE_PROTECT      = SPR_OPENTTD_BASE + 160;
static const SpriteID SPR_GROUP_REPLACE_ACTIVE       = SPR_OPENTTD_BASE + 161;

static const SpriteID SPR_GROUP_CREATE_TRAIN         = SPR_OPENTTD_BASE + 114;
static const SpriteID SPR_GROUP_CREATE_ROADVEH       = SPR_OPENTTD_BASE + 115;
static const SpriteID SPR_GROUP_CREATE_SHIP          = SPR_OPENTTD_BASE + 116;
static const SpriteID SPR_GROUP_CREATE_AIRCRAFT      = SPR_OPENTTD_BASE + 117;
static const SpriteID SPR_GROUP_DELETE_TRAIN         = SPR_OPENTTD_BASE + 118;
static const SpriteID SPR_GROUP_DELETE_ROADVEH       = SPR_OPENTTD_BASE + 119;
static const SpriteID SPR_GROUP_DELETE_SHIP          = SPR_OPENTTD_BASE + 120;
static const SpriteID SPR_GROUP_DELETE_AIRCRAFT      = SPR_OPENTTD_BASE + 121;
static const SpriteID SPR_GROUP_RENAME_TRAIN         = SPR_OPENTTD_BASE + 122;
static const SpriteID SPR_GROUP_RENAME_ROADVEH       = SPR_OPENTTD_BASE + 123;
static const SpriteID SPR_GROUP_RENAME_SHIP          = SPR_OPENTTD_BASE + 124;
static const SpriteID SPR_GROUP_RENAME_AIRCRAFT      = SPR_OPENTTD_BASE + 125;
static const SpriteID SPR_GROUP_REPLACE_ON_TRAIN     = SPR_OPENTTD_BASE + 126;
static const SpriteID SPR_GROUP_REPLACE_ON_ROADVEH   = SPR_OPENTTD_BASE + 127;
static const SpriteID SPR_GROUP_REPLACE_ON_SHIP      = SPR_OPENTTD_BASE + 128;
static const SpriteID SPR_GROUP_REPLACE_ON_AIRCRAFT  = SPR_OPENTTD_BASE + 129;
static const SpriteID SPR_GROUP_REPLACE_OFF_TRAIN    = SPR_OPENTTD_BASE + 130;
static const SpriteID SPR_GROUP_REPLACE_OFF_ROADVEH  = SPR_OPENTTD_BASE + 131;
static const SpriteID SPR_GROUP_REPLACE_OFF_SHIP     = SPR_OPENTTD_BASE + 132;
static const SpriteID SPR_GROUP_REPLACE_OFF_AIRCRAFT = SPR_OPENTTD_BASE + 133;

static const SpriteID SPR_TOWN_RATING_NA             = SPR_OPENTTD_BASE + 162;
static const SpriteID SPR_TOWN_RATING_APALLING       = SPR_OPENTTD_BASE + 163;
static const SpriteID SPR_TOWN_RATING_MEDIOCRE       = SPR_OPENTTD_BASE + 164;
static const SpriteID SPR_TOWN_RATING_GOOD           = SPR_OPENTTD_BASE + 165;

static const SpriteID SPR_IMG_SWITCH_TOOLBAR = SPR_OPENTTD_BASE + 144;

static const SpriteID SPR_IMG_DELETE_LEFT            = SPR_OPENTTD_BASE + 166;
static const SpriteID SPR_IMG_DELETE_RIGHT           = SPR_OPENTTD_BASE + 167;

static const SpriteID SPR_WINDOW_DEFSIZE             = SPR_OPENTTD_BASE + 168;

static const SpriteID SPR_IMG_CARGOFLOW              = SPR_OPENTTD_BASE + 174;

static const SpriteID SPR_SIGNALS_BASE  = SPR_OPENTTD_BASE + OPENTTD_SPRITE_COUNT;
static const uint16 PRESIGNAL_SPRITE_COUNT                   =  48;
static const uint16 PRESIGNAL_AND_SEMAPHORE_SPRITE_COUNT     = 112;
static const uint16 PRESIGNAL_SEMAPHORE_AND_PBS_SPRITE_COUNT = 240;

static const SpriteID SPR_CANALS_BASE   = SPR_SIGNALS_BASE + PRESIGNAL_SEMAPHORE_AND_PBS_SPRITE_COUNT;
static const uint16 CANALS_SPRITE_COUNT = 65;

/** Sprites for the Aqueduct. */
static const SpriteID SPR_AQUEDUCT_BASE     = SPR_CANALS_BASE + CANALS_SPRITE_COUNT;
static const SpriteID SPR_AQUEDUCT_RAMP_SW  = SPR_AQUEDUCT_BASE + 0;
static const SpriteID SPR_AQUEDUCT_RAMP_SE  = SPR_AQUEDUCT_BASE + 1;
static const SpriteID SPR_AQUEDUCT_RAMP_NE  = SPR_AQUEDUCT_BASE + 2;
static const SpriteID SPR_AQUEDUCT_RAMP_NW  = SPR_AQUEDUCT_BASE + 3;
static const SpriteID SPR_AQUEDUCT_MIDDLE_X = SPR_AQUEDUCT_BASE + 4;
static const SpriteID SPR_AQUEDUCT_MIDDLE_Y = SPR_AQUEDUCT_BASE + 5;
static const SpriteID SPR_AQUEDUCT_PILLAR_X = SPR_AQUEDUCT_BASE + 6;
static const SpriteID SPR_AQUEDUCT_PILLAR_Y = SPR_AQUEDUCT_BASE + 7;
static const uint16 AQUEDUCT_SPRITE_COUNT = 8;

/** Sprites for 'highlighting' tracks on sloped land. */
static const SpriteID SPR_TRACKS_FOR_SLOPES_BASE        = SPR_AQUEDUCT_BASE + AQUEDUCT_SPRITE_COUNT;
static const SpriteID SPR_TRACKS_FOR_SLOPES_RAIL_BASE   = SPR_TRACKS_FOR_SLOPES_BASE + 0;
static const SpriteID SPR_TRACKS_FOR_SLOPES_MONO_BASE   = SPR_TRACKS_FOR_SLOPES_BASE + 4;
static const SpriteID SPR_TRACKS_FOR_SLOPES_MAGLEV_BASE = SPR_TRACKS_FOR_SLOPES_BASE + 8;
static const uint16 TRACKS_FOR_SLOPES_SPRITE_COUNT = 12;

static const SpriteID SPR_SLOPES_BASE              = SPR_TRACKS_FOR_SLOPES_BASE + TRACKS_FOR_SLOPES_SPRITE_COUNT;
static const SpriteID SPR_SLOPES_INCLINED_OFFSET   = 15;
static const SpriteID SPR_SLOPES_VIRTUAL_BASE      = SPR_SLOPES_BASE - SPR_SLOPES_INCLINED_OFFSET; // The original foundations (see SPR_FOUNDATION_BASE below) are mapped before the additional foundations.
static const SpriteID SPR_TRKFOUND_BLOCK_SIZE      = 22; // The normal track foundation sprites are organized in blocks of 22.
static const uint16 NORMAL_FOUNDATION_SPRITE_COUNT = 74;
/** Halftile foundations */
static const SpriteID SPR_HALFTILE_FOUNDATION_BASE = SPR_SLOPES_BASE + NORMAL_FOUNDATION_SPRITE_COUNT;
static const SpriteID SPR_HALFTILE_BLOCK_SIZE      = 4;  // The half tile foundation sprites are organized in blocks of 4.
static const uint16 NORMAL_AND_HALFTILE_FOUNDATION_SPRITE_COUNT = 90;

static const SpriteID SPR_AUTORAIL_BASE = SPR_HALFTILE_FOUNDATION_BASE + NORMAL_AND_HALFTILE_FOUNDATION_SPRITE_COUNT;
static const uint16 AUTORAIL_SPRITE_COUNT = 55;

static const SpriteID SPR_ELRAIL_BASE   = SPR_AUTORAIL_BASE + AUTORAIL_SPRITE_COUNT;
static const uint16 ELRAIL_SPRITE_COUNT = 48;

static const SpriteID SPR_2CCMAP_BASE   = SPR_ELRAIL_BASE + ELRAIL_SPRITE_COUNT;
static const uint16 TWOCCMAP_SPRITE_COUNT = 256;

/** shore tiles - action 05-0D */
static const SpriteID SPR_SHORE_BASE                  = SPR_2CCMAP_BASE + TWOCCMAP_SPRITE_COUNT;
static const SpriteID SPR_SHORE_SPRITE_COUNT          = 18;
static const SpriteID SPR_ORIGINALSHORE_START         = 4062;
static const SpriteID SPR_ORIGINALSHORE_END           = 4069;

static const SpriteID SPR_AIRPORTX_BASE     = SPR_SHORE_BASE + SPR_SHORE_SPRITE_COUNT; // The sprites used for other airport angles
static const SpriteID SPR_NEWAIRPORT_TARMAC = SPR_AIRPORTX_BASE;
static const SpriteID SPR_NSRUNWAY1         = SPR_AIRPORTX_BASE + 1;
static const SpriteID SPR_NSRUNWAY2         = SPR_AIRPORTX_BASE + 2;
static const SpriteID SPR_NSRUNWAY3         = SPR_AIRPORTX_BASE + 3;
static const SpriteID SPR_NSRUNWAY4         = SPR_AIRPORTX_BASE + 4;
static const SpriteID SPR_NSRUNWAY_END      = SPR_AIRPORTX_BASE + 5;
static const SpriteID SPR_NEWHANGAR_S       = SPR_AIRPORTX_BASE + 6;
static const SpriteID SPR_NEWHANGAR_S_WALL  = SPR_AIRPORTX_BASE + 7;
static const SpriteID SPR_NEWHANGAR_W       = SPR_AIRPORTX_BASE + 8;
static const SpriteID SPR_NEWHANGAR_W_WALL  = SPR_AIRPORTX_BASE + 9;
static const SpriteID SPR_NEWHANGAR_N       = SPR_AIRPORTX_BASE + 10;
static const SpriteID SPR_NEWHANGAR_E       = SPR_AIRPORTX_BASE + 11;
static const SpriteID SPR_NEWHELIPAD        = SPR_AIRPORTX_BASE + 12;
static const SpriteID SPR_GRASS_RIGHT       = SPR_AIRPORTX_BASE + 13;
static const SpriteID SPR_GRASS_LEFT        = SPR_AIRPORTX_BASE + 14;
static const uint16 AIRPORTX_SPRITE_COUNT = 15;

/** Airport preview sprites */
static const SpriteID SPR_AIRPORT_PREVIEW_BASE             = SPR_AIRPORTX_BASE + AIRPORTX_SPRITE_COUNT;
static const SpriteID SPR_AIRPORT_PREVIEW_SMALL            = SPR_AIRPORT_PREVIEW_BASE;
static const SpriteID SPR_AIRPORT_PREVIEW_LARGE            = SPR_AIRPORT_PREVIEW_BASE + 1;
static const SpriteID SPR_AIRPORT_PREVIEW_HELIPORT         = SPR_AIRPORT_PREVIEW_BASE + 2;
static const SpriteID SPR_AIRPORT_PREVIEW_METROPOLITAN     = SPR_AIRPORT_PREVIEW_BASE + 3;
static const SpriteID SPR_AIRPORT_PREVIEW_INTERNATIONAL    = SPR_AIRPORT_PREVIEW_BASE + 4;
static const SpriteID SPR_AIRPORT_PREVIEW_COMMUTER         = SPR_AIRPORT_PREVIEW_BASE + 5;
static const SpriteID SPR_AIRPORT_PREVIEW_HELIDEPOT        = SPR_AIRPORT_PREVIEW_BASE + 6;
static const SpriteID SPR_AIRPORT_PREVIEW_INTERCONTINENTAL = SPR_AIRPORT_PREVIEW_BASE + 7;
static const SpriteID SPR_AIRPORT_PREVIEW_HELISTATION      = SPR_AIRPORT_PREVIEW_BASE + 8;
static const SpriteID SPR_AIRPORT_PREVIEW_COUNT            = 9;

static const SpriteID SPR_ROADSTOP_BASE     = SPR_AIRPORT_PREVIEW_BASE + SPR_AIRPORT_PREVIEW_COUNT; // The sprites used for drive-through road stops
static const SpriteID SPR_BUS_STOP_DT_Y_W   = SPR_ROADSTOP_BASE;
static const SpriteID SPR_BUS_STOP_DT_Y_E   = SPR_ROADSTOP_BASE + 1;
static const SpriteID SPR_BUS_STOP_DT_X_W   = SPR_ROADSTOP_BASE + 2;
static const SpriteID SPR_BUS_STOP_DT_X_E   = SPR_ROADSTOP_BASE + 3;
static const SpriteID SPR_TRUCK_STOP_DT_Y_W = SPR_ROADSTOP_BASE + 4;
static const SpriteID SPR_TRUCK_STOP_DT_Y_E = SPR_ROADSTOP_BASE + 5;
static const SpriteID SPR_TRUCK_STOP_DT_X_W = SPR_ROADSTOP_BASE + 6;
static const SpriteID SPR_TRUCK_STOP_DT_X_E = SPR_ROADSTOP_BASE + 7;
static const uint16 ROADSTOP_SPRITE_COUNT = 8;

/** Tramway sprites */
static const SpriteID SPR_TRAMWAY_BASE                 = SPR_ROADSTOP_BASE + ROADSTOP_SPRITE_COUNT;
static const SpriteID SPR_TRAMWAY_OVERLAY              = SPR_TRAMWAY_BASE + 4;
static const SpriteID SPR_TRAMWAY_TRAM                 = SPR_TRAMWAY_BASE + 27;
static const SpriteID SPR_TRAMWAY_SLOPED_OFFSET        = 11;
static const SpriteID SPR_TRAMWAY_BUS_STOP_DT_Y_W      = SPR_TRAMWAY_BASE + 25;
static const SpriteID SPR_TRAMWAY_BUS_STOP_DT_Y_E      = SPR_TRAMWAY_BASE + 23;
static const SpriteID SPR_TRAMWAY_BUS_STOP_DT_X_W      = SPR_TRAMWAY_BASE + 24;
static const SpriteID SPR_TRAMWAY_BUS_STOP_DT_X_E      = SPR_TRAMWAY_BASE + 26;
static const SpriteID SPR_TRAMWAY_PAVED_STRAIGHT_Y     = SPR_TRAMWAY_BASE + 46;
static const SpriteID SPR_TRAMWAY_PAVED_STRAIGHT_X     = SPR_TRAMWAY_BASE + 47;
static const SpriteID SPR_TRAMWAY_DEPOT_WITH_TRACK     = SPR_TRAMWAY_BASE + 49;
static const SpriteID SPR_TRAMWAY_BACK_WIRES_STRAIGHT  = SPR_TRAMWAY_BASE + 55;
static const SpriteID SPR_TRAMWAY_FRONT_WIRES_STRAIGHT = SPR_TRAMWAY_BASE + 56;
static const SpriteID SPR_TRAMWAY_BACK_WIRES_SLOPED    = SPR_TRAMWAY_BASE + 72;
static const SpriteID SPR_TRAMWAY_FRONT_WIRES_SLOPED   = SPR_TRAMWAY_BASE + 68;
static const SpriteID SPR_TRAMWAY_TUNNEL_WIRES         = SPR_TRAMWAY_BASE + 80;
static const SpriteID SPR_TRAMWAY_BRIDGE               = SPR_TRAMWAY_BASE + 107;
static const SpriteID SPR_TRAMWAY_DEPOT_NO_TRACK       = SPR_TRAMWAY_BASE + 113;
static const uint16 TRAMWAY_SPRITE_COUNT = 119;

/** One way road sprites */
static const SpriteID SPR_ONEWAY_BASE = SPR_TRAMWAY_BASE + TRAMWAY_SPRITE_COUNT;
static const uint16 ONEWAY_SPRITE_COUNT = 6;

/** Flags sprites (in same order as enum NetworkLanguage) */
static const SpriteID SPR_FLAGS_BASE = SPR_ONEWAY_BASE + ONEWAY_SPRITE_COUNT;
static const uint16 FLAGS_SPRITE_COUNT = 36;

/** Tunnel sprites with grass only for custom railtype tunnel. */
static const SpriteID SPR_RAILTYPE_TUNNEL_BASE = SPR_FLAGS_BASE + FLAGS_SPRITE_COUNT;
static const uint16 RAILTYPE_TUNNEL_BASE_COUNT = 16;

/* Not really a sprite, but an empty bounding box. Used to construct bounding boxes that help sorting the sprites, but do not have a sprite associated. */
static const SpriteID SPR_EMPTY_BOUNDING_BOX = SPR_RAILTYPE_TUNNEL_BASE + RAILTYPE_TUNNEL_BASE_COUNT;
static const uint16 EMPTY_BOUNDING_BOX_SPRITE_COUNT = 1;

/* Black palette sprite, needed for painting (fictive) tiles outside map */
static const SpriteID SPR_PALETTE_BASE = SPR_EMPTY_BOUNDING_BOX + EMPTY_BOUNDING_BOX_SPRITE_COUNT;
static const uint16 PALETTE_SPRITE_COUNT = 1;

/* From where can we start putting NewGRFs? */
static const SpriteID SPR_NEWGRFS_BASE = SPR_PALETTE_BASE + PALETTE_SPRITE_COUNT;

/* Manager face sprites */
static const SpriteID SPR_GRADIENT = 874; // background gradient behind manager face

/* Icon showing company colour. */
static const SpriteID SPR_COMPANY_ICON = 747;

/* is itself no foundation sprite, because tileh 0 has no foundation */
static const SpriteID SPR_FOUNDATION_BASE = 989;

/* Shadow cell */
static const SpriteID SPR_SHADOW_CELL = 1004;

/* Objects spritenumbers */
static const SpriteID SPR_TRANSMITTER             = 2601;
static const SpriteID SPR_LIGHTHOUSE              = 2602;
static const SpriteID SPR_TINYHQ_NORTH            = 2603;
static const SpriteID SPR_TINYHQ_EAST             = 2604;
static const SpriteID SPR_TINYHQ_WEST             = 2605;
static const SpriteID SPR_TINYHQ_SOUTH            = 2606;
static const SpriteID SPR_SMALLHQ_NORTH           = 2607;
static const SpriteID SPR_SMALLHQ_EAST            = 2608;
static const SpriteID SPR_SMALLHQ_WEST            = 2609;
static const SpriteID SPR_SMALLHQ_SOUTH           = 2610;
static const SpriteID SPR_MEDIUMHQ_NORTH          = 2611;
static const SpriteID SPR_MEDIUMHQ_NORTH_WALL     = 2612;
static const SpriteID SPR_MEDIUMHQ_EAST           = 2613;
static const SpriteID SPR_MEDIUMHQ_EAST_WALL      = 2614;
static const SpriteID SPR_MEDIUMHQ_WEST           = 2615;
static const SpriteID SPR_MEDIUMHQ_WEST_WALL      = 2616; // very tiny piece of wall
static const SpriteID SPR_MEDIUMHQ_SOUTH          = 2617;
static const SpriteID SPR_LARGEHQ_NORTH_GROUND    = 2618;
static const SpriteID SPR_LARGEHQ_NORTH_BUILD     = 2619;
static const SpriteID SPR_LARGEHQ_EAST_GROUND     = 2620;
static const SpriteID SPR_LARGEHQ_EAST_BUILD      = 2621;
static const SpriteID SPR_LARGEHQ_WEST_GROUND     = 2622;
static const SpriteID SPR_LARGEHQ_WEST_BUILD      = 2623;
static const SpriteID SPR_LARGEHQ_SOUTH           = 2624;
static const SpriteID SPR_HUGEHQ_NORTH_GROUND     = 2625;
static const SpriteID SPR_HUGEHQ_NORTH_BUILD      = 2626;
static const SpriteID SPR_HUGEHQ_EAST_GROUND      = 2627;
static const SpriteID SPR_HUGEHQ_EAST_BUILD       = 2628;
static const SpriteID SPR_HUGEHQ_WEST_GROUND      = 2629;
static const SpriteID SPR_HUGEHQ_WEST_BUILD       = 2630;
static const SpriteID SPR_HUGEHQ_SOUTH            = 2631;
static const SpriteID SPR_CONCRETE_GROUND         = 1420;
static const SpriteID SPR_STATUE_COMPANY          = 2632;
static const SpriteID SPR_BOUGHT_LAND             = 4790;

/* sprites for rail and rail stations*/
static const SpriteID SPR_RAIL_SNOW_OFFSET        = 26;
static const SpriteID SPR_MONO_SNOW_OFFSET        = 26;
static const SpriteID SPR_MGLV_SNOW_OFFSET        = 26;

static const SpriteID SPR_ORIGINAL_SIGNALS_BASE   = 1275;

static const SpriteID SPR_RAIL_SINGLE_X           = 1005;
static const SpriteID SPR_RAIL_SINGLE_Y           = 1006;
static const SpriteID SPR_RAIL_SINGLE_NORTH       = 1007;
static const SpriteID SPR_RAIL_SINGLE_SOUTH       = 1008;
static const SpriteID SPR_RAIL_SINGLE_EAST        = 1009;
static const SpriteID SPR_RAIL_SINGLE_WEST        = 1010;
static const SpriteID SPR_RAIL_TRACK_Y            = 1011;
static const SpriteID SPR_RAIL_TRACK_X            = 1012;
static const SpriteID SPR_RAIL_TRACK_BASE         = 1018;
static const SpriteID SPR_RAIL_TRACK_N_S          = 1035;
static const SpriteID SPR_RAIL_TRACK_Y_SNOW       = 1037;
static const SpriteID SPR_RAIL_TRACK_X_SNOW       = 1038;
static const SpriteID SPR_RAIL_DEPOT_SE_1         = 1063;
static const SpriteID SPR_RAIL_DEPOT_SE_2         = 1064;
static const SpriteID SPR_RAIL_DEPOT_SW_1         = 1065;
static const SpriteID SPR_RAIL_DEPOT_SW_2         = 1066;
static const SpriteID SPR_RAIL_DEPOT_NE           = 1067;
static const SpriteID SPR_RAIL_DEPOT_NW           = 1068;
static const SpriteID SPR_RAIL_PLATFORM_Y_FRONT         = 1069;
static const SpriteID SPR_RAIL_PLATFORM_X_REAR          = 1070;
static const SpriteID SPR_RAIL_PLATFORM_Y_REAR          = 1071;
static const SpriteID SPR_RAIL_PLATFORM_X_FRONT         = 1072;
static const SpriteID SPR_RAIL_PLATFORM_BUILDING_X      = 1073;
static const SpriteID SPR_RAIL_PLATFORM_BUILDING_Y      = 1074;
static const SpriteID SPR_RAIL_PLATFORM_PILLARS_Y_FRONT = 1075;
static const SpriteID SPR_RAIL_PLATFORM_PILLARS_X_REAR  = 1076;
static const SpriteID SPR_RAIL_PLATFORM_PILLARS_Y_REAR  = 1077;
static const SpriteID SPR_RAIL_PLATFORM_PILLARS_X_FRONT = 1078;
static const SpriteID SPR_RAIL_ROOF_STRUCTURE_X_TILE_A  = 1079; // First half of the roof structure
static const SpriteID SPR_RAIL_ROOF_STRUCTURE_Y_TILE_A  = 1080;
static const SpriteID SPR_RAIL_ROOF_STRUCTURE_X_TILE_B  = 1081; // Second half of the roof structure
static const SpriteID SPR_RAIL_ROOF_STRUCTURE_Y_TILE_B  = 1082;
static const SpriteID SPR_RAIL_ROOF_GLASS_X_TILE_A      = 1083; // First half of the roof glass
static const SpriteID SPR_RAIL_ROOF_GLASS_Y_TILE_A      = 1084;
static const SpriteID SPR_RAIL_ROOF_GLASS_X_TILE_B      = 1085; // second half of the roof glass
static const SpriteID SPR_RAIL_ROOF_GLASS_Y_TILE_B      = 1086;
static const SpriteID SPR_MONO_SINGLE_X                 = 1087;
static const SpriteID SPR_MONO_SINGLE_Y                 = 1088;
static const SpriteID SPR_MONO_SINGLE_NORTH             = 1089;
static const SpriteID SPR_MONO_SINGLE_SOUTH             = 1090;
static const SpriteID SPR_MONO_SINGLE_EAST              = 1091;
static const SpriteID SPR_MONO_SINGLE_WEST              = 1092;
static const SpriteID SPR_MONO_TRACK_Y                  = 1093;
static const SpriteID SPR_MONO_TRACK_BASE               = 1100;
static const SpriteID SPR_MONO_TRACK_N_S                = 1117;
static const SpriteID SPR_MGLV_SINGLE_X                 = 1169;
static const SpriteID SPR_MGLV_SINGLE_Y                 = 1170;
static const SpriteID SPR_MGLV_SINGLE_NORTH             = 1171;
static const SpriteID SPR_MGLV_SINGLE_SOUTH             = 1172;
static const SpriteID SPR_MGLV_SINGLE_EAST              = 1173;
static const SpriteID SPR_MGLV_SINGLE_WEST              = 1174;
static const SpriteID SPR_MGLV_TRACK_Y                  = 1175;
static const SpriteID SPR_MGLV_TRACK_BASE               = 1182;
static const SpriteID SPR_MGLV_TRACK_N_S                = 1199;
static const SpriteID SPR_WAYPOINT_X_1            = SPR_OPENTTD_BASE + 78;
static const SpriteID SPR_WAYPOINT_X_2            = SPR_OPENTTD_BASE + 79;
static const SpriteID SPR_WAYPOINT_Y_1            = SPR_OPENTTD_BASE + 80;
static const SpriteID SPR_WAYPOINT_Y_2            = SPR_OPENTTD_BASE + 81;
/* see _track_sloped_sprites in rail_cmd.cpp for slope offsets */

/* Track fences */
static const SpriteID SPR_TRACK_FENCE_FLAT_X    = 1301;
static const SpriteID SPR_TRACK_FENCE_FLAT_Y    = 1302;
static const SpriteID SPR_TRACK_FENCE_FLAT_VERT = 1303;
static const SpriteID SPR_TRACK_FENCE_FLAT_HORZ = 1304;
static const SpriteID SPR_TRACK_FENCE_SLOPE_SW  = 1305;
static const SpriteID SPR_TRACK_FENCE_SLOPE_SE  = 1306;
static const SpriteID SPR_TRACK_FENCE_SLOPE_NE  = 1307;
static const SpriteID SPR_TRACK_FENCE_SLOPE_NW  = 1308;

/* Base sprites for elrail.
 * Offsets via an enum are used so a complete list of absolute
 * sprite numbers is unnecessary.
 */
static const SpriteID SPR_WIRE_BASE         = SPR_ELRAIL_BASE +  0;
static const SpriteID SPR_PYLON_BASE        = SPR_ELRAIL_BASE + 28;

/* sprites for roads */
static const SpriteID SPR_ROAD_PAVED_STRAIGHT_Y       = 1313;
static const SpriteID SPR_ROAD_PAVED_STRAIGHT_X       = 1314;

/* sprites for airports and airfields*/
/* Small airports are AIRFIELD, everything else is AIRPORT */
static const SpriteID SPR_HELIPORT                    = 2633;
static const SpriteID SPR_AIRPORT_APRON               = 2634;
static const SpriteID SPR_AIRPORT_AIRCRAFT_STAND      = 2635;
static const SpriteID SPR_AIRPORT_TAXIWAY_NS_WEST     = 2636;
static const SpriteID SPR_AIRPORT_TAXIWAY_EW_SOUTH    = 2637;
static const SpriteID SPR_AIRPORT_TAXIWAY_XING_SOUTH  = 2638;
static const SpriteID SPR_AIRPORT_TAXIWAY_XING_WEST   = 2639;
static const SpriteID SPR_AIRPORT_TAXIWAY_NS_CTR      = 2640;
static const SpriteID SPR_AIRPORT_TAXIWAY_XING_EAST   = 2641;
static const SpriteID SPR_AIRPORT_TAXIWAY_NS_EAST     = 2642;
static const SpriteID SPR_AIRPORT_TAXIWAY_EW_NORTH    = 2643;
static const SpriteID SPR_AIRPORT_TAXIWAY_EW_CTR      = 2644;
static const SpriteID SPR_AIRPORT_RUNWAY_EXIT_A       = 2645;
static const SpriteID SPR_AIRPORT_RUNWAY_EXIT_B       = 2646;
static const SpriteID SPR_AIRPORT_RUNWAY_EXIT_C       = 2647;
static const SpriteID SPR_AIRPORT_RUNWAY_EXIT_D       = 2648;
static const SpriteID SPR_AIRPORT_RUNWAY_END          = 2649; // We should have different ends
static const SpriteID SPR_AIRPORT_TERMINAL_A          = 2650;
static const SpriteID SPR_AIRPORT_TOWER               = 2651;
static const SpriteID SPR_AIRPORT_CONCOURSE           = 2652;
static const SpriteID SPR_AIRPORT_TERMINAL_B          = 2653;
static const SpriteID SPR_AIRPORT_TERMINAL_C          = 2654;
static const SpriteID SPR_AIRPORT_HANGAR_FRONT        = 2655;
static const SpriteID SPR_AIRPORT_HANGAR_REAR         = 2656;
static const SpriteID SPR_AIRFIELD_HANGAR_FRONT       = 2657;
static const SpriteID SPR_AIRFIELD_HANGAR_REAR        = 2658;
static const SpriteID SPR_AIRPORT_JETWAY_1            = 2659;
static const SpriteID SPR_AIRPORT_JETWAY_2            = 2660;
static const SpriteID SPR_AIRPORT_JETWAY_3            = 2661;
static const SpriteID SPR_AIRPORT_PASSENGER_TUNNEL    = 2662;
static const SpriteID SPR_AIRPORT_FENCE_Y             = 2663;
static const SpriteID SPR_AIRPORT_FENCE_X             = 2664;
static const SpriteID SPR_AIRFIELD_TERM_A             = 2665;
static const SpriteID SPR_AIRFIELD_TERM_B             = 2666;
static const SpriteID SPR_AIRFIELD_TERM_C_GROUND      = 2667;
static const SpriteID SPR_AIRFIELD_TERM_C_BUILD       = 2668;
static const SpriteID SPR_AIRFIELD_APRON_A            = 2669;
static const SpriteID SPR_AIRFIELD_APRON_B            = 2670;
static const SpriteID SPR_AIRFIELD_APRON_C            = 2671;
static const SpriteID SPR_AIRFIELD_APRON_D            = 2672;
static const SpriteID SPR_AIRFIELD_RUNWAY_NEAR_END    = 2673;
static const SpriteID SPR_AIRFIELD_RUNWAY_MIDDLE      = 2674;
static const SpriteID SPR_AIRFIELD_RUNWAY_FAR_END     = 2675;
static const SpriteID SPR_AIRFIELD_WIND_1             = 2676;
static const SpriteID SPR_AIRFIELD_WIND_2             = 2677;
static const SpriteID SPR_AIRFIELD_WIND_3             = 2678;
static const SpriteID SPR_AIRFIELD_WIND_4             = 2679;
static const SpriteID SPR_AIRPORT_RADAR_1             = 2680;
static const SpriteID SPR_AIRPORT_RADAR_2             = 2681;
static const SpriteID SPR_AIRPORT_RADAR_3             = 2682;
static const SpriteID SPR_AIRPORT_RADAR_4             = 2683;
static const SpriteID SPR_AIRPORT_RADAR_5             = 2684;
static const SpriteID SPR_AIRPORT_RADAR_6             = 2685;
static const SpriteID SPR_AIRPORT_RADAR_7             = 2686;
static const SpriteID SPR_AIRPORT_RADAR_8             = 2687;
static const SpriteID SPR_AIRPORT_RADAR_9             = 2688;
static const SpriteID SPR_AIRPORT_RADAR_A             = 2689;
static const SpriteID SPR_AIRPORT_RADAR_B             = 2690;
static const SpriteID SPR_AIRPORT_RADAR_C             = 2691;
static const SpriteID SPR_AIRPORT_HELIPAD             = SPR_OPENTTD_BASE + 86;
static const SpriteID SPR_AIRPORT_HELIDEPOT_OFFICE    = 2095;

/* Road Stops
 * Road stops have a ground tile and 3 buildings, one on each side
 * (except the side where the entry is). These are marked _A _B and _C */
static const SpriteID SPR_BUS_STOP_NE_GROUND          = 2692;
static const SpriteID SPR_BUS_STOP_SE_GROUND          = 2693;
static const SpriteID SPR_BUS_STOP_SW_GROUND          = 2694;
static const SpriteID SPR_BUS_STOP_NW_GROUND          = 2695;
static const SpriteID SPR_BUS_STOP_NE_BUILD_A         = 2696;
static const SpriteID SPR_BUS_STOP_SE_BUILD_A         = 2697;
static const SpriteID SPR_BUS_STOP_SW_BUILD_A         = 2698;
static const SpriteID SPR_BUS_STOP_NW_BUILD_A         = 2699;
static const SpriteID SPR_BUS_STOP_NE_BUILD_B         = 2700;
static const SpriteID SPR_BUS_STOP_SE_BUILD_B         = 2701;
static const SpriteID SPR_BUS_STOP_SW_BUILD_B         = 2702;
static const SpriteID SPR_BUS_STOP_NW_BUILD_B         = 2703;
static const SpriteID SPR_BUS_STOP_NE_BUILD_C         = 2704;
static const SpriteID SPR_BUS_STOP_SE_BUILD_C         = 2705;
static const SpriteID SPR_BUS_STOP_SW_BUILD_C         = 2706;
static const SpriteID SPR_BUS_STOP_NW_BUILD_C         = 2707;
static const SpriteID SPR_TRUCK_STOP_NE_GROUND        = 2708;
static const SpriteID SPR_TRUCK_STOP_SE_GROUND        = 2709;
static const SpriteID SPR_TRUCK_STOP_SW_GROUND        = 2710;
static const SpriteID SPR_TRUCK_STOP_NW_GROUND        = 2711;
static const SpriteID SPR_TRUCK_STOP_NE_BUILD_A       = 2712;
static const SpriteID SPR_TRUCK_STOP_SE_BUILD_A       = 2713;
static const SpriteID SPR_TRUCK_STOP_SW_BUILD_A       = 2714;
static const SpriteID SPR_TRUCK_STOP_NW_BUILD_A       = 2715;
static const SpriteID SPR_TRUCK_STOP_NE_BUILD_B       = 2716;
static const SpriteID SPR_TRUCK_STOP_SE_BUILD_B       = 2717;
static const SpriteID SPR_TRUCK_STOP_SW_BUILD_B       = 2718;
static const SpriteID SPR_TRUCK_STOP_NW_BUILD_B       = 2719;
static const SpriteID SPR_TRUCK_STOP_NE_BUILD_C       = 2720;
static const SpriteID SPR_TRUCK_STOP_SE_BUILD_C       = 2721;
static const SpriteID SPR_TRUCK_STOP_SW_BUILD_C       = 2722;
static const SpriteID SPR_TRUCK_STOP_NW_BUILD_C       = 2723;

/* Sprites for docks
 * Docks consist of two tiles, the sloped one and the flat one */
static const SpriteID SPR_DOCK_SLOPE_NE               = 2727;
static const SpriteID SPR_DOCK_SLOPE_SE               = 2728;
static const SpriteID SPR_DOCK_SLOPE_SW               = 2729;
static const SpriteID SPR_DOCK_SLOPE_NW               = 2730;
static const SpriteID SPR_DOCK_FLAT_X                 = 2731; // for NE and SW
static const SpriteID SPR_DOCK_FLAT_Y                 = 2732; // for NW and SE
static const SpriteID SPR_BUOY                        = 4076; // XXX this sucks, because it displays wrong stuff on canals

/* Sprites for road */
static const SpriteID SPR_ROAD_Y                  = 1332;
static const SpriteID SPR_ROAD_X                  = 1333;
static const SpriteID SPR_ROAD_SLOPE_START        = 1343;
static const SpriteID SPR_ROAD_Y_SNOW             = 1351;
static const SpriteID SPR_ROAD_X_SNOW             = 1352;
/* see _road_sloped_sprites_offset in road_cmd.cpp for offsets for sloped road tiles */
static const SpriteID SPR_ROAD_DEPOT              = 1408;

static const SpriteID SPR_EXCAVATION_X = 1414;
static const SpriteID SPR_EXCAVATION_Y = 1415;

/* Landscape sprites */
static const SpriteID SPR_FLAT_BARE_LAND                = 3924;
static const SpriteID SPR_FLAT_1_THIRD_GRASS_TILE       = 3943;
static const SpriteID SPR_FLAT_2_THIRD_GRASS_TILE       = 3962;
static const SpriteID SPR_FLAT_GRASS_TILE               = 3981;
static const SpriteID SPR_FLAT_ROUGH_LAND               = 4000;
static const SpriteID SPR_FLAT_ROUGH_LAND_1             = 4019;
static const SpriteID SPR_FLAT_ROUGH_LAND_2             = 4020;
static const SpriteID SPR_FLAT_ROUGH_LAND_3             = 4021;
static const SpriteID SPR_FLAT_ROUGH_LAND_4             = 4022;
static const SpriteID SPR_FLAT_ROCKY_LAND_1             = 4023;
static const SpriteID SPR_FLAT_ROCKY_LAND_2             = 4042;
static const SpriteID SPR_FLAT_WATER_TILE               = 4061;
static const SpriteID SPR_FLAT_1_QUART_SNOW_DESERT_TILE = 4493;
static const SpriteID SPR_FLAT_2_QUART_SNOW_DESERT_TILE = 4512;
static const SpriteID SPR_FLAT_3_QUART_SNOW_DESERT_TILE = 4531;
static const SpriteID SPR_FLAT_SNOW_DESERT_TILE         = 4550;

/* Hedge, Farmland-fence sprites */
static const SpriteID SPR_HEDGE_BUSHES            = 4090;
static const SpriteID SPR_HEDGE_BUSHES_WITH_GATE  = 4096;
static const SpriteID SPR_HEDGE_FENCE             = 4102;
static const SpriteID SPR_HEDGE_BLOOMBUSH_YELLOW  = 4108;
static const SpriteID SPR_HEDGE_BLOOMBUSH_RED     = 4114;
static const SpriteID SPR_HEDGE_STONE             = 4120;

/* Farmland sprites, only flat tiles listed, various stages */
static const SpriteID SPR_FARMLAND_BARE           = 4126;
static const SpriteID SPR_FARMLAND_STATE_1        = 4145;
static const SpriteID SPR_FARMLAND_STATE_2        = 4164;
static const SpriteID SPR_FARMLAND_STATE_3        = 4183;
static const SpriteID SPR_FARMLAND_STATE_4        = 4202;
static const SpriteID SPR_FARMLAND_STATE_5        = 4221;
static const SpriteID SPR_FARMLAND_STATE_6        = 4240;
static const SpriteID SPR_FARMLAND_STATE_7        = 4259;
static const SpriteID SPR_FARMLAND_HAYPACKS       = 4278;

/* Water-related sprites */
static const SpriteID SPR_SHIP_DEPOT_SE_FRONT     = 4070;
static const SpriteID SPR_SHIP_DEPOT_SW_FRONT     = 4071;
static const SpriteID SPR_SHIP_DEPOT_NW           = 4072;
static const SpriteID SPR_SHIP_DEPOT_NE           = 4073;
static const SpriteID SPR_SHIP_DEPOT_SE_REAR      = 4074;
static const SpriteID SPR_SHIP_DEPOT_SW_REAR      = 4075;
/* here come sloped water sprites */
static const SpriteID SPR_WATER_SLOPE_Y_UP        = SPR_CANALS_BASE + 0; // Water flowing negative Y direction
static const SpriteID SPR_WATER_SLOPE_X_DOWN      = SPR_CANALS_BASE + 1; // positive X
static const SpriteID SPR_WATER_SLOPE_X_UP        = SPR_CANALS_BASE + 2; // negative X
static const SpriteID SPR_WATER_SLOPE_Y_DOWN      = SPR_CANALS_BASE + 3; // positive Y
/* sprites for the locks
 * there are 4 kinds of locks, each of them is 3 tiles long.
 * the four kinds are running in the X and Y direction and
 * are "lowering" either in the "+" or the "-" direction.
 * the three tiles are the center tile (where the slope is)
 * and a bottom and a top tile */
static const SpriteID SPR_LOCK_BASE                 = SPR_CANALS_BASE +  4;
static const SpriteID SPR_LOCK_Y_UP_CENTER_REAR     = SPR_CANALS_BASE +  4;
static const SpriteID SPR_LOCK_X_DOWN_CENTER_REAR   = SPR_CANALS_BASE +  5;
static const SpriteID SPR_LOCK_X_UP_CENTER_REAR     = SPR_CANALS_BASE +  6;
static const SpriteID SPR_LOCK_Y_DOWN_CENTER_REAR   = SPR_CANALS_BASE +  7;
static const SpriteID SPR_LOCK_Y_UP_CENTER_FRONT    = SPR_CANALS_BASE +  8;
static const SpriteID SPR_LOCK_X_DOWN_CENTER_FRONT  = SPR_CANALS_BASE +  9;
static const SpriteID SPR_LOCK_X_UP_CENTER_FRONT    = SPR_CANALS_BASE + 10;
static const SpriteID SPR_LOCK_Y_DOWN_CENTER_FRONT  = SPR_CANALS_BASE + 11;
static const SpriteID SPR_LOCK_Y_UP_BOTTOM_REAR     = SPR_CANALS_BASE + 12;
static const SpriteID SPR_LOCK_X_DOWN_BOTTOM_REAR   = SPR_CANALS_BASE + 13;
static const SpriteID SPR_LOCK_X_UP_BOTTOM_REAR     = SPR_CANALS_BASE + 14;
static const SpriteID SPR_LOCK_Y_DOWN_BOTTOM_REAR   = SPR_CANALS_BASE + 15;
static const SpriteID SPR_LOCK_Y_UP_BOTTOM_FRONT    = SPR_CANALS_BASE + 16;
static const SpriteID SPR_LOCK_X_DOWN_BOTTOM_FRONT  = SPR_CANALS_BASE + 17;
static const SpriteID SPR_LOCK_X_UP_BOTTOM_FRONT    = SPR_CANALS_BASE + 18;
static const SpriteID SPR_LOCK_Y_DOWN_BOTTOM_FRONT  = SPR_CANALS_BASE + 19;
static const SpriteID SPR_LOCK_Y_UP_TOP_REAR        = SPR_CANALS_BASE + 20;
static const SpriteID SPR_LOCK_X_DOWN_TOP_REAR      = SPR_CANALS_BASE + 21;
static const SpriteID SPR_LOCK_X_UP_TOP_REAR        = SPR_CANALS_BASE + 22;
static const SpriteID SPR_LOCK_Y_DOWN_TOP_REAR      = SPR_CANALS_BASE + 23;
static const SpriteID SPR_LOCK_Y_UP_TOP_FRONT       = SPR_CANALS_BASE + 24;
static const SpriteID SPR_LOCK_X_DOWN_TOP_FRONT     = SPR_CANALS_BASE + 25;
static const SpriteID SPR_LOCK_X_UP_TOP_FRONT       = SPR_CANALS_BASE + 26;
static const SpriteID SPR_LOCK_Y_DOWN_TOP_FRONT     = SPR_CANALS_BASE + 27;
static const SpriteID SPR_CANAL_DIKES_BASE          = SPR_CANALS_BASE + 52;

/* Sprites for tunnels and bridges */
static const SpriteID SPR_TUNNEL_ENTRY_REAR_RAIL   = 2365;
static const SpriteID SPR_TUNNEL_ENTRY_REAR_MONO   = 2373;
static const SpriteID SPR_TUNNEL_ENTRY_REAR_MAGLEV = 2381;
static const SpriteID SPR_TUNNEL_ENTRY_REAR_ROAD   = 2389;

/* Level crossings */
static const SpriteID SPR_CROSSING_OFF_X_RAIL   = 1370;
static const SpriteID SPR_CROSSING_OFF_X_MONO   = 1382;
static const SpriteID SPR_CROSSING_OFF_X_MAGLEV = 1394;

/* bridge type sprites */
static const SpriteID SPR_PILLARS_BASE = SPR_OPENTTD_BASE + 14;

/* Wooden bridge (type 0) */
static const SpriteID SPR_BTWDN_RAIL_Y_REAR       = 2545;
static const SpriteID SPR_BTWDN_RAIL_X_REAR       = 2546;
static const SpriteID SPR_BTWDN_ROAD_Y_REAR       = 2547;
static const SpriteID SPR_BTWDN_ROAD_X_REAR       = 2548;
static const SpriteID SPR_BTWDN_Y_FRONT           = 2549;
static const SpriteID SPR_BTWDN_X_FRONT           = 2550;
static const SpriteID SPR_BTWDN_Y_PILLAR          = 2551;
static const SpriteID SPR_BTWDN_X_PILLAR          = 2552;
static const SpriteID SPR_BTWDN_MONO_Y_REAR       = 4360;
static const SpriteID SPR_BTWDN_MONO_X_REAR       = 4361;
static const SpriteID SPR_BTWDN_MGLV_Y_REAR       = 4400;
static const SpriteID SPR_BTWDN_MGLV_X_REAR       = 4401;
/* ramps */
static const SpriteID SPR_BTWDN_ROAD_RAMP_Y_DOWN  = 2529;
static const SpriteID SPR_BTWDN_ROAD_RAMP_X_DOWN  = 2530;
static const SpriteID SPR_BTWDN_ROAD_RAMP_X_UP    = 2531; // for some weird reason the order is swapped
static const SpriteID SPR_BTWDN_ROAD_RAMP_Y_UP    = 2532; // between X and Y.
static const SpriteID SPR_BTWDN_ROAD_Y_SLOPE_UP   = 2533;
static const SpriteID SPR_BTWDN_ROAD_X_SLOPE_UP   = 2534;
static const SpriteID SPR_BTWDN_ROAD_Y_SLOPE_DOWN = 2535;
static const SpriteID SPR_BTWDN_ROAD_X_SLOPE_DOWN = 2536;
static const SpriteID SPR_BTWDN_RAIL_RAMP_Y_DOWN  = 2537;
static const SpriteID SPR_BTWDN_RAIL_RAMP_X_DOWN  = 2538;
static const SpriteID SPR_BTWDN_RAIL_RAMP_X_UP    = 2539; // for some weird reason the order is swapped
static const SpriteID SPR_BTWDN_RAIL_RAMP_Y_UP    = 2540; // between X and Y.
static const SpriteID SPR_BTWDN_RAIL_Y_SLOPE_UP   = 2541;
static const SpriteID SPR_BTWDN_RAIL_X_SLOPE_UP   = 2542;
static const SpriteID SPR_BTWDN_RAIL_Y_SLOPE_DOWN = 2543;
static const SpriteID SPR_BTWDN_RAIL_X_SLOPE_DOWN = 2544;
static const SpriteID SPR_BTWDN_MONO_RAMP_Y_DOWN  = 4352;
static const SpriteID SPR_BTWDN_MONO_RAMP_X_DOWN  = 4353;
static const SpriteID SPR_BTWDN_MONO_RAMP_X_UP    = 4354; // for some weird reason the order is swapped
static const SpriteID SPR_BTWDN_MONO_RAMP_Y_UP    = 4355; // between X and Y.
static const SpriteID SPR_BTWDN_MONO_Y_SLOPE_UP   = 4356;
static const SpriteID SPR_BTWDN_MONO_X_SLOPE_UP   = 4357;
static const SpriteID SPR_BTWDN_MONO_Y_SLOPE_DOWN = 4358;
static const SpriteID SPR_BTWDN_MONO_X_SLOPE_DOWN = 4359;
static const SpriteID SPR_BTWDN_MGLV_RAMP_Y_DOWN  = 4392;
static const SpriteID SPR_BTWDN_MGLV_RAMP_X_DOWN  = 4393;
static const SpriteID SPR_BTWDN_MGLV_RAMP_X_UP    = 4394; // for some weird reason the order is swapped
static const SpriteID SPR_BTWDN_MGLV_RAMP_Y_UP    = 4395; // between X and Y.
static const SpriteID SPR_BTWDN_MGLV_Y_SLOPE_UP   = 4396;
static const SpriteID SPR_BTWDN_MGLV_X_SLOPE_UP   = 4397;
static const SpriteID SPR_BTWDN_MGLV_Y_SLOPE_DOWN = 4398;
static const SpriteID SPR_BTWDN_MGLV_X_SLOPE_DOWN = 4399;

/* Steel Girder with arches
 * BTSGA == Bridge Type Steel Girder Arched
 * This is bridge type number 2 */
static const SpriteID SPR_BTSGA_RAIL_X_REAR       = 2499;
static const SpriteID SPR_BTSGA_RAIL_Y_REAR       = 2500;
static const SpriteID SPR_BTSGA_ROAD_X_REAR       = 2501;
static const SpriteID SPR_BTSGA_ROAD_Y_REAR       = 2502;
static const SpriteID SPR_BTSGA_X_FRONT           = 2503;
static const SpriteID SPR_BTSGA_Y_FRONT           = 2504;
static const SpriteID SPR_BTSGA_X_PILLAR          = 2505;
static const SpriteID SPR_BTSGA_Y_PILLAR          = 2506;
static const SpriteID SPR_BTSGA_MONO_X_REAR       = 4324;
static const SpriteID SPR_BTSGA_MONO_Y_REAR       = 4325;
static const SpriteID SPR_BTSGA_MGLV_X_REAR       = 4364;
static const SpriteID SPR_BTSGA_MGLV_Y_REAR       = 4365;

/* BTSUS == Suspension bridge
 * TILE_* denotes the different tiles a suspension bridge
 * can have
 * TILE_A and TILE_B are the "beginnings" and "ends" of the
 *   suspension system. They have small rectangular endcaps
 * TILE_C and TILE_D look almost identical to TILE_A and
 *   TILE_B, but they do not have the "endcaps". They form the
 *   middle part
 * TILE_E is a condensed configuration of two pillars. while they
 *   are usually 2 pillars apart, they only have 1 pillar separation
 *   here
 * TILE_F is an extended configuration of pillars. They are
 *   plugged in when pillars should be 3 tiles apart
 */
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_A  = 2453;
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_B  = 2454;
static const SpriteID SPR_BTSUS_Y_FRONT_TILE_A      = 2455;
static const SpriteID SPR_BTSUS_Y_FRONT_TILE_B      = 2456;
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_D  = 2457;
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_C  = 2458;
static const SpriteID SPR_BTSUS_Y_FRONT_TILE_D      = 2459;
static const SpriteID SPR_BTSUS_Y_FRONT_TILE_C      = 2460;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_A  = 2461;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_B  = 2462;
static const SpriteID SPR_BTSUS_X_FRONT_TILE_A      = 2463;
static const SpriteID SPR_BTSUS_X_FRONT_TILE_B      = 2464;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_D  = 2465;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_C  = 2466;
static const SpriteID SPR_BTSUS_X_FRONT_TILE_D      = 2467;
static const SpriteID SPR_BTSUS_X_FRONT_TILE_C      = 2468;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_A  = 2469;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_B  = 2470;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_D  = 2471;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_C  = 2472;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_A  = 2473;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_B  = 2474;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_D  = 2475;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_C  = 2476;
static const SpriteID SPR_BTSUS_Y_PILLAR_TILE_A     = 2477;
static const SpriteID SPR_BTSUS_Y_PILLAR_TILE_B     = 2478;
static const SpriteID SPR_BTSUS_Y_PILLAR_TILE_D     = 2479;
static const SpriteID SPR_BTSUS_Y_PILLAR_TILE_C     = 2480;
static const SpriteID SPR_BTSUS_X_PILLAR_TILE_A     = 2481;
static const SpriteID SPR_BTSUS_X_PILLAR_TILE_B     = 2482;
static const SpriteID SPR_BTSUS_X_PILLAR_TILE_D     = 2483;
static const SpriteID SPR_BTSUS_X_PILLAR_TILE_C     = 2484;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_E  = 2485;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_E  = 2486;
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_E  = 2487;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_E  = 2488;
static const SpriteID SPR_BTSUS_Y_FRONT_TILE_E      = 2489;
static const SpriteID SPR_BTSUS_X_FRONT_TILE_E      = 2490;
static const SpriteID SPR_BTSUS_Y_PILLAR_TILE_E     = 2491;
static const SpriteID SPR_BTSUS_X_PILLAR_TILE_E     = 2492;
static const SpriteID SPR_BTSUS_RAIL_X_REAR_TILE_F  = 2493;
static const SpriteID SPR_BTSUS_RAIL_Y_REAR_TILE_F  = 2494;
static const SpriteID SPR_BTSUS_ROAD_X_REAR_TILE_F  = 2495;
static const SpriteID SPR_BTSUS_ROAD_Y_REAR_TILE_F  = 2496;
static const SpriteID SPR_BTSUS_X_FRONT             = 2497;
static const SpriteID SPR_BTSUS_Y_FRONT             = 2498;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_A  = 4334;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_B  = 4335;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_D  = 4336;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_C  = 4337;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_A  = 4338;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_B  = 4339;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_D  = 4340;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_C  = 4341;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_E  = 4342;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_E  = 4343;
static const SpriteID SPR_BTSUS_MONO_X_REAR_TILE_F  = 4344;
static const SpriteID SPR_BTSUS_MONO_Y_REAR_TILE_F  = 4345;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_A  = 4374;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_B  = 4375;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_D  = 4376;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_C  = 4377;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_A  = 4378;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_B  = 4379;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_D  = 4380;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_C  = 4381;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_E  = 4382;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_E  = 4383;
static const SpriteID SPR_BTSUS_MGLV_X_REAR_TILE_F  = 4384;
static const SpriteID SPR_BTSUS_MGLV_Y_REAR_TILE_F  = 4385;

/* cantilever bridges
 * They have three different kinds of tiles:
 * END(ing), MID(dle), BEG(ginning) */
static const SpriteID SPR_BTCAN_RAIL_X_BEG          = 2507;
static const SpriteID SPR_BTCAN_RAIL_X_MID          = 2508;
static const SpriteID SPR_BTCAN_RAIL_X_END          = 2509;
static const SpriteID SPR_BTCAN_RAIL_Y_END          = 2510;
static const SpriteID SPR_BTCAN_RAIL_Y_MID          = 2511;
static const SpriteID SPR_BTCAN_RAIL_Y_BEG          = 2512;
static const SpriteID SPR_BTCAN_ROAD_X_BEG          = 2513;
static const SpriteID SPR_BTCAN_ROAD_X_MID          = 2514;
static const SpriteID SPR_BTCAN_ROAD_X_END          = 2515;
static const SpriteID SPR_BTCAN_ROAD_Y_END          = 2516;
static const SpriteID SPR_BTCAN_ROAD_Y_MID          = 2517;
static const SpriteID SPR_BTCAN_ROAD_Y_BEG          = 2518;
static const SpriteID SPR_BTCAN_X_FRONT_BEG         = 2519;
static const SpriteID SPR_BTCAN_X_FRONT_MID         = 2520;
static const SpriteID SPR_BTCAN_X_FRONT_END         = 2521;
static const SpriteID SPR_BTCAN_Y_FRONT_END         = 2522;
static const SpriteID SPR_BTCAN_Y_FRONT_MID         = 2523;
static const SpriteID SPR_BTCAN_Y_FRONT_BEG         = 2524;
static const SpriteID SPR_BTCAN_X_PILLAR_BEG        = 2525;
static const SpriteID SPR_BTCAN_X_PILLAR_MID        = 2526;
static const SpriteID SPR_BTCAN_Y_PILLAR_MID        = 2527;
static const SpriteID SPR_BTCAN_Y_PILLAR_BEG        = 2528;
static const SpriteID SPR_BTCAN_MONO_X_BEG          = 4346;
static const SpriteID SPR_BTCAN_MONO_X_MID          = 4347;
static const SpriteID SPR_BTCAN_MONO_X_END          = 4348;
static const SpriteID SPR_BTCAN_MONO_Y_END          = 4349;
static const SpriteID SPR_BTCAN_MONO_Y_MID          = 4350;
static const SpriteID SPR_BTCAN_MONO_Y_BEG          = 4351;
static const SpriteID SPR_BTCAN_MGLV_X_BEG          = 4386;
static const SpriteID SPR_BTCAN_MGLV_X_MID          = 4387;
static const SpriteID SPR_BTCAN_MGLV_X_END          = 4388;
static const SpriteID SPR_BTCAN_MGLV_Y_END          = 4389;
static const SpriteID SPR_BTCAN_MGLV_Y_MID          = 4390;
static const SpriteID SPR_BTCAN_MGLV_Y_BEG          = 4391;

/* little concrete bridge */
static const SpriteID SPR_BTCON_RAIL_X        = 2493;
static const SpriteID SPR_BTCON_RAIL_Y        = 2494;
static const SpriteID SPR_BTCON_ROAD_X        = 2495;
static const SpriteID SPR_BTCON_ROAD_Y        = 2496;
static const SpriteID SPR_BTCON_X_FRONT       = 2497;
static const SpriteID SPR_BTCON_Y_FRONT       = 2498;
static const SpriteID SPR_BTCON_X_PILLAR      = 2505;
static const SpriteID SPR_BTCON_Y_PILLAR      = 2506;
static const SpriteID SPR_BTCON_MONO_X        = 4344;
static const SpriteID SPR_BTCON_MONO_Y        = 4345;
static const SpriteID SPR_BTCON_MGLV_X        = 4384;
static const SpriteID SPR_BTCON_MGLV_Y        = 4385;

/* little steel girder bridge */
static const SpriteID SPR_BTGIR_RAIL_X        = 2553;
static const SpriteID SPR_BTGIR_RAIL_Y        = 2554;
static const SpriteID SPR_BTGIR_ROAD_X        = 2555;
static const SpriteID SPR_BTGIR_ROAD_Y        = 2556;
static const SpriteID SPR_BTGIR_X_FRONT       = 2557;
static const SpriteID SPR_BTGIR_Y_FRONT       = 2558;
static const SpriteID SPR_BTGIR_X_PILLAR      = 2505;
static const SpriteID SPR_BTGIR_Y_PILLAR      = 2506;
static const SpriteID SPR_BTGIR_MONO_X        = 4362;
static const SpriteID SPR_BTGIR_MONO_Y        = 4363;
static const SpriteID SPR_BTGIR_MGLV_X        = 4402;
static const SpriteID SPR_BTGIR_MGLV_Y        = 4403;

/* tubular bridges
 * tubular bridges have 3 kinds of tiles:
 *  a start tile (with only half a tube on the far side, marked _BEG
 *  a middle tile (full tunnel), marked _MID
 *  and an end tile (half a tube on the near side, marked _END
 */
static const SpriteID SPR_BTTUB_X_FRONT_BEG       = 2559;
static const SpriteID SPR_BTTUB_X_FRONT_MID       = 2560;
static const SpriteID SPR_BTTUB_X_FRONT_END       = 2561;
static const SpriteID SPR_BTTUB_Y_FRONT_END       = 2562;
static const SpriteID SPR_BTTUB_Y_FRONT_MID       = 2563;
static const SpriteID SPR_BTTUB_Y_FRONT_BEG       = 2564;
static const SpriteID SPR_BTTUB_X_PILLAR_BEG      = 2565;
static const SpriteID SPR_BTTUB_X_PILLAR_MID      = 2566;
static const SpriteID SPR_BTTUB_Y_PILLAR_MID      = 2567;
static const SpriteID SPR_BTTUB_Y_PILLAR_BEG      = 2568;
static const SpriteID SPR_BTTUB_X_RAIL_REAR_BEG   = 2569;
static const SpriteID SPR_BTTUB_X_RAIL_REAR_MID   = 2570;
static const SpriteID SPR_BTTUB_X_RAIL_REAR_END   = 2571;
static const SpriteID SPR_BTTUB_Y_RAIL_REAR_BEG   = 2572;
static const SpriteID SPR_BTTUB_Y_RAIL_REAR_MID   = 2573;
static const SpriteID SPR_BTTUB_Y_RAIL_REAR_END   = 2574;
static const SpriteID SPR_BTTUB_X_ROAD_REAR_BEG   = 2575;
static const SpriteID SPR_BTTUB_X_ROAD_REAR_MID   = 2576;
static const SpriteID SPR_BTTUB_X_ROAD_REAR_END   = 2577;
static const SpriteID SPR_BTTUB_Y_ROAD_REAR_BEG   = 2578;
static const SpriteID SPR_BTTUB_Y_ROAD_REAR_MID   = 2579;
static const SpriteID SPR_BTTUB_Y_ROAD_REAR_END   = 2580;
static const SpriteID SPR_BTTUB_X_MONO_REAR_BEG   = 2581;
static const SpriteID SPR_BTTUB_X_MONO_REAR_MID   = 2582;
static const SpriteID SPR_BTTUB_X_MONO_REAR_END   = 2583;
static const SpriteID SPR_BTTUB_Y_MONO_REAR_BEG   = 2584;
static const SpriteID SPR_BTTUB_Y_MONO_REAR_MID   = 2585;
static const SpriteID SPR_BTTUB_Y_MONO_REAR_END   = 2586;
static const SpriteID SPR_BTTUB_X_MGLV_REAR_BEG   = 2587;
static const SpriteID SPR_BTTUB_X_MGLV_REAR_MID   = 2588;
static const SpriteID SPR_BTTUB_X_MGLV_REAR_END   = 2589;
static const SpriteID SPR_BTTUB_Y_MGLV_REAR_BEG   = 2590;
static const SpriteID SPR_BTTUB_Y_MGLV_REAR_MID   = 2591;
static const SpriteID SPR_BTTUB_Y_MGLV_REAR_END   = 2592;


/* ramps (for all bridges except wood and tubular?)*/
static const SpriteID SPR_BTGEN_RAIL_X_SLOPE_DOWN = 2437;
static const SpriteID SPR_BTGEN_RAIL_X_SLOPE_UP   = 2438;
static const SpriteID SPR_BTGEN_RAIL_Y_SLOPE_DOWN = 2439;
static const SpriteID SPR_BTGEN_RAIL_Y_SLOPE_UP   = 2440;
static const SpriteID SPR_BTGEN_RAIL_RAMP_X_UP    = 2441;
static const SpriteID SPR_BTGEN_RAIL_RAMP_X_DOWN  = 2442;
static const SpriteID SPR_BTGEN_RAIL_RAMP_Y_UP    = 2443;
static const SpriteID SPR_BTGEN_RAIL_RAMP_Y_DOWN  = 2444;
static const SpriteID SPR_BTGEN_ROAD_X_SLOPE_DOWN = 2445;
static const SpriteID SPR_BTGEN_ROAD_X_SLOPE_UP   = 2446;
static const SpriteID SPR_BTGEN_ROAD_Y_SLOPE_DOWN = 2447;
static const SpriteID SPR_BTGEN_ROAD_Y_SLOPE_UP   = 2448;
static const SpriteID SPR_BTGEN_ROAD_RAMP_X_UP    = 2449;
static const SpriteID SPR_BTGEN_ROAD_RAMP_X_DOWN  = 2450;
static const SpriteID SPR_BTGEN_ROAD_RAMP_Y_UP    = 2451;
static const SpriteID SPR_BTGEN_ROAD_RAMP_Y_DOWN  = 2452;
static const SpriteID SPR_BTGEN_MONO_X_SLOPE_DOWN = 4326;
static const SpriteID SPR_BTGEN_MONO_X_SLOPE_UP   = 4327;
static const SpriteID SPR_BTGEN_MONO_Y_SLOPE_DOWN = 4328;
static const SpriteID SPR_BTGEN_MONO_Y_SLOPE_UP   = 4329;
static const SpriteID SPR_BTGEN_MONO_RAMP_X_UP    = 4330;
static const SpriteID SPR_BTGEN_MONO_RAMP_X_DOWN  = 4331;
static const SpriteID SPR_BTGEN_MONO_RAMP_Y_UP    = 4332;
static const SpriteID SPR_BTGEN_MONO_RAMP_Y_DOWN  = 4333;
static const SpriteID SPR_BTGEN_MGLV_X_SLOPE_DOWN = 4366;
static const SpriteID SPR_BTGEN_MGLV_X_SLOPE_UP   = 4367;
static const SpriteID SPR_BTGEN_MGLV_Y_SLOPE_DOWN = 4368;
static const SpriteID SPR_BTGEN_MGLV_Y_SLOPE_UP   = 4369;
static const SpriteID SPR_BTGEN_MGLV_RAMP_X_UP    = 4370;
static const SpriteID SPR_BTGEN_MGLV_RAMP_X_DOWN  = 4371;
static const SpriteID SPR_BTGEN_MGLV_RAMP_Y_UP    = 4372;
static const SpriteID SPR_BTGEN_MGLV_RAMP_Y_DOWN  = 4373;

/* Vehicle view sprites */
static const SpriteID SPR_CENTRE_VIEW_VEHICLE   = 683;
static const SpriteID SPR_SEND_TRAIN_TODEPOT    = 685;
static const SpriteID SPR_SEND_ROADVEH_TODEPOT  = 686;
static const SpriteID SPR_SEND_AIRCRAFT_TODEPOT = 687;
static const SpriteID SPR_SEND_SHIP_TODEPOT     = 688;

static const SpriteID SPR_IGNORE_SIGNALS        = 689;
static const SpriteID SPR_SHOW_ORDERS           = 690;
static const SpriteID SPR_SHOW_VEHICLE_DETAILS  = 691;
static const SpriteID SPR_REFIT_VEHICLE         = 692;
static const SpriteID SPR_FORCE_VEHICLE_TURN    = 715;

/* Vehicle sprite-flags (red/green) */
static const SpriteID SPR_FLAG_VEH_STOPPED  = 3090;
static const SpriteID SPR_FLAG_VEH_RUNNING  = 3091;

static const SpriteID SPR_VEH_BUS_SW_VIEW   = 3097;
static const SpriteID SPR_VEH_BUS_SIDE_VIEW = 3098;

/* Rotor sprite numbers */
static const SpriteID SPR_ROTOR_STOPPED   = 3901;
static const SpriteID SPR_ROTOR_MOVING_1  = 3902;
static const SpriteID SPR_ROTOR_MOVING_3  = 3904;

/* Town/house sprites */
static const SpriteID SPR_LIFT = 1443;

/* used in town_land.h
 * CNST1..3 = Those are the different stages of construction
 * The last 2 hexas correspond to the type of building it represent, if any */
static const SpriteID SPR_CNST1_TALLOFFICE_00                 = 1421;
static const SpriteID SPR_CNST2_TALLOFFICE_00                 = 1422;
static const SpriteID SPR_CNST3_TALLOFFICE_00                 = 1423;
static const SpriteID SPR_GROUND_TALLOFFICE_00                = 1424;
static const SpriteID SPR_BUILD_TALLOFFICE_00                 = 1425; // temperate
static const SpriteID SPR_CNST1_OFFICE_01                     = 1426;
static const SpriteID SPR_CNST2_OFFICE_01                     = 1427;
static const SpriteID SPR_BUILD_OFFICE_01                     = 1428; // temperate
static const SpriteID SPR_GROUND_OFFICE_01                    = 1429;
static const SpriteID SPR_CNST1_SMLBLCKFLATS_02               = 1430; // Small Block of Flats
static const SpriteID SPR_CNST2_SMLBLCKFLATS_02               = 1431;
static const SpriteID SPR_BUILD_SMLBLCKFLATS_02               = 1432; // temperate
static const SpriteID SPR_GROUND_SMLBLCKFLATS_02              = 1433;
static const SpriteID SPR_CNST1_TEMPCHURCH                    = 1434;
static const SpriteID SPR_CNST2_TEMPCHURCH                    = 1435;
static const SpriteID SPR_BUILD_TEMPCHURCH                    = 1436;
static const SpriteID SPR_GROUND_TEMPCHURCH                   = 1437;
static const SpriteID SPR_CNST1_LARGEOFFICE_04                = 1440;
static const SpriteID SPR_CNST2_LARGEOFFICE_04                = 1441;
static const SpriteID SPR_BUILD_LARGEOFFICE_04                = 1442; // temperate, sub-arctic, subtropical
static const SpriteID SPR_BUILD_LARGEOFFICE_04_SNOW           = 4569; // same, with snow
/* These are in fact two houses for the same houseID.  so V1 and V2 */
static const SpriteID SPR_CNST1_TOWNHOUSE_06_V1               = 1444;
static const SpriteID SPR_CNST2_TOWNHOUSE_06_V1               = 1445;
static const SpriteID SPR_BUILD_TOWNHOUSE_06_V1               = 1446; // 1st variation
static const SpriteID SPR_GRND_TOWNHOUSE_06_V1                = 1447;
static const SpriteID SPR_GRND_STADIUM_N                      = 1479; // stadium ground at north
static const SpriteID SPR_GRND_STADIUM_E                      = 1480; // stadium ground at east
static const SpriteID SPR_GRND_STADIUM_W                      = 1481; // stadium ground at west
static const SpriteID SPR_GRND_STADIUM_S                      = 1482; // stadium ground at south
static const SpriteID SPR_CNST1_TOWNHOUSE_06_V2               = 1501; // used as ground, but is stage1
static const SpriteID SPR_CNST1_TOWNHOUSE_06_V2_P             = 1502; // pipes extensions for previous
static const SpriteID SPR_CNST2_TOWNHOUSE_06_V2_G             = 1503; // Ground of cnst stage 2
static const SpriteID SPR_CNST2_TOWNHOUSE_06_V2               = 1504; // real cnst stage 2
static const SpriteID SPR_GRND_TOWNHOUSE_06_V2                = 1505;
static const SpriteID SPR_BUILD_TOWNHOUSE_06_V2               = 1506; // 2nd variation
static const SpriteID SPR_CNST1_HOTEL_07_NW                   = 1448;
static const SpriteID SPR_CNST2_HOTEL_07_NW                   = 1449;
static const SpriteID SPR_BUILD_HOTEL_07_NW                   = 1450;
static const SpriteID SPR_CNST1_HOTEL_07_SE                   = 1451;
static const SpriteID SPR_CNST2_HOTEL_07_SE                   = 1452;
static const SpriteID SPR_BUILD_HOTEL_07_SE                   = 1453;
static const SpriteID SPR_STATUE_HORSERIDER_09                = 1454;
static const SpriteID SPR_FOUNTAIN_0A                         = 1455;
static const SpriteID SPR_PARKSTATUE_0B                       = 1456;
static const SpriteID SPR_PARKALLEY_0C                        = 1457;
static const SpriteID SPR_CNST1_OFFICE_0D                     = 1458;
static const SpriteID SPR_CNST2_OFFICE_0D                     = 1459;
static const SpriteID SPR_BUILD_OFFICE_0D                     = 1460;
static const SpriteID SPR_CNST1_SHOPOFFICE_0E                 = 1461;
static const SpriteID SPR_CNST2_SHOPOFFICE_0E                 = 1462;
static const SpriteID SPR_BUILD_SHOPOFFICE_0E                 = 1463;
static const SpriteID SPR_CNST1_SHOPOFFICE_0F                 = 1464;
static const SpriteID SPR_CNST2_SHOPOFFICE_0F                 = 1465;
static const SpriteID SPR_BUILD_SHOPOFFICE_0F                 = 1466;
static const SpriteID SPR_GRND_HOUSE_TOY1                     = 4675;
static const SpriteID SPR_GRND_HOUSE_TOY2                     = 4676;

/* Easter egg/disaster sprites */
static const SpriteID SPR_BLIMP                  = 3905; // Zeppelin
static const SpriteID SPR_BLIMP_CRASHING         = 3906;
static const SpriteID SPR_BLIMP_CRASHED          = 3907;
static const SpriteID SPR_UFO_SMALL_SCOUT        = 3908; // XCOM - UFO Defense
static const SpriteID SPR_UFO_SMALL_SCOUT_DARKER = 3909;
static const SpriteID SPR_SUB_SMALL_NE           = 3910; // Silent Service
static const SpriteID SPR_SUB_SMALL_SE           = 3911;
static const SpriteID SPR_SUB_SMALL_SW           = 3912;
static const SpriteID SPR_SUB_SMALL_NW           = 3913;
static const SpriteID SPR_SUB_LARGE_NE           = 3914;
static const SpriteID SPR_SUB_LARGE_SE           = 3915;
static const SpriteID SPR_SUB_LARGE_SW           = 3916;
static const SpriteID SPR_SUB_LARGE_NW           = 3917;
static const SpriteID SPR_F_15                   = 3918; // F-15 Strike Eagle
static const SpriteID SPR_F_15_FIRING            = 3919;
static const SpriteID SPR_UFO_HARVESTER          = 3920; // XCOM - UFO Defense
static const SpriteID SPR_XCOM_SKYRANGER         = 3921;
static const SpriteID SPR_AH_64A                 = 3922; // Gunship
static const SpriteID SPR_AH_64A_FIRING          = 3923;

/* main_gui.cpp */
static const SpriteID SPR_IMG_TERRAFORM_UP    = 694;
static const SpriteID SPR_IMG_TERRAFORM_DOWN  = 695;
static const SpriteID SPR_IMG_DYNAMITE        = 703;
static const SpriteID SPR_IMG_ROCKS           = 4084;
static const SpriteID SPR_IMG_DESERT          = 4085;
static const SpriteID SPR_IMG_TRANSMITTER     = 4086;
static const SpriteID SPR_IMG_LEVEL_LAND      = SPR_OPENTTD_BASE + 91;
static const SpriteID SPR_IMG_BUILD_CANAL     = SPR_OPENTTD_BASE + 88;
static const SpriteID SPR_IMG_BUILD_RIVER     = SPR_OPENTTD_BASE + 136;
static const SpriteID SPR_IMG_BUILD_LOCK      = SPR_CANALS_BASE + 64;
static const SpriteID SPR_IMG_PAUSE           = 726;
static const SpriteID SPR_IMG_FASTFORWARD     = SPR_OPENTTD_BASE + 90;
static const SpriteID SPR_IMG_SETTINGS        = 751;
static const SpriteID SPR_IMG_SAVE            = 724;
static const SpriteID SPR_IMG_SMALLMAP        = 708;
static const SpriteID SPR_IMG_TOWN            = 4077;
static const SpriteID SPR_IMG_SUBSIDIES       = 679;
static const SpriteID SPR_IMG_COMPANY_LIST    = 1299;
static const SpriteID SPR_IMG_COMPANY_FINANCE = 737;
static const SpriteID SPR_IMG_COMPANY_GENERAL = 743;
static const SpriteID SPR_IMG_GRAPHS          = 745;
static const SpriteID SPR_IMG_COMPANY_LEAGUE  = 684;
static const SpriteID SPR_IMG_SHOW_COUNTOURS  = 738;
static const SpriteID SPR_IMG_SHOW_VEHICLES   = 739;
static const SpriteID SPR_IMG_SHOW_ROUTES     = 740;
static const SpriteID SPR_IMG_INDUSTRY        = 741;
static const SpriteID SPR_IMG_PLANTTREES      = 742;
static const SpriteID SPR_IMG_TRAINLIST       = 731;
static const SpriteID SPR_IMG_TRUCKLIST       = 732;
static const SpriteID SPR_IMG_SHIPLIST        = 733;
static const SpriteID SPR_IMG_AIRPLANESLIST   = 734;
static const SpriteID SPR_IMG_ZOOMIN          = 735;
static const SpriteID SPR_IMG_ZOOMOUT         = 736;
static const SpriteID SPR_IMG_BUILDRAIL       = 727;
static const SpriteID SPR_IMG_BUILDROAD       = 728;
static const SpriteID SPR_IMG_BUILDTRAMS      = SPR_OPENTTD_BASE + 175;
static const SpriteID SPR_IMG_BUILDWATER      = 729;
static const SpriteID SPR_IMG_BUILDAIR        = 730;
static const SpriteID SPR_IMG_LANDSCAPING     = 4083;
static const SpriteID SPR_IMG_MUSIC           = 713;
static const SpriteID SPR_IMG_MESSAGES        = 680;
static const SpriteID SPR_IMG_QUERY           = 723;
static const SpriteID SPR_IMG_SIGN            = 4082;
static const SpriteID SPR_IMG_BUY_LAND        = 4791;
static const SpriteID SPR_IMG_STORY_BOOK      = SPR_OPENTTD_BASE + 169;

/* OpenTTD in gamescreen */
static const SpriteID SPR_OTTD_O                = 4842;
static const SpriteID SPR_OTTD_P                = 4841;
static const SpriteID SPR_OTTD_E                = SPR_OPENTTD_BASE + 12;
static const SpriteID SPR_OTTD_D                = SPR_OPENTTD_BASE + 13;
static const SpriteID SPR_OTTD_N                = 4839;
static const SpriteID SPR_OTTD_T                = 4836;
/* Letters not used: R,A,S,Y,C (4837, 4838, 4840, 4843, 4844) */

static const SpriteID SPR_HIGHSCORE_CHART_BEGIN = 4804;
static const SpriteID SPR_TYCOON_IMG1_BEGIN     = 4814;
static const SpriteID SPR_TYCOON_IMG2_BEGIN     = 4824;

/* Industry sprites */
static const SpriteID SPR_IT_SUGAR_MINE_SIEVE         = 4775;
static const SpriteID SPR_IT_SUGAR_MINE_CLOUDS        = 4784;
static const SpriteID SPR_IT_SUGAR_MINE_PILE          = 4780;
static const SpriteID SPR_IT_TOFFEE_QUARRY_TOFFEE     = 4766;
static const SpriteID SPR_IT_TOFFEE_QUARRY_SHOVEL     = 4767;
static const SpriteID SPR_IT_BUBBLE_GENERATOR_SPRING  = 4746;
static const SpriteID SPR_IT_BUBBLE_GENERATOR_BUBBLE  = 4747;
static const SpriteID SPR_IT_TOY_FACTORY_STAMP_HOLDER = 4717;
static const SpriteID SPR_IT_TOY_FACTORY_STAMP        = 4718;
static const SpriteID SPR_IT_TOY_FACTORY_CLAY         = 4719;
static const SpriteID SPR_IT_TOY_FACTORY_ROBOT        = 4720;
static const SpriteID SPR_IT_POWER_PLANT_TRANSFORMERS = 2054;

/* small icons of cargo available in station waiting*/
static const SpriteID SPR_CARGO_PASSENGER             = 4297;
static const SpriteID SPR_CARGO_COAL                  = 4298;
static const SpriteID SPR_CARGO_MAIL                  = 4299;
static const SpriteID SPR_CARGO_OIL                   = 4300;
static const SpriteID SPR_CARGO_LIVESTOCK             = 4301;
static const SpriteID SPR_CARGO_GOODS                 = 4302;
static const SpriteID SPR_CARGO_GRAIN                 = 4303;
static const SpriteID SPR_CARGO_WOOD                  = 4304;
static const SpriteID SPR_CARGO_IRON_ORE              = 4305;
static const SpriteID SPR_CARGO_STEEL                 = 4306;
static const SpriteID SPR_CARGO_VALUES_GOLD           = 4307;  // shared between temperate and arctic
static const SpriteID SPR_CARGO_FRUIT                 = 4308;
static const SpriteID SPR_CARGO_COPPER_ORE            = 4309;
static const SpriteID SPR_CARGO_WATERCOLA             = 4310;  // shared between desert and toyland
static const SpriteID SPR_CARGO_DIAMONDS              = 4311;
static const SpriteID SPR_CARGO_FOOD                  = 4312;
static const SpriteID SPR_CARGO_PAPER                 = 4313;
static const SpriteID SPR_CARGO_RUBBER                = 4314;
static const SpriteID SPR_CARGO_CANDY                 = 4315;
static const SpriteID SPR_CARGO_SUGAR                 = 4316;
static const SpriteID SPR_CARGO_TOYS                  = 4317;
static const SpriteID SPR_CARGO_COTTONCANDY           = 4318;
static const SpriteID SPR_CARGO_FIZZYDRINK            = 4319;
static const SpriteID SPR_CARGO_TOFFEE                = 4320;
static const SpriteID SPR_CARGO_BUBBLES               = 4321;
static const SpriteID SPR_CARGO_PLASTIC               = 4322;
static const SpriteID SPR_CARGO_BATTERIES             = 4323;

/* Effect vehicles */
static const SpriteID SPR_BULLDOZER_NE = 1416;
static const SpriteID SPR_BULLDOZER_SE = 1417;
static const SpriteID SPR_BULLDOZER_SW = 1418;
static const SpriteID SPR_BULLDOZER_NW = 1419;

static const SpriteID SPR_SMOKE_0 = 2040;
static const SpriteID SPR_SMOKE_1 = 2041;
static const SpriteID SPR_SMOKE_2 = 2042;
static const SpriteID SPR_SMOKE_3 = 2043;
static const SpriteID SPR_SMOKE_4 = 2044;

static const SpriteID SPR_DIESEL_SMOKE_0 = 3073;
static const SpriteID SPR_DIESEL_SMOKE_1 = 3074;
static const SpriteID SPR_DIESEL_SMOKE_2 = 3075;
static const SpriteID SPR_DIESEL_SMOKE_3 = 3076;
static const SpriteID SPR_DIESEL_SMOKE_4 = 3077;
static const SpriteID SPR_DIESEL_SMOKE_5 = 3078;

static const SpriteID SPR_STEAM_SMOKE_0 = 3079;
static const SpriteID SPR_STEAM_SMOKE_1 = 3080;
static const SpriteID SPR_STEAM_SMOKE_2 = 3081;
static const SpriteID SPR_STEAM_SMOKE_3 = 3082;
static const SpriteID SPR_STEAM_SMOKE_4 = 3083;

static const SpriteID SPR_ELECTRIC_SPARK_0 = 3084;
static const SpriteID SPR_ELECTRIC_SPARK_1 = 3085;
static const SpriteID SPR_ELECTRIC_SPARK_2 = 3086;
static const SpriteID SPR_ELECTRIC_SPARK_3 = 3087;
static const SpriteID SPR_ELECTRIC_SPARK_4 = 3088;
static const SpriteID SPR_ELECTRIC_SPARK_5 = 3089;

static const SpriteID SPR_CHIMNEY_SMOKE_0 = 3701;
static const SpriteID SPR_CHIMNEY_SMOKE_1 = 3702;
static const SpriteID SPR_CHIMNEY_SMOKE_2 = 3703;
static const SpriteID SPR_CHIMNEY_SMOKE_3 = 3704;
static const SpriteID SPR_CHIMNEY_SMOKE_4 = 3705;
static const SpriteID SPR_CHIMNEY_SMOKE_5 = 3706;
static const SpriteID SPR_CHIMNEY_SMOKE_6 = 3707;
static const SpriteID SPR_CHIMNEY_SMOKE_7 = 3708;

static const SpriteID SPR_EXPLOSION_LARGE_0 = 3709;
static const SpriteID SPR_EXPLOSION_LARGE_1 = 3710;
static const SpriteID SPR_EXPLOSION_LARGE_2 = 3711;
static const SpriteID SPR_EXPLOSION_LARGE_3 = 3712;
static const SpriteID SPR_EXPLOSION_LARGE_4 = 3713;
static const SpriteID SPR_EXPLOSION_LARGE_5 = 3714;
static const SpriteID SPR_EXPLOSION_LARGE_6 = 3715;
static const SpriteID SPR_EXPLOSION_LARGE_7 = 3716;
static const SpriteID SPR_EXPLOSION_LARGE_8 = 3717;
static const SpriteID SPR_EXPLOSION_LARGE_9 = 3718;
static const SpriteID SPR_EXPLOSION_LARGE_A = 3719;
static const SpriteID SPR_EXPLOSION_LARGE_B = 3720;
static const SpriteID SPR_EXPLOSION_LARGE_C = 3721;
static const SpriteID SPR_EXPLOSION_LARGE_D = 3722;
static const SpriteID SPR_EXPLOSION_LARGE_E = 3723;
static const SpriteID SPR_EXPLOSION_LARGE_F = 3724;

static const SpriteID SPR_EXPLOSION_SMALL_0 = 3725;
static const SpriteID SPR_EXPLOSION_SMALL_1 = 3726;
static const SpriteID SPR_EXPLOSION_SMALL_2 = 3727;
static const SpriteID SPR_EXPLOSION_SMALL_3 = 3728;
static const SpriteID SPR_EXPLOSION_SMALL_4 = 3729;
static const SpriteID SPR_EXPLOSION_SMALL_5 = 3730;
static const SpriteID SPR_EXPLOSION_SMALL_6 = 3731;
static const SpriteID SPR_EXPLOSION_SMALL_7 = 3732;
static const SpriteID SPR_EXPLOSION_SMALL_8 = 3733;
static const SpriteID SPR_EXPLOSION_SMALL_9 = 3734;
static const SpriteID SPR_EXPLOSION_SMALL_A = 3735;
static const SpriteID SPR_EXPLOSION_SMALL_B = 3736;

static const SpriteID SPR_BREAKDOWN_SMOKE_0 = 3737;
static const SpriteID SPR_BREAKDOWN_SMOKE_1 = 3738;
static const SpriteID SPR_BREAKDOWN_SMOKE_2 = 3739;
static const SpriteID SPR_BREAKDOWN_SMOKE_3 = 3740;

static const SpriteID SPR_BUBBLE_0 = 4748;
static const SpriteID SPR_BUBBLE_1 = 4749;
static const SpriteID SPR_BUBBLE_2 = 4750;
static const SpriteID SPR_BUBBLE_GENERATE_0 = 4751;
static const SpriteID SPR_BUBBLE_GENERATE_1 = 4752;
static const SpriteID SPR_BUBBLE_GENERATE_2 = 4753;
static const SpriteID SPR_BUBBLE_GENERATE_3 = 4754;
static const SpriteID SPR_BUBBLE_BURST_0 = 4755;
static const SpriteID SPR_BUBBLE_BURST_1 = 4756;
static const SpriteID SPR_BUBBLE_BURST_2 = 4757;
static const SpriteID SPR_BUBBLE_ABSORB_0 = 4758;
static const SpriteID SPR_BUBBLE_ABSORB_1 = 4759;
static const SpriteID SPR_BUBBLE_ABSORB_2 = 4760;
static const SpriteID SPR_BUBBLE_ABSORB_3 = 4761;
static const SpriteID SPR_BUBBLE_ABSORB_4 = 4762;

/* Electrified rail build menu */
static const SpriteID SPR_BUILD_NS_ELRAIL = SPR_ELRAIL_BASE + 36;
static const SpriteID SPR_BUILD_X_ELRAIL  = SPR_ELRAIL_BASE + 37;
static const SpriteID SPR_BUILD_EW_ELRAIL = SPR_ELRAIL_BASE + 38;
static const SpriteID SPR_BUILD_Y_ELRAIL  = SPR_ELRAIL_BASE + 39;
static const SpriteID SPR_BUILD_TUNNEL_ELRAIL = SPR_ELRAIL_BASE + 44;

/* airport_gui.cpp */
static const SpriteID SPR_IMG_AIRPORT       = 744;

/* dock_gui.cpp */
static const SpriteID SPR_IMG_SHIP_DEPOT    = 748;
static const SpriteID SPR_IMG_SHIP_DOCK     = 746;
static const SpriteID SPR_IMG_BUOY          = 693;
static const SpriteID SPR_IMG_AQUEDUCT      = SPR_OPENTTD_BASE + 145;

/* music_gui.cpp */
static const SpriteID SPR_IMG_SKIP_TO_PREV  = 709;
static const SpriteID SPR_IMG_SKIP_TO_NEXT  = 710;
static const SpriteID SPR_IMG_STOP_MUSIC    = 711;
static const SpriteID SPR_IMG_PLAY_MUSIC    = 712;

/* road_gui.cpp */
static const SpriteID SPR_IMG_ROAD_Y_DIR    = 1309;
static const SpriteID SPR_IMG_ROAD_X_DIR    = 1310;
static const SpriteID SPR_IMG_AUTOROAD      = SPR_OPENTTD_BASE + 82;
static const SpriteID SPR_IMG_ROAD_DEPOT    = 1295;
static const SpriteID SPR_IMG_BUS_STATION   = 749;
static const SpriteID SPR_IMG_TRUCK_BAY     = 750;
static const SpriteID SPR_IMG_BRIDGE        = 2594;
static const SpriteID SPR_IMG_ROAD_TUNNEL   = 2429;
static const SpriteID SPR_IMG_REMOVE        = 714;
static const SpriteID SPR_IMG_ROAD_ONE_WAY  = SPR_OPENTTD_BASE + 134;
static const SpriteID SPR_IMG_TRAMWAY_Y_DIR = SPR_TRAMWAY_BASE + 0;
static const SpriteID SPR_IMG_TRAMWAY_X_DIR = SPR_TRAMWAY_BASE + 1;
static const SpriteID SPR_IMG_AUTOTRAM      = SPR_OPENTTD_BASE + 84;

/* rail_gui.cpp */
static const SpriteID SPR_IMG_RAIL_NS    = 1251;
static const SpriteID SPR_IMG_RAIL_NE    = 1252;
static const SpriteID SPR_IMG_RAIL_EW    = 1253;
static const SpriteID SPR_IMG_RAIL_NW    = 1254;
static const SpriteID SPR_IMG_AUTORAIL   = SPR_OPENTTD_BASE + 53;
static const SpriteID SPR_IMG_AUTOELRAIL = SPR_OPENTTD_BASE + 57;
static const SpriteID SPR_IMG_AUTOMONO   = SPR_OPENTTD_BASE + 63;
static const SpriteID SPR_IMG_AUTOMAGLEV = SPR_OPENTTD_BASE + 69;

static const SpriteID SPR_IMG_WAYPOINT = SPR_OPENTTD_BASE + 76;

static const SpriteID SPR_IMG_DEPOT_RAIL   = 1294;
static const SpriteID SPR_IMG_DEPOT_ELRAIL = SPR_OPENTTD_BASE + 61;
static const SpriteID SPR_IMG_DEPOT_MONO   = SPR_OPENTTD_BASE + 67;
static const SpriteID SPR_IMG_DEPOT_MAGLEV = SPR_OPENTTD_BASE + 73;

static const SpriteID SPR_IMG_RAIL_STATION = 1298;
static const SpriteID SPR_IMG_RAIL_SIGNALS = 1291;

static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_NORM     = 1287;
static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_ENTRY    = SPR_SIGNALS_BASE +  12;
static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_EXIT     = SPR_SIGNALS_BASE +  28;
static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_COMBO    = SPR_SIGNALS_BASE +  44;
static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_PBS      = SPR_SIGNALS_BASE + 124;
static const SpriteID SPR_IMG_SIGNAL_ELECTRIC_PBS_OWAY = SPR_SIGNALS_BASE + 140;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_NORM    = SPR_SIGNALS_BASE +  60;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_ENTRY   = SPR_SIGNALS_BASE +  76;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_EXIT    = SPR_SIGNALS_BASE +  92;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_COMBO   = SPR_SIGNALS_BASE + 108;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_PBS     = SPR_SIGNALS_BASE + 188;
static const SpriteID SPR_IMG_SIGNAL_SEMAPHORE_PBS_OWAY= SPR_SIGNALS_BASE + 204;
static const SpriteID SPR_IMG_SIGNAL_CONVERT           = SPR_OPENTTD_BASE + 135;

static const SpriteID SPR_IMG_TUNNEL_RAIL   = 2430;
static const SpriteID SPR_IMG_TUNNEL_MONO   = 2431;
static const SpriteID SPR_IMG_TUNNEL_MAGLEV = 2432;

static const SpriteID SPR_IMG_CONVERT_RAIL   = SPR_OPENTTD_BASE + 55;
static const SpriteID SPR_IMG_CONVERT_ELRAIL = SPR_OPENTTD_BASE + 59;
static const SpriteID SPR_IMG_CONVERT_MONO   = SPR_OPENTTD_BASE + 65;
static const SpriteID SPR_IMG_CONVERT_MAGLEV = SPR_OPENTTD_BASE + 71;

/* story_gui.cpp */
static const SpriteID SPR_IMG_VIEW_LOCATION  = SPR_OPENTTD_BASE + 170;
static const SpriteID SPR_IMG_GOAL           = SPR_OPENTTD_BASE + 171;
static const SpriteID SPR_IMG_GOAL_COMPLETED = SPR_OPENTTD_BASE + 172;
static const SpriteID SPR_IMG_GOAL_BROKEN_REF= SPR_OPENTTD_BASE + 173;

static const SpriteID SPR_IMG_CONVERT_ROAD           = SPR_OPENTTD_BASE + 176;
static const CursorID SPR_CURSOR_CONVERT_ROAD        = SPR_OPENTTD_BASE + 177;
static const SpriteID SPR_IMG_CONVERT_TRAM           = SPR_OPENTTD_BASE + 178;
static const CursorID SPR_CURSOR_CONVERT_TRAM        = SPR_OPENTTD_BASE + 179;

/* intro_gui.cpp, genworld_gui.cpp */
static const SpriteID SPR_SELECT_TEMPERATE           = 4882;
static const SpriteID SPR_SELECT_TEMPERATE_PUSHED    = 4883;
static const SpriteID SPR_SELECT_SUB_ARCTIC          = 4884;
static const SpriteID SPR_SELECT_SUB_ARCTIC_PUSHED   = 4885;
static const SpriteID SPR_SELECT_SUB_TROPICAL        = 4886;
static const SpriteID SPR_SELECT_SUB_TROPICAL_PUSHED = 4887;
static const SpriteID SPR_SELECT_TOYLAND             = 4888;
static const SpriteID SPR_SELECT_TOYLAND_PUSHED      = 4889;

/** Cursor sprite numbers */

/* Terraform
 * Cursors */
static const CursorID SPR_CURSOR_MOUSE          = 0;
static const CursorID SPR_CURSOR_ZZZ            = 1;
static const CursorID SPR_CURSOR_BUOY           = 702;
static const CursorID SPR_CURSOR_QUERY          = 719;
static const CursorID SPR_CURSOR_HQ             = 720;
static const CursorID SPR_CURSOR_SHIP_DEPOT     = 721;
static const CursorID SPR_CURSOR_SIGN           = 722;

static const CursorID SPR_CURSOR_TREE           = 2010;
static const CursorID SPR_CURSOR_BUY_LAND       = 4792;
static const CursorID SPR_CURSOR_LEVEL_LAND     = SPR_OPENTTD_BASE + 92;

static const CursorID SPR_CURSOR_TOWN           = 4080;
static const CursorID SPR_CURSOR_INDUSTRY       = 4081;
static const CursorID SPR_CURSOR_ROCKY_AREA     = 4087;
static const CursorID SPR_CURSOR_DESERT         = 4088;
static const CursorID SPR_CURSOR_TRANSMITTER    = 4089;

/* airport cursors */
static const CursorID SPR_CURSOR_AIRPORT        = 2724;

/* dock cursors */
static const CursorID SPR_CURSOR_DOCK           = 3668;
static const CursorID SPR_CURSOR_CANAL          = SPR_OPENTTD_BASE + 89;
static const CursorID SPR_CURSOR_LOCK           = SPR_OPENTTD_BASE + 87;
static const CursorID SPR_CURSOR_RIVER          = SPR_OPENTTD_BASE + 137;
static const CursorID SPR_CURSOR_AQUEDUCT       = SPR_OPENTTD_BASE + 146;

/* shared road & rail cursors */
static const CursorID SPR_CURSOR_BRIDGE         = 2593;

/* rail cursors */
static const CursorID SPR_CURSOR_NS_TRACK       = 1263;
static const CursorID SPR_CURSOR_SWNE_TRACK     = 1264;
static const CursorID SPR_CURSOR_EW_TRACK       = 1265;
static const CursorID SPR_CURSOR_NWSE_TRACK     = 1266;

static const CursorID SPR_CURSOR_NS_MONO        = 1267;
static const CursorID SPR_CURSOR_SWNE_MONO      = 1268;
static const CursorID SPR_CURSOR_EW_MONO        = 1269;
static const CursorID SPR_CURSOR_NWSE_MONO      = 1270;

static const CursorID SPR_CURSOR_NS_MAGLEV      = 1271;
static const CursorID SPR_CURSOR_SWNE_MAGLEV    = 1272;
static const CursorID SPR_CURSOR_EW_MAGLEV      = 1273;
static const CursorID SPR_CURSOR_NWSE_MAGLEV    = 1274;

static const CursorID SPR_CURSOR_NS_ELRAIL      = SPR_ELRAIL_BASE + 40;
static const CursorID SPR_CURSOR_SWNE_ELRAIL    = SPR_ELRAIL_BASE + 41;
static const CursorID SPR_CURSOR_EW_ELRAIL      = SPR_ELRAIL_BASE + 42;
static const CursorID SPR_CURSOR_NWSE_ELRAIL    = SPR_ELRAIL_BASE + 43;

static const CursorID SPR_CURSOR_RAIL_STATION   = 1300;

static const CursorID SPR_CURSOR_TUNNEL_RAIL    = 2434;
static const CursorID SPR_CURSOR_TUNNEL_ELRAIL  = SPR_ELRAIL_BASE + 45;
static const CursorID SPR_CURSOR_TUNNEL_MONO    = 2435;
static const CursorID SPR_CURSOR_TUNNEL_MAGLEV  = 2436;

static const CursorID SPR_CURSOR_AUTORAIL       = SPR_OPENTTD_BASE + 54;
static const CursorID SPR_CURSOR_AUTOELRAIL     = SPR_OPENTTD_BASE + 58;
static const CursorID SPR_CURSOR_AUTOMONO       = SPR_OPENTTD_BASE + 64;
static const CursorID SPR_CURSOR_AUTOMAGLEV     = SPR_OPENTTD_BASE + 70;

static const CursorID SPR_CURSOR_WAYPOINT       = SPR_OPENTTD_BASE + 77;

static const CursorID SPR_CURSOR_RAIL_DEPOT     = 1296;
static const CursorID SPR_CURSOR_ELRAIL_DEPOT   = SPR_OPENTTD_BASE + 62;
static const CursorID SPR_CURSOR_MONO_DEPOT     = SPR_OPENTTD_BASE + 68;
static const CursorID SPR_CURSOR_MAGLEV_DEPOT   = SPR_OPENTTD_BASE + 74;

static const CursorID SPR_CURSOR_CONVERT_RAIL   = SPR_OPENTTD_BASE + 56;
static const CursorID SPR_CURSOR_CONVERT_ELRAIL = SPR_OPENTTD_BASE + 60;
static const CursorID SPR_CURSOR_CONVERT_MONO   = SPR_OPENTTD_BASE + 66;
static const CursorID SPR_CURSOR_CONVERT_MAGLEV = SPR_OPENTTD_BASE + 72;

/* road cursors */
static const CursorID SPR_CURSOR_ROAD_NESW      = 1311;
static const CursorID SPR_CURSOR_ROAD_NWSE      = 1312;
static const CursorID SPR_CURSOR_AUTOROAD       = SPR_OPENTTD_BASE + 83;
static const CursorID SPR_CURSOR_TRAMWAY_NESW   = SPR_TRAMWAY_BASE + 2;
static const CursorID SPR_CURSOR_TRAMWAY_NWSE   = SPR_TRAMWAY_BASE + 3;
static const CursorID SPR_CURSOR_AUTOTRAM       = SPR_OPENTTD_BASE + 85;

static const CursorID SPR_CURSOR_ROAD_DEPOT     = 1297;
static const CursorID SPR_CURSOR_BUS_STATION    = 2725;
static const CursorID SPR_CURSOR_TRUCK_STATION  = 2726;
static const CursorID SPR_CURSOR_ROAD_TUNNEL    = 2433;

static const CursorID SPR_CURSOR_CLONE_TRAIN    = SPR_OPENTTD_BASE + 110;
static const CursorID SPR_CURSOR_CLONE_ROADVEH  = SPR_OPENTTD_BASE + 111;
static const CursorID SPR_CURSOR_CLONE_SHIP     = SPR_OPENTTD_BASE + 112;
static const CursorID SPR_CURSOR_CLONE_AIRPLANE = SPR_OPENTTD_BASE + 113;

/** Animation macro in table/animcursors.h (_animcursors[]) */

static const CursorID SPR_CURSOR_DEMOLISH_FIRST = 704;
static const CursorID SPR_CURSOR_DEMOLISH_1     = 705;
static const CursorID SPR_CURSOR_DEMOLISH_2     = 706;
static const CursorID SPR_CURSOR_DEMOLISH_LAST  = 707;

static const CursorID SPR_CURSOR_LOWERLAND_FIRST = 699;
static const CursorID SPR_CURSOR_LOWERLAND_1     = 700;
static const CursorID SPR_CURSOR_LOWERLAND_LAST  = 701;

static const CursorID SPR_CURSOR_RAISELAND_FIRST = 696;
static const CursorID SPR_CURSOR_RAISELAND_1     = 697;
static const CursorID SPR_CURSOR_RAISELAND_LAST  = 698;

static const CursorID SPR_CURSOR_PICKSTATION_FIRST = 716;
static const CursorID SPR_CURSOR_PICKSTATION_1     = 717;
static const CursorID SPR_CURSOR_PICKSTATION_LAST  = 718;

static const CursorID SPR_CURSOR_BUILDSIGNALS_FIRST = 1292;
static const CursorID SPR_CURSOR_BUILDSIGNALS_LAST  = 1293;

/** Flag for saying a cursor sprite is an animated cursor. */
static const CursorID ANIMCURSOR_FLAG         = 1U << 31;
static const CursorID ANIMCURSOR_DEMOLISH     = ANIMCURSOR_FLAG | 0; ///<  704 -  707 - demolish dynamite
static const CursorID ANIMCURSOR_LOWERLAND    = ANIMCURSOR_FLAG | 1; ///<  699 -  701 - lower land tool
static const CursorID ANIMCURSOR_RAISELAND    = ANIMCURSOR_FLAG | 2; ///<  696 -  698 - raise land tool
static const CursorID ANIMCURSOR_PICKSTATION  = ANIMCURSOR_FLAG | 3; ///<  716 -  718 - goto-order icon
static const CursorID ANIMCURSOR_BUILDSIGNALS = ANIMCURSOR_FLAG | 4; ///< 1292 - 1293 - build signal

/**
 * Bitmask setup. For the graphics system, 32 bits are used to define
 * the sprite to be displayed. This variable contains various information:<p>
 * <ul><li> SPRITE_WIDTH is the number of bits used for the actual sprite to be displayed.
 * This always starts at bit 0.</li>
 * <li> TRANSPARENT_BIT is the bit number which toggles sprite transparency</li>
 * <li> RECOLOUR_BIT toggles the recolouring system</li>
 * <li> PALETTE_SPRITE_WIDTH and PALETTE_SPRITE_START determine the position and number of
 * bits used for the recolouring process. For transparency, it must be 0x322.</li></ul>
 */
enum SpriteSetup {
	/* These bits are applied to sprite ID */
	TRANSPARENT_BIT = 31,       ///< toggles transparency in the sprite
	RECOLOUR_BIT = 30,          ///< toggles recolouring in the sprite
	CUSTOM_BIT = 29,
	OPAQUE_BIT = 28,

	/* This bit is applied to palette ID */
	PALETTE_TEXT_RECOLOUR = 31, ///< Set if palette is actually a magic text recolour

	PALETTE_WIDTH = 24,         ///< number of bits of the sprite containing the recolour palette
	SPRITE_WIDTH = 24,          ///< number of bits for the sprite number
};

/**
 * these masks change the colours of the palette for a sprite.
 * Apart from this bit, a sprite number is needed to define
 * the palette used for recolouring. This palette is stored
 * in the bits marked by PALETTE_SPRITE_MASK.
 * @note Do not modify this enum. Alter SpriteSetup instead
 * @see SpriteSetup
 */
enum Modifiers {
	SPRITE_MODIFIER_CUSTOM_SPRITE = CUSTOM_BIT,      ///< Set when a sprite originates from an Action 1
	SPRITE_MODIFIER_OPAQUE        = OPAQUE_BIT,      ///< Set when a sprite must not ever be displayed transparently
	PALETTE_MODIFIER_TRANSPARENT  = TRANSPARENT_BIT, ///< when a sprite is to be displayed transparently, this bit needs to be set.
	PALETTE_MODIFIER_COLOUR       = RECOLOUR_BIT,    ///< this bit is set when a recolouring process is in action
};

/**
 * Masks needed for sprite operations.
 * @note Do not modify this enum. Alter SpriteSetup instead
 * @see SpriteSetup
 */
enum SpriteMasks {
	MAX_SPRITES = 1 << SPRITE_WIDTH,       ///< Maximum number of sprites that can be loaded at a given time
	SPRITE_MASK = MAX_SPRITES - 1,         ///< The mask to for the main sprite

	MAX_PALETTES = 1 << PALETTE_WIDTH,
	PALETTE_MASK = MAX_PALETTES - 1,       ///< The mask for the auxiliary sprite (the one that takes care of recolouring)
};

assert_compile( (1 << TRANSPARENT_BIT & SPRITE_MASK) == 0 );
assert_compile( (1 << RECOLOUR_BIT & SPRITE_MASK) == 0 );
assert_compile( TRANSPARENT_BIT != RECOLOUR_BIT );
assert_compile( (1 << TRANSPARENT_BIT & PALETTE_MASK) == 0);
assert_compile( (1 << RECOLOUR_BIT & PALETTE_MASK) == 0 );


static const PaletteID PAL_NONE                    = 0;
static const PaletteID PALETTE_TILE_RED_PULSATING  = 771;  ///< pulsating red tile drawn if you try to build a wrong tunnel or raise/lower land where it is not possible
static const PaletteID PALETTE_SEL_TILE_RED        = 772;  ///< makes a square red. is used when removing rails or other stuff
static const PaletteID PALETTE_SEL_TILE_BLUE       = 773;  ///< This draws a blueish square (catchment areas for example)

/* Company re-colour sprites */
static const PaletteID PALETTE_RECOLOUR_START      = 775;  ///< First recolour sprite for company colours
static const PaletteID PALETTE_TO_DARK_BLUE        = 775;
static const PaletteID PALETTE_TO_PALE_GREEN       = 776;
static const PaletteID PALETTE_TO_PINK             = 777;
static const PaletteID PALETTE_TO_YELLOW           = 778;
static const PaletteID PALETTE_TO_RED              = 779;
static const PaletteID PALETTE_TO_LIGHT_BLUE       = 780;
static const PaletteID PALETTE_TO_GREEN            = 781;
static const PaletteID PALETTE_TO_DARK_GREEN       = 782;
static const PaletteID PALETTE_TO_BLUE             = 783;
static const PaletteID PALETTE_TO_CREAM            = 784;
/* maybe don't use as company colour because it doesn't display in the graphs? */
static const PaletteID PALETTE_TO_MAUVE            = 785;
static const PaletteID PALETTE_TO_PURPLE           = 786;
static const PaletteID PALETTE_TO_ORANGE           = 787;
static const PaletteID PALETTE_TO_BROWN            = 788;
static const PaletteID PALETTE_TO_GREY             = 789;
static const PaletteID PALETTE_TO_WHITE            = 790;

static const PaletteID PALETTE_TO_BARE_LAND        = 791;  ///< sets colour to bare land stuff for rail, road and crossings
/* recolour sprites 792-794 are not used */
static const PaletteID PALETTE_TO_STRUCT_BLUE      = 795;  ///< sets bridge or structure to blue (e.g. some town houses)
static const PaletteID PALETTE_TO_STRUCT_BROWN     = 796;  ///< sets bridge or structure to brown (e.g. cantilever bridge)
static const PaletteID PALETTE_TO_STRUCT_WHITE     = 797;  ///< sets bridge or structure to white (e.g. some town houses)
static const PaletteID PALETTE_TO_STRUCT_RED       = 798;  ///< sets bridge or structure to red (e.g. concrete and cantilever bridge)
static const PaletteID PALETTE_TO_STRUCT_GREEN     = 799;  ///< sets bridge or structure to green (e.g. bridge)
static const PaletteID PALETTE_TO_STRUCT_CONCRETE  = 800;  ///< Sets the suspension bridge to concrete, also other structures use it
static const PaletteID PALETTE_TO_STRUCT_YELLOW    = 801;  ///< Sets the bridge colour to yellow (suspension and tubular)
static const PaletteID PALETTE_TO_TRANSPARENT      = 802;  ///< This sets the sprite to transparent

static const PaletteID PALETTE_NEWSPAPER           = 803;  ///< Recolour sprite for newspaper-greying.
static const PaletteID PALETTE_CRASH               = 804;  ///< Recolour sprite greying of crashed vehicles.

/* Two recolourings only used by the church */
static const PaletteID PALETTE_CHURCH_RED          = 1438; ///< Recolour sprite for reddish churches
static const PaletteID PALETTE_CHURCH_CREAM        = 1439; ///< Recolour sprite for white churches

static const PaletteID PALETTE_ALL_BLACK           = SPR_PALETTE_BASE; ///< Exchange any color by black, needed for painting fictive tiles outside map

#endif /* SPRITES_H */
