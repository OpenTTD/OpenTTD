#ifndef CONSOLE_CMDS_H
#define CONSOLE_CMDS_H

/* Console_CMDS.h is the placeholder of all the console commands
 * that will be added to the game. Register the command in
 * * console.c IConsoleStdLibRegister;
 * then put the command in the appropiate place (eg. where it belongs, stations
 * stuff in station_cmd.c, etc.), and add the function decleration here.
 */

_iconsole_var * IConsoleResetEngines(byte argc, byte* argv[], byte argt[]);

#endif /* CONSOLE_CMDS_H */
