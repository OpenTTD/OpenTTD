/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_type.h Types related to the industry. */

#ifndef INDUSTRY_TYPE_H
#define INDUSTRY_TYPE_H

typedef uint16_t IndustryID;
typedef uint16_t IndustryGfx;
typedef uint8_t IndustryType;
struct Industry;

struct IndustrySpec;
struct IndustryTileSpec;

static const IndustryID INVALID_INDUSTRY = 0xFFFF;

static const IndustryType NUM_INDUSTRYTYPES_PER_GRF = 128;            ///< maximum number of industry types per NewGRF; limited to 128 because bit 7 has a special meaning in some variables/callbacks (see MapNewGRFIndustryType).

static const IndustryType NEW_INDUSTRYOFFSET     = 37;                ///< original number of industry types
static const IndustryType NUM_INDUSTRYTYPES      = 240;               ///< total number of industry types, new and old; limited to 240 because we need some special ids like INVALID_INDUSTRYTYPE, IT_AI_UNKNOWN, IT_AI_TOWN, ...
static const IndustryType INVALID_INDUSTRYTYPE   = NUM_INDUSTRYTYPES; ///< one above amount is considered invalid

static const IndustryGfx  NUM_INDUSTRYTILES_PER_GRF = 255;            ///< Maximum number of industry tiles per NewGRF; limited to 255 to allow extending Action3 with an extended byte later on.

static const IndustryGfx  INDUSTRYTILE_NOANIM    = 0xFF;              ///< flag to mark industry tiles as having no animation
static const IndustryGfx  NEW_INDUSTRYTILEOFFSET = 175;               ///< original number of tiles
static const IndustryGfx  NUM_INDUSTRYTILES      = 512;               ///< total number of industry tiles, new and old
static const IndustryGfx  INVALID_INDUSTRYTILE   = NUM_INDUSTRYTILES; ///< one above amount is considered invalid

static const int INDUSTRY_COMPLETED = 3; ///< final stage of industry construction.

static const int INDUSTRY_NUM_INPUTS = 16;  ///< Number of cargo types an industry can accept
static const int INDUSTRY_NUM_OUTPUTS = 16; ///< Number of cargo types an industry can produce


void CheckIndustries();

#endif /* INDUSTRY_TYPE_H */
