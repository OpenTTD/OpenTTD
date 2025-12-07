/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file date_gui.h Functions related to the graphical selection of a date. */

#ifndef DATE_GUI_H
#define DATE_GUI_H

#include "timer/timer_game_economy.h"
#include "window_type.h"

/**
 * Callback for when a date has been chosen
 * @param w the window that sends the callback
 * @param date the date that has been chosen
 */
using SetDateCallback = std::function<void (const Window *w, TimerGameEconomy::Date date)>;

void ShowSetDateWindow(Window *parent, int window_number, TimerGameEconomy::Date initial_date, TimerGameEconomy::Year min_year, TimerGameEconomy::Year max_year, SetDateCallback &&callback);

#endif /* DATE_GUI_H */
