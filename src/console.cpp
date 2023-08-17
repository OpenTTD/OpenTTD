/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file console.cpp Handling of the in-game console. */

#include "stdafx.h"
#include "console_internal.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_admin.h"
#include "debug.h"
#include "console_func.h"
#include "settings_type.h"

#include "safeguards.h"

static const uint ICON_TOKEN_COUNT = 20;     ///< Maximum number of tokens in one command
static const uint ICON_MAX_RECURSE = 10;     ///< Maximum number of recursion

/* console parser */
/* static */ IConsole::CommandList &IConsole::Commands()
{
	static IConsole::CommandList cmds;
	return cmds;
}

/* static */ IConsole::AliasList &IConsole::Aliases()
{
	static IConsole::AliasList aliases;
	return aliases;
}

FILE *_iconsole_output_file;

void IConsoleInit()
{
	_iconsole_output_file = nullptr;
	_redirect_console_to_client = INVALID_CLIENT_ID;
	_redirect_console_to_admin  = INVALID_ADMIN_ID;

	IConsoleGUIInit();

	IConsoleStdLibRegister();
}

static void IConsoleWriteToLogFile(const std::string &string)
{
	if (_iconsole_output_file != nullptr) {
		/* if there is an console output file ... also print it there */
		const char *header = GetLogPrefix();
		if ((strlen(header) != 0 && fwrite(header, strlen(header), 1, _iconsole_output_file) != 1) ||
				fwrite(string.c_str(), string.size(), 1, _iconsole_output_file) != 1 ||
				fwrite("\n", 1, 1, _iconsole_output_file) != 1) {
			fclose(_iconsole_output_file);
			_iconsole_output_file = nullptr;
			IConsolePrint(CC_ERROR, "Cannot write to console log file; closing the log file.");
		}
	}
}

bool CloseConsoleLogIfActive()
{
	if (_iconsole_output_file != nullptr) {
		IConsolePrint(CC_INFO, "Console log file closed.");
		fclose(_iconsole_output_file);
		_iconsole_output_file = nullptr;
		return true;
	}

	return false;
}

void IConsoleFree()
{
	IConsoleGUIFree();
	CloseConsoleLogIfActive();
}

/**
 * Handle the printing of text entered into the console or redirected there
 * by any other means. Text can be redirected to other clients in a network game
 * as well as to a logfile. If the network server is a dedicated server, all activities
 * are also logged. All lines to print are added to a temporary buffer which can be
 * used as a history to print them onscreen
 * @param colour_code The colour of the command.
 * @param string The message to output on the console (notice, error, etc.)
 */
void IConsolePrint(TextColour colour_code, const std::string &string)
{
	assert(IsValidConsoleColour(colour_code));

	if (_redirect_console_to_client != INVALID_CLIENT_ID) {
		/* Redirect the string to the client */
		NetworkServerSendRcon(_redirect_console_to_client, colour_code, string);
		return;
	}

	if (_redirect_console_to_admin != INVALID_ADMIN_ID) {
		NetworkServerSendAdminRcon(_redirect_console_to_admin, colour_code, string);
		return;
	}

	/* Create a copy of the string, strip it of colours and invalid
	 * characters and (when applicable) assign it to the console buffer */
	std::string str = StrMakeValid(string);

	if (_network_dedicated) {
		NetworkAdminConsole("console", str);
		fmt::print("{}{}\n", GetLogPrefix(), str);
		fflush(stdout);
		IConsoleWriteToLogFile(str);
		return;
	}

	IConsoleWriteToLogFile(str);
	IConsoleGUIPrint(colour_code, str);
}

/**
 * Change a string into its number representation. Supports
 * decimal and hexadecimal numbers as well as 'on'/'off' 'true'/'false'
 * @param *value the variable a successful conversion will be put in
 * @param *arg the string to be converted
 * @return Return true on success or false on failure
 */
bool GetArgumentInteger(uint32_t *value, const char *arg)
{
	char *endptr;

	if (strcmp(arg, "on") == 0 || strcmp(arg, "true") == 0) {
		*value = 1;
		return true;
	}
	if (strcmp(arg, "off") == 0 || strcmp(arg, "false") == 0) {
		*value = 0;
		return true;
	}

	*value = std::strtoul(arg, &endptr, 0);
	return arg != endptr;
}

/**
 * Creates a copy of a string with underscores removed from it
 * @param name String to remove the underscores from.
 * @return A copy of \a name, without underscores.
 */
static std::string RemoveUnderscores(std::string name)
{
	name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
	return name;
}

/**
 * Register a new command to be used in the console
 * @param name name of the command that will be used
 * @param proc function that will be called upon execution of command
 */
/* static */ void IConsole::CmdRegister(const std::string &name, IConsoleCmdProc *proc, IConsoleHook *hook)
{
	IConsole::Commands().try_emplace(RemoveUnderscores(name), name, proc, hook);
}

/**
 * Find the command pointed to by its string
 * @param name command to be found
 * @return return Cmdstruct of the found command, or nullptr on failure
 */
/* static */ IConsoleCmd *IConsole::CmdGet(const std::string &name)
{
	auto item = IConsole::Commands().find(RemoveUnderscores(name));
	if (item != IConsole::Commands().end()) return &item->second;
	return nullptr;
}

/**
 * Register a an alias for an already existing command in the console
 * @param name name of the alias that will be used
 * @param cmd name of the command that 'name' will be alias of
 */
/* static */ void IConsole::AliasRegister(const std::string &name, const std::string &cmd)
{
	auto result = IConsole::Aliases().try_emplace(RemoveUnderscores(name), name, cmd);
	if (!result.second) IConsolePrint(CC_ERROR, "An alias with the name '{}' already exists.", name);
}

/**
 * Find the alias pointed to by its string
 * @param name alias to be found
 * @return return Aliasstruct of the found alias, or nullptr on failure
 */
/* static */ IConsoleAlias *IConsole::AliasGet(const std::string &name)
{
	auto item = IConsole::Aliases().find(RemoveUnderscores(name));
	if (item != IConsole::Aliases().end()) return &item->second;
	return nullptr;
}

/**
 * An alias is just another name for a command, or for more commands
 * Execute it as well.
 * @param *alias is the alias of the command
 * @param tokencount the number of parameters passed
 * @param *tokens are the parameters given to the original command (0 is the first param)
 */
static void IConsoleAliasExec(const IConsoleAlias *alias, byte tokencount, char *tokens[ICON_TOKEN_COUNT], const uint recurse_count)
{
	std::string alias_buffer;

	Debug(console, 6, "Requested command is an alias; parsing...");

	if (recurse_count > ICON_MAX_RECURSE) {
		IConsolePrint(CC_ERROR, "Too many alias expansions, recursion limit reached.");
		return;
	}

	for (const char *cmdptr = alias->cmdline.c_str(); *cmdptr != '\0'; cmdptr++) {
		switch (*cmdptr) {
			case '\'': // ' will double for ""
				alias_buffer += '\"';
				break;

			case ';': // Cmd separator; execute previous and start new command
				IConsoleCmdExec(alias_buffer, recurse_count);

				alias_buffer.clear();

				cmdptr++;
				break;

			case '%': // Some or all parameters
				cmdptr++;
				switch (*cmdptr) {
					case '+': { // All parameters separated: "[param 1]" "[param 2]"
						for (uint i = 0; i != tokencount; i++) {
							if (i != 0) alias_buffer += ' ';
							alias_buffer += '\"';
							alias_buffer += tokens[i];
							alias_buffer += '\"';
						}
						break;
					}

					case '!': { // Merge the parameters to one: "[param 1] [param 2] [param 3...]"
						alias_buffer += '\"';
						for (uint i = 0; i != tokencount; i++) {
							if (i != 0) alias_buffer += " ";
							alias_buffer += tokens[i];
						}
						alias_buffer += '\"';
						break;
					}

					default: { // One specific parameter: %A = [param 1] %B = [param 2] ...
						int param = *cmdptr - 'A';

						if (param < 0 || param >= tokencount) {
							IConsolePrint(CC_ERROR, "Too many or wrong amount of parameters passed to alias.");
							IConsolePrint(CC_HELP, "Usage of alias '{}': '{}'.", alias->name, alias->cmdline);
							return;
						}

						alias_buffer += '\"';
						alias_buffer += tokens[param];
						alias_buffer += '\"';
						break;
					}
				}
				break;

			default:
				alias_buffer += *cmdptr;
				break;
		}

		if (alias_buffer.size() >= ICON_MAX_STREAMSIZE - 1) {
			IConsolePrint(CC_ERROR, "Requested alias execution would overflow execution buffer.");
			return;
		}
	}

	IConsoleCmdExec(alias_buffer, recurse_count);
}

/**
 * Execute a given command passed to us. First chop it up into
 * individual tokens (separated by spaces), then execute it if possible
 * @param command_string string to be parsed and executed
 */
void IConsoleCmdExec(const std::string &command_string, const uint recurse_count)
{
	const char *cmdptr;
	char *tokens[ICON_TOKEN_COUNT], tokenstream[ICON_MAX_STREAMSIZE];
	uint t_index, tstream_i;

	bool longtoken = false;
	bool foundtoken = false;

	if (command_string[0] == '#') return; // comments

	for (cmdptr = command_string.c_str(); *cmdptr != '\0'; cmdptr++) {
		if (!IsValidChar(*cmdptr, CS_ALPHANUMERAL)) {
			IConsolePrint(CC_ERROR, "Command '{}' contains malformed characters.", command_string);
			return;
		}
	}

	Debug(console, 4, "Executing cmdline: '{}'", command_string);

	memset(&tokens, 0, sizeof(tokens));
	memset(&tokenstream, 0, sizeof(tokenstream));

	/* 1. Split up commandline into tokens, separated by spaces, commands
	 * enclosed in "" are taken as one token. We can only go as far as the amount
	 * of characters in our stream or the max amount of tokens we can handle */
	for (cmdptr = command_string.c_str(), t_index = 0, tstream_i = 0; *cmdptr != '\0'; cmdptr++) {
		if (tstream_i >= lengthof(tokenstream)) {
			IConsolePrint(CC_ERROR, "Command line too long.");
			return;
		}

		switch (*cmdptr) {
		case ' ': // Token separator
			if (!foundtoken) break;

			if (longtoken) {
				tokenstream[tstream_i] = *cmdptr;
			} else {
				tokenstream[tstream_i] = '\0';
				foundtoken = false;
			}

			tstream_i++;
			break;
		case '"': // Tokens enclosed in "" are one token
			longtoken = !longtoken;
			if (!foundtoken) {
				if (t_index >= lengthof(tokens)) {
					IConsolePrint(CC_ERROR, "Command line too long.");
					return;
				}
				tokens[t_index++] = &tokenstream[tstream_i];
				foundtoken = true;
			}
			break;
		case '\\': // Escape character for ""
			if (cmdptr[1] == '"' && tstream_i + 1 < lengthof(tokenstream)) {
				tokenstream[tstream_i++] = *++cmdptr;
				break;
			}
			FALLTHROUGH;
		default: // Normal character
			tokenstream[tstream_i++] = *cmdptr;

			if (!foundtoken) {
				if (t_index >= lengthof(tokens)) {
					IConsolePrint(CC_ERROR, "Command line too long.");
					return;
				}
				tokens[t_index++] = &tokenstream[tstream_i - 1];
				foundtoken = true;
			}
			break;
		}
	}

	for (uint i = 0; i < lengthof(tokens) && tokens[i] != nullptr; i++) {
		Debug(console, 8, "Token {} is: '{}'", i, tokens[i]);
	}

	if (StrEmpty(tokens[0])) return; // don't execute empty commands
	/* 2. Determine type of command (cmd or alias) and execute
	 * First try commands, then aliases. Execute
	 * the found action taking into account its hooking code
	 */
	IConsoleCmd *cmd = IConsole::CmdGet(tokens[0]);
	if (cmd != nullptr) {
		ConsoleHookResult chr = (cmd->hook == nullptr ? CHR_ALLOW : cmd->hook(true));
		switch (chr) {
			case CHR_ALLOW:
				if (!cmd->proc(t_index, tokens)) { // index started with 0
					cmd->proc(0, nullptr); // if command failed, give help
				}
				return;

			case CHR_DISALLOW: return;
			case CHR_HIDE: break;
		}
	}

	t_index--;
	IConsoleAlias *alias = IConsole::AliasGet(tokens[0]);
	if (alias != nullptr) {
		IConsoleAliasExec(alias, t_index, &tokens[1], recurse_count + 1);
		return;
	}

	IConsolePrint(CC_ERROR, "Command '{}' not found.", tokens[0]);
}
