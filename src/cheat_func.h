/* $Id$ */

/** @file cheat_func.h Functions related to cheating. */

#ifndef CHEAT_FUNC_H
#define CHEAT_FUNC_H

#include "cheat_type.h"

extern Cheats _cheats;

void ShowCheatWindow();

/**
 * Return true if any cheat has been used, false otherwise
 * @return has a cheat been used?
 */
bool CheatHasBeenUsed();

#endif /* CHEAT_FUNC_H */
