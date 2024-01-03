/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file news_func.h Functions related to news. */

#ifndef NEWS_FUNC_H
#define NEWS_FUNC_H

#include "news_type.h"
#include "vehicle_type.h"
#include "station_type.h"
#include "industry_type.h"

void AddNewsItem(StringID string, NewsType type, NewsFlag flags, NewsReferenceType reftype1 = NR_NONE, uint32_t ref1 = UINT32_MAX, NewsReferenceType reftype2 = NR_NONE, uint32_t ref2 = UINT32_MAX, const NewsAllocatedData *data = nullptr);

static inline void AddCompanyNewsItem(StringID string, CompanyNewsInformation *cni)
{
	AddNewsItem(string, NT_COMPANY_INFO, NF_COMPANY, NR_NONE, UINT32_MAX, NR_NONE, UINT32_MAX, cni);
}

/**
 * Adds a newsitem referencing a vehicle.
 *
 * @warning The DParams may not reference the vehicle due to autoreplace stuff. See AddVehicleAdviceNewsItem for how that can be done.
 */
static inline void AddVehicleNewsItem(StringID string, NewsType type, VehicleID vehicle, StationID station = INVALID_STATION)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, NR_VEHICLE, vehicle, station == INVALID_STATION ? NR_NONE : NR_STATION, station);
}

/**
 * Adds a vehicle-advice news item.
 *
 * @warning DParam 0 must reference the vehicle!
 */
static inline void AddVehicleAdviceNewsItem(StringID string, VehicleID vehicle)
{
	AddNewsItem(string, NT_ADVICE, NF_INCOLOUR | NF_SMALL | NF_VEHICLE_PARAM0, NR_VEHICLE, vehicle);
}

static inline void AddTileNewsItem(StringID string, NewsType type, TileIndex tile, const NewsAllocatedData *data = nullptr, StationID station = INVALID_STATION)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, NR_TILE, tile.base(), station == INVALID_STATION ? NR_NONE : NR_STATION, station, data);
}

static inline void AddIndustryNewsItem(StringID string, NewsType type, IndustryID industry, const NewsAllocatedData *data = nullptr)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, NR_INDUSTRY, industry, NR_NONE, UINT32_MAX, data);
}

void NewsLoop();
void InitNewsItemStructs();

extern const NewsItem *_statusbar_news_item;

void DeleteInvalidEngineNews();
void DeleteVehicleNews(VehicleID vid, StringID news);
void DeleteStationNews(StationID sid);
void DeleteIndustryNews(IndustryID iid);

#endif /* NEWS_FUNC_H */
