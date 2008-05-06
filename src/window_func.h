/* $Id$ */

/** @file window_func.h Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"
#include "player_type.h"

void SetWindowDirty(const Window *w);
void SendWindowMessage(WindowClass wnd_class, WindowNumber wnd_num, int msg, int wparam, int lparam);
void SendWindowMessageClass(WindowClass wnd_class, int msg, int wparam, int lparam);

Window *FindWindowById(WindowClass cls, WindowNumber number);
void ChangeWindowOwner(PlayerID old_player, PlayerID new_player);

void ResizeWindow(Window *w, int x, int y);
int PositionMainToolbar(Window *w);

void InitWindowSystem();
void UnInitWindowSystem();
void ResetWindowSystem();
void SetupColorsAndInitialWindow();
void InputLoop();
void InvalidateThisWindowData(Window *w);
void InvalidateWindowData(WindowClass cls, WindowNumber number);

void DeleteNonVitalWindows();
void DeleteAllNonVitalWindows();
void HideVitalWindows();
void ShowVitalWindows();

void InvalidateWindow(WindowClass cls, WindowNumber number);
void InvalidateWindowWidget(WindowClass cls, WindowNumber number, byte widget_index);
void InvalidateWindowClasses(WindowClass cls);
void InvalidateWindowClassesData(WindowClass cls);
void DeleteWindowById(WindowClass cls, WindowNumber number);
void DeleteWindowByClass(WindowClass cls);

#endif /* WINDOW_FUNC_H */
