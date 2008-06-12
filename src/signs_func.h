/* $Id$ */

/** @file signs_func.h Functions related to signs. */

#ifndef SIGNS_FUNC_H
#define SIGNS_FUNC_H

#include "signs_type.h"

extern SignID _new_sign_id;
extern bool _sign_sort_dirty;

void UpdateAllSignVirtCoords();
void PlaceProc_Sign(TileIndex tile);

/* signs_gui.cpp */
void ShowRenameSignWindow(const Sign *si);
void HandleClickOnSign(const Sign *si);

void ShowSignList();

#endif /* SIGNS_FUNC_H */
