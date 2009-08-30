/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_type.h Types related to the industry. */

#ifndef INDUSTRY_TYPE_H
#define INDUSTRY_TYPE_H

typedef uint16 IndustryID;
typedef uint16 IndustryGfx;
typedef uint8 IndustryType;
struct Industry;

struct IndustrySpec;
struct IndustryTileSpec;

static const IndustryID INVALID_INDUSTRY = 0xFFFF;

enum {
	NEW_INDUSTRYOFFSET     = 37,                         ///< original number of industries
	NUM_INDUSTRYTYPES      = 64,                         ///< total number of industries, new and old
	INDUSTRYTILE_NOANIM    = 0xFF,                       ///< flag to mark industry tiles as having no animation
	NEW_INDUSTRYTILEOFFSET = 175,                        ///< original number of tiles
	INVALID_INDUSTRYTYPE   = NUM_INDUSTRYTYPES,          ///< one above amount is considered invalid
	NUM_INDUSTRYTILES      = 512,                        ///< total number of industry tiles, new and old
	INVALID_INDUSTRYTILE   = NUM_INDUSTRYTILES,          ///< one above amount is considered invalid
	INDUSTRY_COMPLETED     = 3,                          ///< final stage of industry construction.
};

#endif /* INDUSTRY_TYPE_H */
