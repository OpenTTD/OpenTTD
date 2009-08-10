/* $Id$ */

/** @file news_func.h Functions related to news. */

#ifndef NEWS_FUNC_H
#define NEWS_FUNC_H

#include "news_type.h"
#include "vehicle_type.h"
#include "station_type.h"
#include "industry_type.h"

void AddNewsItem(StringID string, NewsSubtype subtype, NewsReferenceType reftype1 = NR_NONE, uint32 ref1 = UINT32_MAX, NewsReferenceType reftype2 = NR_NONE, uint32 ref2 = UINT32_MAX, void *free_data = NULL);

static inline void AddCompanyNewsItem(StringID string, NewsSubtype subtype, CompanyNewsInformation *cni)
{
	AddNewsItem(string, subtype, NR_NONE, UINT32_MAX, NR_NONE, UINT32_MAX, cni);
}

/**
 * Adds a newsitem referencing a vehicle.
 *
 * @warning
 * Be careful!
 * Vehicles are a special case, as news are kept when vehicles are autoreplaced/renewed.
 * You have to make sure, #ChangeVehicleNews catches the DParams of your message.
 * This is NOT ensured by the references.
 */
static inline void AddVehicleNewsItem(StringID string, NewsSubtype subtype, VehicleID vehicle, StationID station = INVALID_STATION)
{
	AddNewsItem(string, subtype, NR_VEHICLE, vehicle, station == INVALID_STATION ? NR_NONE : NR_STATION, station);
}

static inline void AddIndustryNewsItem(StringID string, NewsSubtype subtype, IndustryID industry)
{
	AddNewsItem(string, subtype, NR_INDUSTRY, industry);
}

void NewsLoop();
void InitNewsItemStructs();

extern NewsItem _statusbar_news_item;
extern bool _news_ticker_sound;

extern NewsTypeData _news_type_data[];

/**
 * Delete a news item type about a vehicle
 * if the news item type is INVALID_STRING_ID all news about the vehicle get
 * deleted
 */
void DeleteVehicleNews(VehicleID vid, StringID news);

/** Delete news associated with given station */
void DeleteStationNews(StationID sid);

/** Delete news associated with given station */
void DeleteIndustryNews(IndustryID iid);

#endif /* NEWS_FUNC_H */
