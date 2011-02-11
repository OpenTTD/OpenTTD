/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_func.h Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"
#include "company_type.h"

Window *FindWindowById(WindowClass cls, WindowNumber number);
Window *FindWindowByClass(WindowClass cls);
void ChangeWindowOwner(Owner old_owner, Owner new_owner);

void ResizeWindow(Window *w, int x, int y);
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

void InvalidateWindowData(WindowClass cls, WindowNumber number, int data = 0);
void InvalidateWindowClassesData(WindowClass cls, int data = 0);

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

#endif /* WINDOW_FUNC_H */
