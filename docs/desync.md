# Some explanations about Desyncs

Last updated: 2014-02-23

## Table of contents

- 1.0) Desync theory
    - 1.1) [OpenTTD multiplayer architecture](#11-openttd-multiplayer-architecture)
    - 1.2) [What is a Desync and how is it detected](#12-what-is-a-desync-and-how-is-it-detected)
    - 1.3) [Typical causes of Desyncs](#13-typical-causes-of-desyncs)
- 2.0) What to do in case of a Desync
    - 2.1) [Cache debugging](#21-cache-debugging)
    - 2.2) [Desync recording](#22-desync-recording)
- 3.0) Evaluating the Desync records
    - 3.1) [Replaying](#31-replaying)
    - 3.2) [Evaluation of the replay](#32-evaluation-of-the-replay)
    - 3.3) [Comparing savegames](#33-comparing-savegames)


## 1.1) OpenTTD multiplayer architecture

  OpenTTD has a huge gamestate, which changes all of the time.
  The savegame contains the complete gamestate at a specific point
  in time. But this state changes completely each tick: Vehicles move
  and trees grow.

  However, most of these changes in the gamestate are deterministic:
  Without a player interfering a vehicle follows its orders always
  in the same way, and trees always grow the same.

  In OpenTTD multiplayer synchronisation works by creating a savegame
  when clients join, and then transferring that savegame to the client,
  so it has the complete gamestate at a fixed point in time.

  Afterwards clients only receive 'commands', that is: Stuff which is
  not predictable, like
   - player actions
   - AI actions
   - GameScript actions
   - Admin Port command
   - rcon commands
   - ...

  These commands contain the information on how to execute the command,
  and when to execute it. Time is measured in 'network frames'.
  Mind that network frames to not match ingame time. Network frames
  also run while the game is paused, to give a defined behaviour to
  stuff that is executing while the game is paused.

  The deterministic part of the gamestate is run by the clients on
  their own. All they get from the server is the instruction to
  run the gamestate up to a certain network time, which basically
  says that there are no commands scheduled in that time.

  When a client (which includes the server itself) wants to execute
  a command (i.e. a non-predictable action), it does this by
   - calling DoCommandP resp. DoCommandPInternal
   - These functions first do a local test-run of the command to
     check simple preconditions. (Just to give the client an
     immediate response without bothering the server and waiting for
     the response.) The test-run may not actually change the
     gamestate, all changes must be discarded.
   - If the local test-run succeeds the command is sent to the server.
   - The server inserts the command into the command queue, which
     assigns a network frame to the commands, i.e. when it shall be
     executed on all clients.
   - Enhanced with this specific timestamp, the command is send to all
     clients, which execute the command simultaneously in the same
     network frame in the same order.

## 1.2) What is a Desync and how is it detected

  In the ideal case all clients have the same gamestate as the server
  and run in sync. That is, vehicle movement is the same on all
  clients, and commands are executed the same everywhere and
  have the same results.

  When a Desync happens, it means that the gamestates on the clients
  (including the server) are no longer the same. Just imagine
  that a vehicle picks the left line instead of the right line at
  a junction on one client.

  The important thing here is, that no one notices when a Desync
  occurs. The desync client will continue to simulate the gamestate
  and execute commands from the server. Once the gamestate differs
  it will increasingly spiral out of control: If a vehicle picks a
  different route, it will arrive at a different time at a station,
  which will load different cargo, which causes other vehicles to
  load other stuff, which causes industries to notice different
  servicing, which causes industries to change production, ...
  the client could run all day in a different universe.

  To limit how long a Desync can remain unnoticed, the server
  transfers some checksums every now and then for the gamestate.
  Currently this checksum is the state of the random number
  generator of the game logic. A lot of things in OpenTTD depend
  on the RNG, and if the gamestate differs, it is likely that the
  RNG is called at different times, and the state differs when
  checked.

  The clients compare this 'checksum' with the checksum of their
  own gamestate at the specific network frame. If they differ,
  the client disconnects with a Desync error.

  The important thing here is: The detection of the Desync is
  only an ultimate failure detection. It does not give any
  indication on when the Desync happened. The Desync may after
  all have occurred long ago, and just did not affect the checksum
  up to now. The checksum may have matched 10 times or more
  since the Desync happened, and only now the Desync has spiraled
  enough to finally affect the checksum. (There was once a desync
  which was only noticed by the checksum after 20 game years.)

## 1.3) Typical causes of Desyncs

  Desyncs can be caused by the following scenarios:
   - The savegame does not describe the complete gamestate.
      - Some information which affects the progression of the
        gamestate is not saved in the savegame.
      - Some information which affects the progression of the
        gamestate is not loaded from the savegame.
        This includes the case that something is not completely
        reset before loading the savegame, so data from the
        previous game is carried over to the new one.
   - The gamestate does not behave deterministic.
      - Cache mismatch: The game logic depends on some cached
        values, which are not invalidated properly. This is
        the usual case for NewGRF-specific Desyncs.
      - Undefined behaviour: The game logic performs multiple
        things in an undefined order or with an undefined
        result. E.g. when sorting something with a key while
        some keys are equal. Or some computation that depends
        on the CPU architecture (32/64 bit, little/big endian).
   - The gamestate is modified when it shall not be modified.
      - The test-run of a command alters the gamestate.
      - The gamestate is altered by a player or script without
        using commands.


## 2.1) Cache debugging

  Desyncs which are caused by improper cache validation can
  often be found by enabling cache validation:
   - Start OpenTTD with '-d desync=2'.
   - This will enable validation of caches every tick.
     That is, cached values are recomputed every tick and compared
     to the cached value.
   - Differences are logged to 'commands-out.log' in the autosave
     folder.

  Mind that this type of debugging can also be done in singleplayer.

## 2.2) Desync recording

  If you have a server, which happens to encounter Desyncs often,
  you can enable recording of the gamestate alterations. This
  will later allow the replay the gamestate and locate the Desync
  cause.

  There are two levels of Desync recording, which are enabled
  via '-d desync=2' resp. '-d desync=3'. Both will record all
  commands to a file 'commands-out.log' in the autosave folder.

  If you have the savegame from the start of the server, and
  this command log you can replay the whole game. (see Section 3.1)

  If you do not start the server from a savegame, there will
  also be a savegame created just after a map has been generated.
  The savegame will be named 'dmp_cmds_*.sav' and be put into
  the autosave folder.

  In addition to that '-d desync=3' also creates regular savegames
  at defined spots in network time. (more defined than regular
  autosaves). These will be created in the autosave folder
  and will also be named 'dmp_cmds_*.sav'.

  These saves allow comparing the gamestate with the original
  gamestate during replaying, and thus greatly help debugging.
  However, they also take a lot of disk space.


## 3.1) Replaying

  To replay a Desync recording, you need these files:
   - The savegame from when the server was started, resp.
     the automatically created savegame from when the map
     was generated.
   - The 'commands-out.log' file.
   - Optionally the 'dmp_cmds_*.sav'.
  Put these files into a safe spot. (Not your autosave folder!)

  Next, prepare your OpenTTD for replaying:
   - Get the same version of OpenTTD as the original server was running.
   - Uncomment/enable the define 'DEBUG_DUMP_COMMANDS' in
     'src/network/network_func.h'.
     (DEBUG_FAILED_DUMP_COMMANDS is explained later)
   - Put the 'commands-out.log' into the root save folder, and rename
      it to 'commands.log'. Strip everything and including the "newgame"
      entry from the log.
   - Run 'openttd -D -d desync=0 -g startsavegame.sav'.
     This replays the server log. Use "-d desync=3" to also create a
     new 'commands-out.log' and 'dmp_cmds_*.sav' in your autosave folder.

## 3.2) Evaluation of the replay

  The replaying will also compare the checksums which are part of
  the 'commands-out.log' with the replayed gamestate.
  If they differ, it will trigger a 'NOT_REACHED'.

  If the replay succeeds without mismatch, that is the replay reproduces
  the original server state:
   - Repeat the replay starting from incrementally later 'dmp_cmds_*.sav'
     while truncating the 'commands.log' at the beginning appropriately.
     The 'dmp_cmds_*.sav' can be your own ones from the first reply, or
     the ones from the original server (if you have them).
     (This simulates the view of joining clients during the game.)
   - If one of those replays fails, you have located the Desync between
     the last dmp_cmds that reproduces the replay and the first one
     that fails.

  If the replay does not succeed without mismatch, you can check the logs
  whether there were failed commands. Then you may try to replay with
  DEBUG_FAILED_DUMP_COMMANDS enabled. If the replay then fails, the
  command test-run of the failed command modified the game state.

  If you have the original 'dmp_cmds_*.sav', you can also compare those
  savegames with your own ones from the replay. You can also comment/disable
  the 'NOT_REACHED' mentioned above, to get another 'dmp_cmds_*.sav' from
  the replay after the mismatch has already been detected.
  See Section 3.3 on how to compare savegames.
  If the saves differ you have located the Desync between the last dmp_cmds
  that match and the first one that does not. The difference of the saves
  may point you in the direction of what causes it.

  If the replay succeeds without mismatch, and you do not have any
  'dmp_cmd_*.sav' from the original server, it is a lost case.
  Enable creation of the 'dmp_cmd_*.sav' on the server, and wait for the
  next Desync.

  Finally, you can also compare the 'commands-out.log' from the original
  server with the one from the replay. They will differ in stuff like
  dates, and the original log will contain the chat, but otherwise they
  should match.

## 3.3) Comparing savegames

  The binary form of the savegames from the original server and from
  your replay will always differ:
   - The savegame contains paths to used NewGRF files.
   - The gamelog will log your loading of the savegame.
   - The savegame data of AIs and the Gamescript will differ.
     Scripts are not run during the replay, only their recorded commands
     are replayed. Their internal state will thus not change in the
     replay and will differ.

  To compare savegame more semantically, easiest is to first export them
  to a JSON format with for example:

  https://github.com/TrueBrain/OpenTTD-savegame-reader

  By running:

  python -m savegame_reader --export-json dmp_cmds_NNN.sav | jq . > NNN.json

  Now you can use any (JSON) diff tool to compare the two savegames in a
  somewhat human readable way.
