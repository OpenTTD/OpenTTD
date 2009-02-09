/* $Id$ */

/** @file window_func.h Window functions not directly related to making/drawing windows. */

#ifndef WINDOW_FUNC_H
#define WINDOW_FUNC_H

#include "window_type.h"
#include "company_type.h"

void SetWindowDirty(const Window *w);

Window *FindWindowById(WindowClass cls, WindowNumber number);
void ChangeWindowOwner(Owner old_owner, Owner new_owner);

void ResizeWindow(Window *w, int x, int y);
int PositionMainToolbar(Window *w);

void InitWindowSystem();
void UnInitWindowSystem();
void ResetWindowSystem();
void SetupColoursAndInitialWindow();
void InputLoop();

void InvalidateThisWindowData(Window *w, int data = 0);
void InvalidateWindowData(WindowClass cls, WindowNumber number, int data = 0);
void InvalidateWindowClassesData(WindowClass cls, int data = 0);

void DeleteNonVitalWindows();
void DeleteAllNonVitalWindows();
void DeleteConstructionWindows();
void HideVitalWindows();
void ShowVitalWindows();

void InvalidateWindowWidget(WindowClass cls, WindowNumber number, byte widget_index);
void InvalidateWindow(WindowClass cls, WindowNumber number);
void InvalidateWindowClasses(WindowClass cls);

void DeleteWindowById(WindowClass cls, WindowNumber number, bool force = true);
void DeleteWindowByClass(WindowClass cls);

#endif /* WINDOW_FUNC_H */
