/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_gui.h Functions related to the graphical selection of a date. */

#ifndef DATE_GUI_H
#define DATE_GUI_H

#include "timer/timer_game_calendar.h"
#include "window_type.h"

/**
 * Callback for when a date has been chosen
 * @param w the window that sends the callback
 * @param date the date that has been chosen
 */
typedef void SetDateCallback(const Window *w, TimerGameCalendar::Date date, void *data);

void ShowSetDateWindow(Window *parent, int window_number, TimerGameCalendar::Date initial_date, TimerGameCalendar::Year min_year, TimerGameCalendar::Year max_year, SetDateCallback *callback, void *callback_data);

#endif /* DATE_GUI_H */
