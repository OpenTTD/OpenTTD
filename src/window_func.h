/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_func.h %Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"
#include "company_type.h"
#include "core/geometry_type.hpp"

Window *FindWindowById(WindowClass cls, WindowNumber number);
Window *FindWindowByClass(WindowClass cls);
void ChangeWindowOwner(Owner old_owner, Owner new_owner);

void ResizeWindow(Window *w, int x, int y, bool clamp_to_screen = true);
int PositionMainToolbar(Window *w);
int PositionStatusbar(Window *w);
int PositionNewsMessage(Window *w);
int PositionNetworkChatWindow(Window *w);

int GetMainViewTop();
int GetMainViewBottom();

void InitWindowSystem();
void UnInitWindowSystem();
void ResetWindowSystem();
void SetupColoursAndInitialWindow();
void InputLoop();

void InvalidateWindowData(WindowClass cls, WindowNumber number, int data = 0, bool gui_scope = false);
void InvalidateWindowClassesData(WindowClass cls, int data = 0, bool gui_scope = false);

void DeleteNonVitalWindows();
void DeleteAllNonVitalWindows();
void DeleteConstructionWindows();
void HideVitalWindows();
void ShowVitalWindows();

void ReInitAllWindows();

void SetWindowWidgetDirty(WindowClass cls, WindowNumber number, byte widget_index);
void SetWindowDirty(WindowClass cls, WindowNumber number);
void SetWindowClassesDirty(WindowClass cls);

void DeleteWindowById(WindowClass cls, WindowNumber number, bool force = true);
void DeleteWindowByClass(WindowClass cls);

bool EditBoxInGlobalFocus();
Point GetCaretPosition();

/**
 * Count how many times the interval has elapsed, and update the timer.
 * Use to ensure a specific amount of events happen within a timeframe, e.g. for animation.
 * The timer value does not need to be initialised.
 * @param timer Timer to test. Value will be increased.
 * @param delta Time since last test.
 * @param interval Timing interval.
 * @return Number of times the interval has elapsed.
 */
static inline uint CountIntervalElapsed(uint &timer, uint delta, uint interval)
{
	uint count = delta / interval;
	if (timer + (delta % interval) >= interval) count++;
	timer = (timer + delta) % interval;
	return count;
}

/**
 * Test if a timer has elapsed, and update the timer.
 * Use to ensure an event happens only once within a timeframe, e.g. for window updates.
 * The timer value must be initialised in order for the timer to elapsed.
 * @param timer Timer to test. Value will be decreased.
 * @param delta Time since last test.
 * @return True iff the timer has elapsed.
 */
static inline bool TimerElapsed(int &timer, uint delta)
{
	if (timer <= 0) return false;
	timer -= delta;
	return timer <= 0;
}

#endif /* WINDOW_FUNC_H */
