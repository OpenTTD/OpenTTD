/* $Id$ */

/** @file window.h Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"

/**
 * Marks the window as dirty for repaint.
 *
 * @ingroup dirty
 */
void SetWindowDirty(const Window *w);
void SendWindowMessage(WindowClass wnd_class, WindowNumber wnd_num, int msg, int wparam, int lparam);
void SendWindowMessageClass(WindowClass wnd_class, int msg, int wparam, int lparam);

Window *FindWindowById(WindowClass cls, WindowNumber number);
void DeleteWindow(Window *w);
void DeletePlayerWindows(PlayerID pi);
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
void RelocateAllWindows(int neww, int newh);

void DeleteNonVitalWindows();
void DeleteAllNonVitalWindows();
void HideVitalWindows();
void ShowVitalWindows();
Window **FindWindowZPosition(const Window *w);

#endif /* WINDOW_FUNC_H */
