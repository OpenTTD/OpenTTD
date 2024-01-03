# Debugging / reporting desyncs

As desyncs are hard to make reproducible OpenTTD has the ability to log all
actions done by clients so we can replay the whole game in an effort to make
desyncs better reproducible. You need to turn this ability on. When turned
on an automatic savegame will be made once the map has been constructed in
the 'save/autosave' directory, see OpenTTD directories to know where to find
this directory. Furthermore the log file 'commands-out.log' will be created
and all actions will be written to there.

To enable the desync debugging you need to set the debug level for 'desync'
to at least 1. You do this by starting OpenTTD with '`-d desync=<level>`' as
parameter or by typing '`debug_level desync=<level>`' in OpenTTD's internal
console.
The desync debug levels are:

- 0: nothing.
- 1: dumping of commands to 'commands-out.log'.
- 2: same as 1 plus checking vehicle caches and dumping that too.
- 3: same as 2 plus monthly saves in autosave.
- 4 and higher: same as 3

Restarting OpenTTD will overwrite 'commands-out.log'. OpenTTD will not remove
the savegames (dmp_cmds_*.sav) made by the desync debugging system, so you
have to occasionally remove them yourself!

The naming format of the desync savegames is as follows:
dmp_cmds_XXXXXXXX_YYYYYYYY.sav. The XXXXXXXX is the hexadecimal representation
of the generation seed of the game and YYYYYYYY is the hexadecimal
representation of the date of the game. This sorts the savegames by game and
then by date making it easier to find the right savegames.

When a desync has occurred with the desync debugging turned on you should file
a bug report with the following files attached:

- commands-out.log as it contains all the commands that were done
- the last saved savegame (search for the last line beginning with
   'save: dmp_cmds_' in commands-out.log). We use this savegame to check
   whether we can quickly reproduce the desync. Otherwise we will need …
- the first saved savegame (search for the first line beginning with 'save'
   where the first part, up to the last underscore '_', is the same). We need
   this savegame to be able to reproduce the bug when the last savegame is not
   old enough. If you loaded a scenario or savegame you need to attach that.
- optionally you can attach the savegames from around 50%, 75%, 85%, 90% and
   95% of the game's progression. We can use these savegames to speed up the
   reproduction of the desync, but we should be able to reproduce these
   savegames based on the first savegame and commands-out.log.
- in case you use any NewGRFs you should attach the ones you used unless
   we can easily find them ourselves via bananas or when they are in the
   #openttdcoop pack.

Do NOT remove the dmp_cmds savegames of a desync you have reported until the
desync has been fixed; if you, by accident, send us the wrong savegames we
will not be able to reproduce the desync and thus will be unable to fix it.

## More information

You can find more theory on the causes and debugging of desyncs in the
[desync documentation](./desync.md).
