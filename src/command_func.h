/* $Id$ */

/** @file command_func.h Functions related to commands. */

#ifndef COMMAND_FUNC_H
#define COMMAND_FUNC_H

#include "command_type.h"

/**
 * Checks if a command failes.
 *
 * As you see the parameter is not a command but the return value of a command,
 * the CommandCost class. This function checks if the command executed by
 * the CommandProc function failed and returns true if it does.
 *
 * @param cost The return value of a CommandProc call
 * @return true if the command failes
 * @see CmdSucceded
 */
static inline bool CmdFailed(CommandCost cost) { return cost.Failed(); }

/**
 * Checks if a command succeeded.
 *
 * As #CmdFailed this function checks if a command succeeded
 *
 * @param cost The return value of a CommandProc call
 * @return true if the command succeeded
 * @see CmdSucceeded
 */
static inline bool CmdSucceeded(CommandCost cost) { return cost.Succeeded(); }

/**
 * Define a default return value for a failed command.
 *
 * This variable contains a CommandCost object with is declared as "failed".
 * Other functions just need to return this error if there is an error,
 * which doesn't need to specific by a StringID.
 */
static const CommandCost CMD_ERROR = CommandCost(INVALID_STRING_ID);

/**
 * Returns from a function with a specific StringID as error.
 *
 * This macro is used to return from a function. The parameter contains the
 * StringID which will be returned.
 *
 * @param errcode The StringID to return
 */
#define return_cmd_error(errcode) return CommandCost(errcode);

/**
 * Execute a command
 */
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint32 procc);

/**
 * Execute a network safe DoCommand function
 */
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd, bool my_cmd = true);

#ifdef ENABLE_NETWORK

/**
 * Send a command over the network
 */
void NetworkSend_Command(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback);
#endif /* ENABLE_NETWORK */

/**
 * Text, which gets sent with a command
 *
 * This variable contains a string (be specific a pointer of the first
 * char of this string) which will be send with a command. This is
 * used for user input data like names or chat messages.
 */
extern const char *_cmd_text;
extern Money _additional_cash_required;

/**
 * Checks if a integer value belongs to a command.
 */
bool IsValidCommand(uint32 cmd);
/**
 * Returns the flags from a given command.
 */
byte GetCommandFlags(uint32 cmd);
/**
 * Returns the current money available which can be used for a command.
 */
Money GetAvailableMoneyForCommand();

#endif /* COMMAND_FUNC_H */
