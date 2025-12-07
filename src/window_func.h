/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_func.h %Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"
#include "company_type.h"
#include "core/geometry_type.hpp"

Window *FindWindowById(WindowClass cls, WindowNumber number);
Window *FindWindowByClass(WindowClass cls);
Window *GetMainWindow();
void ChangeWindowOwner(Owner old_owner, Owner new_owner);

void ResizeWindow(Window *w, int x, int y, bool clamp_to_screen = true, bool schedule_resize = true);
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
void InvalidateWindowData(WindowClass cls, WindowNumber number, ConvertibleThroughBase auto data, bool gui_scope = false) { InvalidateWindowData(cls, number, data.base(), gui_scope); }
void InvalidateWindowClassesData(WindowClass cls, int data = 0, bool gui_scope = false);
void InvalidateWindowClassesData(WindowClass cls, ConvertibleThroughBase auto data, bool gui_scope = false) { InvalidateWindowClassesData(cls, data.base(), gui_scope); }

void CloseNonVitalWindows();
void CloseAllNonVitalWindows();
void DeleteAllMessages();
void CloseConstructionWindows();
void HideVitalWindows();
void ShowVitalWindows();

/**
 * Re-initialize all windows.
 * @param zoom_changed Set if windows are being re-initialized due to a zoom level changed.
 */
void ReInitAllWindows(bool zoom_changed);

void SetWindowWidgetDirty(WindowClass cls, WindowNumber number, WidgetID widget_index);
void SetWindowDirty(WindowClass cls, WindowNumber number);
void SetWindowClassesDirty(WindowClass cls);

void CloseWindowById(WindowClass cls, WindowNumber number, bool force = true, int data = 0);
void CloseWindowByClass(WindowClass cls, int data = 0);

bool EditBoxInGlobalFocus();
bool FocusedWindowIsConsole();
Point GetCaretPosition();

#endif /* WINDOW_FUNC_H */
