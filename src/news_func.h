/* $Id$ */

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

void AddNewsItem(StringID string, NewsSubtype subtype, NewsReferenceType reftype1 = NR_NONE, uint32 ref1 = UINT32_MAX, NewsReferenceType reftype2 = NR_NONE, uint32 ref2 = UINT32_MAX, void *free_data = NULL);

static inline void AddCompanyNewsItem(StringID string, CompanyNewsInformation *cni)
{
	AddNewsItem(string, NS_COMPANY_INFO, NR_NONE, UINT32_MAX, NR_NONE, UINT32_MAX, cni);
}

/**
 * Adds a newsitem referencing a vehicle.
 *
 * @warning The DParams may not reference the vehicle due to autoreplace stuff. See AddVehicleAdviceNewsItem for how that can be done.
 */
static inline void AddVehicleNewsItem(StringID string, NewsSubtype subtype, VehicleID vehicle, StationID station = INVALID_STATION)
{
	AddNewsItem(string, subtype, NR_VEHICLE, vehicle, station == INVALID_STATION ? NR_NONE : NR_STATION, station);
}

/**
 * Adds a vehicle-advice news item.
 *
 * @warning DParam 0 must reference the vehicle!
 */
static inline void AddVehicleAdviceNewsItem(StringID string, VehicleID vehicle)
{
	AddNewsItem(string, NS_ADVICE, NR_VEHICLE, vehicle);
}

static inline void AddTileNewsItem(StringID string, NewsSubtype subtype, TileIndex tile, void *free_data = NULL)
{
	AddNewsItem(string, subtype, NR_TILE, tile, NR_NONE, UINT32_MAX, free_data);
}

static inline void AddIndustryNewsItem(StringID string, NewsSubtype subtype, IndustryID industry)
{
	AddNewsItem(string, subtype, NR_INDUSTRY, industry);
}

void NewsLoop();
void InitNewsItemStructs();

extern const NewsItem *_statusbar_news_item;
extern bool _news_ticker_sound;

extern NewsTypeData _news_type_data[];

void DeleteInvalidEngineNews();
void DeleteVehicleNews(VehicleID vid, StringID news);
void DeleteStationNews(StationID sid);
void DeleteIndustryNews(IndustryID iid);

#endif /* NEWS_FUNC_H */
