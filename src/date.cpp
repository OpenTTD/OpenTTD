/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date.cpp Handling of dates in our native format and transforming them to something human readable. */

#include "stdafx.h"
#include "network/network.h"
#include "network/network_func.h"
#include "currency.h"
#include "window_func.h"
#include "settings_type.h"
#include "date_func.h"
#include "vehicle_base.h"
#include "rail_gui.h"
#include "linkgraph/linkgraph.h"
#include "saveload/saveload.h"
#include "newgrf_profiling.h"
#include "widgets/statusbar_widget.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "safeguards.h"


