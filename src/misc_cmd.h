/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_cmd.h Miscellaneous command definitions. */

#ifndef MISC_CMD_H
#define MISC_CMD_H

#include "command_type.h"

CommandProc CmdMoneyCheat;
CommandProc CmdChangeBankBalance;
CommandProc CmdIncreaseLoan;
CommandProc CmdDecreaseLoan;
CommandProc CmdPause;

DEF_CMD_TRAIT(CMD_MONEY_CHEAT,         CmdMoneyCheat,        CMD_OFFLINE,             CMDT_CHEAT)
DEF_CMD_TRAIT(CMD_CHANGE_BANK_BALANCE, CmdChangeBankBalance, CMD_DEITY,               CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INCREASE_LOAN,       CmdIncreaseLoan,      0,                       CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DECREASE_LOAN,       CmdDecreaseLoan,      0,                       CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_PAUSE,               CmdPause,             CMD_SERVER | CMD_NO_EST, CMDT_SERVER_SETTING)

#endif /* MISC_CMD_H */
