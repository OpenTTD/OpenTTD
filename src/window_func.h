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
#include "core/strong_typedef_type.hpp"

Window *FindWindowById(WindowClass cls, WindowNumber number);
Window *FindWindowByClass(WindowClass cls);
Window *GetMainWindow();
void ChangeWindowOwner(Owner old_owner, Owner new_owner);

template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
Window *FindWindowById(WindowClass cls, T number)
{
	return FindWindowById(cls, number.base());
}

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

template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
void InvalidateWindowData(WindowClass cls, T number, int data = 0, bool gui_scope = false)
{
	InvalidateWindowData(cls, number.base(), data, gui_scope);
}

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

template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
void SetWindowDirty(WindowClass cls, T number)
{
	SetWindowDirty(cls, number.base());
}

void CloseWindowById(WindowClass cls, WindowNumber number, bool force = true, int data = 0);
void CloseWindowByClass(WindowClass cls, int data = 0);

template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
void CloseWindowById(WindowClass cls, T number, bool force = true, int data = 0)
{
	CloseWindowById(cls, number.base(), force, data);
}

bool EditBoxInGlobalFocus();
bool FocusedWindowIsConsole();
Point GetCaretPosition();

#endif /* WINDOW_FUNC_H */
