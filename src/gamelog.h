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
	GLAT_SETTING,      ///< Setting changed
	GLAT_GRFBUG,       ///< GRF bug was triggered
	GLAT_EMERGENCY,    ///< Emergency savegame
	GLAT_END,          ///< So we know how many GLATs are there
	GLAT_NONE  = 0xFF, ///< No logging active; in savegames, end of list
};

void GamelogStartAction(GamelogActionType at);
void GamelogStopAction();

void GamelogReset();

typedef void GamelogPrintProc(const char *s);
void GamelogPrint(GamelogPrintProc *proc); // needed for WIN32 / WINCE crash.log

void GamelogPrintDebug(int level);
void GamelogPrintConsole();

void GamelogEmergency();
bool GamelogTestEmergency();

void GamelogRevision();
void GamelogMode();
void GamelogOldver();
void GamelogSetting(const char *name, int32 oldval, int32 newval);

void GamelogGRFUpdate(const GRFConfig *oldg, const GRFConfig *newg);
void GamelogGRFAddList(const GRFConfig *newg);
void GamelogGRFRemove(uint32 grfid);
void GamelogGRFAdd(const GRFConfig *newg);
void GamelogGRFCompatible(const GRFIdentifier *newg);

void GamelogTestRevision();
void GamelogTestMode();
void GamelogTestGRF();

bool GamelogGRFBugReverse(uint32 grfid, uint16 internal_id);

void GamelogGetOriginalGRFMD5Checksum(uint32 grfid, byte *md5sum);

#endif /* GAMELOG_H */
