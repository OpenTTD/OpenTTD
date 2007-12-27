/* $Id$ */

/** @file ship_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "ship.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "gui.h"
#include "window_gui.h"
#include "viewport.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "vehicle_func.h"

void DrawShipImage(const Vehicle *v, int x, int y, VehicleID selection)
{
	DrawSprite(v->GetImage(DIR_W), GetVehiclePalette(v), x + 32, y + 10);

	if (v->index == selection) {
		DrawFrameRect(x - 5, y - 1, x + 67, y + 21, 15, FR_BORDERONLY);
	}
}

void CcBuildShip(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;
	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (v->tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v);
	}
	ShowVehicleViewWindow(v);
}

/**
* Draw the details for the given vehicle at the position (x,y)
*
* @param v current vehicle
* @param x The x coordinate
* @param y The y coordinate
*/
void DrawShipDetails(const Vehicle *v, int x, int y)
{
	SetDParam(0, v->engine_type);
	SetDParam(1, v->build_year);
	SetDParam(2, v->value);
	DrawString(x, y, STR_9816_BUILT_VALUE, TC_FROMSTRING);

	SetDParam(0, v->cargo_type);
	SetDParam(1, v->cargo_cap);
	DrawString(x, y + 10, STR_9817_CAPACITY, TC_FROMSTRING);

	StringID str = STR_8812_EMPTY;
	if (!v->cargo.Empty()) {
		SetDParam(0, v->cargo_type);
		SetDParam(1, v->cargo.Count());
		SetDParam(2, v->cargo.Source());
		str = STR_8813_FROM;
	}
	DrawString(x, y + 21, str, TC_FROMSTRING);

	/* Draw Transfer credits text */
	SetDParam(0, v->cargo.FeederShare());
	DrawString(x, y + 33, STR_FEEDER_CARGO_VALUE, TC_FROMSTRING);
}
