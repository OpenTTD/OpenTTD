/* $Id$ */

/** @file news_func.h Functions related to news. */

#ifndef NEWS_FUNC_H
#define NEWS_FUNC_H

#include "news_type.h"
#include "vehicle_type.h"

void AddNewsItem(StringID string, NewsMode mode, NewsFlag flag, NewsType type, NewsCallback callback, uint data_a, uint data_b);
void NewsLoop();
void DrawNewsBorder(const Window *w);
void InitNewsItemStructs();

extern NewsItem _statusbar_news_item;
extern bool _news_ticker_sound;

extern NewsTypeData _news_type_data[NT_END];

/**
 * Delete a news item type about a vehicle
 * if the news item type is INVALID_STRING_ID all news about the vehicle get
 * deleted
 */
void DeleteVehicleNews(VehicleID, StringID news);

#endif /* NEWS_FUNC_H */
