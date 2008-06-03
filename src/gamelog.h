/* $Id$ */

/** @file gamelog.h Functions to be called to log possibly unsafe game events */

#ifndef GAMELOG_H
#define GAMELOG_H

#include "newgrf_config.h"

enum GamelogActionType {
	GLAT_START,        ///< Game created
	GLAT_LOAD,         ///< Game loaded
	GLAT_GRF,          ///< GRF changed
	GLAT_CHEAT,        ///< Cheat was used
	GLAT_PATCH,        ///< Patches setting changed
	GLAT_END,          ///< So we know how many GLATs are there
	GLAT_NONE  = 0xFF, ///< No logging active; in savegames, end of list
};

void GamelogStartAction(GamelogActionType at);
void GamelogStopAction();

void GamelogReset();

typedef void GamelogPrintProc(const char *s);
void GamelogPrint(GamelogPrintProc *proc); // needed for WIN32 / WINCE crash.log

void GamelogPrintDebug();
void GamelogPrintConsole();

void GamelogRevision();
void GamelogMode();
void GamelogOldver();
void GamelogPatch(const char *name, int32 oldval, int32 newval);

void GamelogGRFUpdate(const GRFConfig *oldg, const GRFConfig *newg);
void GamelogGRFAddList(const GRFConfig *newg);
void GamelogGRFRemove(uint32 grfid);
void GamelogGRFAdd(const GRFConfig *newg);
void GamelogGRFCompatible(const GRFIdentifier *newg);

void GamelogTestRevision();
void GamelogTestMode();
void GamelogTestGRF();

#endif /* GAMELOG_H */
