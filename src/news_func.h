/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file news_func.h Functions related to news. */

#ifndef NEWS_FUNC_H
#define NEWS_FUNC_H

#include "news_type.h"
#include "vehicle_type.h"
#include "station_type.h"
#include "industry_type.h"

void AddNewsItem(EncodedString &&headline, NewsType type, NewsStyle style, NewsFlags flags, NewsReference ref1 = {}, NewsReference ref2 = {}, std::unique_ptr<NewsAllocatedData> &&data = nullptr, AdviceType advice_type = AdviceType::Invalid);

inline void AddCompanyNewsItem(EncodedString &&headline, std::unique_ptr<CompanyNewsInformation> cni)
{
	AddNewsItem(std::move(headline), NewsType::CompanyInfo, NewsStyle::Company, {}, {}, {}, std::move(cni));
}

/**
 * Adds a newsitem referencing a vehicle.
 *
 * @warning The DParams may not reference the vehicle due to autoreplace stuff. See AddVehicleAdviceNewsItem for how that can be done.
 */
inline void AddVehicleNewsItem(EncodedString &&headline, NewsType type, VehicleID vehicle, StationID station = StationID::Invalid())
{
	AddNewsItem(std::move(headline), type, NewsStyle::Thin, {NewsFlag::NoTransparency, NewsFlag::Shaded}, vehicle, station == StationID::Invalid() ? NewsReference{} : station);
}

/**
 * Adds a vehicle-advice news item.
 *
 * @warning DParam 0 must reference the vehicle!
 */
inline void AddVehicleAdviceNewsItem(AdviceType advice_type, EncodedString &&headline, VehicleID vehicle)
{
	AddNewsItem(std::move(headline), NewsType::Advice, NewsStyle::Small, {NewsFlag::InColour, NewsFlag::VehicleParam0}, vehicle, {}, nullptr, advice_type);
}

inline void AddTileNewsItem(EncodedString &&headline, NewsType type, TileIndex tile, StationID station = StationID::Invalid())
{
	AddNewsItem(std::move(headline), type, NewsStyle::Thin, {NewsFlag::NoTransparency, NewsFlag::Shaded}, tile, station == StationID::Invalid() ? NewsReference{} : station);
}

inline void AddIndustryNewsItem(EncodedString &&headline, NewsType type, IndustryID industry)
{
	AddNewsItem(std::move(headline), type, NewsStyle::Thin, {NewsFlag::NoTransparency, NewsFlag::Shaded}, industry, {});
}

void NewsLoop();
void InitNewsItemStructs();

const NewsItem *GetStatusbarNews();

void DeleteInvalidEngineNews();
void DeleteVehicleNews(VehicleID vid, AdviceType advice_type = AdviceType::Invalid);
void DeleteStationNews(StationID sid);
void DeleteIndustryNews(IndustryID iid);

uint32_t SerialiseNewsReference(const NewsReference &reference);

#endif /* NEWS_FUNC_H */
