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

void AddNewsItem(StringID string, NewsType type, NewsFlag flags, NewsReference ref1 = {}, NewsReference ref2 = {}, std::unique_ptr<NewsAllocatedData> &&data = nullptr, AdviceType advice_type = AdviceType::Invalid);

inline void AddCompanyNewsItem(StringID string, std::unique_ptr<CompanyNewsInformation> cni)
{
	AddNewsItem(string, NT_COMPANY_INFO, NF_COMPANY, {}, {}, std::move(cni));
}

/**
 * Adds a newsitem referencing a vehicle.
 *
 * @warning The DParams may not reference the vehicle due to autoreplace stuff. See AddVehicleAdviceNewsItem for how that can be done.
 */
inline void AddVehicleNewsItem(StringID string, NewsType type, const Vehicle *vehicle, const Station *station = nullptr)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, vehicle, station == nullptr ? NewsReference{} : station);
}

/**
 * Adds a vehicle-advice news item.
 *
 * @warning DParam 0 must reference the vehicle!
 */
inline void AddVehicleAdviceNewsItem(AdviceType advice_type, StringID string, const Vehicle *vehicle)
{
	AddNewsItem(string, NT_ADVICE, NF_INCOLOUR | NF_SMALL | NF_VEHICLE_PARAM0, vehicle, {}, nullptr, advice_type);
}

inline void AddTileNewsItem(StringID string, NewsType type, TileIndex tile, const Station *station = nullptr)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, tile, station == nullptr ? NewsReference{} : station);
}

inline void AddIndustryNewsItem(StringID string, NewsType type, const Industry *industry)
{
	AddNewsItem(string, type, NF_NO_TRANSPARENT | NF_SHADE | NF_THIN, industry);
}

void NewsLoop();
void InitNewsItemStructs();

const NewsItem *GetStatusbarNews();

void DeleteEngineNews();
void DeleteVehicleNews(const Vehicle *vehicle, AdviceType advice_type = AdviceType::Invalid);
void DeleteStationNews(const Station *station);
void DeleteIndustryNews(const Industry *industry);

uint32_t SerialiseNewsReference(const NewsReference &reference);

#endif /* NEWS_FUNC_H */
