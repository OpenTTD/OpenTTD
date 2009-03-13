/* $Id$ */

/** @file signs_func.h Functions related to signs. */

#ifndef SIGNS_FUNC_H
#define SIGNS_FUNC_H

#include "signs_type.h"

extern SignID _new_sign_id;

void UpdateAllSignVirtCoords();
void PlaceProc_Sign(TileIndex tile);
void MarkSignDirty(Sign *si);
void UpdateSignVirtCoords(Sign *si);

/* signs_gui.cpp */
void ShowRenameSignWindow(const Sign *si);
void HandleClickOnSign(const Sign *si);
void DeleteRenameSignWindow(SignID sign);

void ShowSignList();

#endif /* SIGNS_FUNC_H */
