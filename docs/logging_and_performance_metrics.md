# Logging, frame rate and performance metrics

## 1.0) Logging of potentially dangerous actions

OpenTTD is a complex program, and together with NewGRF, it may show a buggy
behaviour. But not only bugs in code can cause problems. There are several
ways to affect game state possibly resulting in program crash or multiplayer
desyncs.

Easier way would be to forbid all these unsafe actions, but that would affect
game usability for many players. We certainly do not want that.
However, we receive bug reports because of this. To reduce time spent with
solving these problems, these potentially unsafe actions are logged in
the savegame (including crash.sav). Log is stored in crash logs, too.

Information logged:

- Adding / removing / changing order of NewGRFs
- Changing NewGRF parameters, loading compatible NewGRF
- Changing game mode (scenario editor <-> normal game)
- Loading game saved in a different OpenTTD / TTDPatch / Transport Tycoon Deluxe /
   original Transport Tycoon version
- Running a modified OpenTTD build
- Changing settings affecting NewGRF behaviour (non-network-safe settings)
- Triggering NewGRF bugs

No personal information is stored.

You can show the game log by typing 'gamelog' in the console or by running
OpenTTD in debug mode.

## 2.0) Frame rate and performance metrics

The Help menu in-game has a function to open the Frame rate window. This
window shows various real-time performance statistics, measuring what parts
of the game require the most processing power currently.

A summary of the statistics can also be retrieved from the console with the
`fps` command. This is especially useful on dedicated servers, where the
administrator might want to determine what's limiting performance in a slow
game.

The frame rate is given as two figures, the simulation rate and the graphics
frame rate. Usually these are identical, as the screen is rendered exactly
once per simulated tick, but in the future there might be support for graphics
and simulation running at different rates. When the game is paused, the
simulation rate drops to zero.

In addition to the simulation rate, a game speed factor is also calculated.
This is based on the target simulation speed, which is 30 milliseconds per
game tick. At that speed, the expected frame rate is 33.33 frames/second, and
the game speed factor is how close to that target the actual rate is. When
the game is in fast forward mode, the game speed factor shows how much
speed up is achieved.

The lower part of the window shows timing statistics for individual parts of
the game. The times shown are short-term and long-term averages of how long
it takes to process one tick of game time, all figures are in milliseconds.

Clicking a line in the lower part of the window opens a graph window, giving
detailed readings on each tick simulated by the game.

The following is an explanation of the different statistics:

- *Game loop* - Total processing time used per simulated "tick" in the game.
  This includes all pathfinding, world updates, and economy handling.
- *Cargo handling* - Time spent loading/unloading cargo at stations, and
  industries and towns sending/retrieving cargo from stations.
- *Train ticks*, *Road vehicle ticks*, *Ship ticks*, *Aircraft ticks* -
  Time spent on pathfinding and other processing for each player vehicle type.
- *World ticks* - Time spent on other world/landscape processing. This
  includes towns growing, building animations, updates of farmland and trees,
  and station rating updates.
- *GS/AI total*, *Game script*, and *AI players* - Time spent running logic
  for game scripts and AI players. The total may show as less than the current
  sum of the individual scripts, this is because AI players at lower
  difficulty settings do not run every game tick, and hence contribute less
  to the average across all ticks. Keep in mind that the "Current" figure is
  also an average, just only over short term.
- *Link graph delay* - Time overruns of the cargo distribution link graph
  update thread. Usually the link graph is updated in a background thread,
  but these updates need to synchronise with the main game loop occasionally,
  if the time spent on link graph updates is longer than the time taken to
  otherwise simulate the game while it was updating, these delays are counted
  in this figure.
- *Graphics rendering* - Total time spent rendering all graphics, including
  both GUI and world viewports. This typically spikes when panning the view
  around, and when more things are happening on screen at once.
- *World viewport rendering* - Isolated time spent rendering just world
  viewports. If this figure is significantly lower than the total graphics
  rendering time, most time is spent rendering GUI than rendering world.
- *Video output* - Speed of copying the rendered graphics to the display
  adapter. Usually this should be very fast (in the range of 0-3 ms), large
  values for this can indicate a graphics driver problem.
- *Sound mixing* - Speed of mixing active audio samples together. Usually
  this should be very fast (in the range of 0-3 ms), if it is slow, consider
  switching to the NoSound set.

If the frame rate window is shaded, the title bar will instead show just the
current simulation rate and the game speed factor.

## 3.0) NewGRF callback profiling

NewGRF developers can profile callback chains via the `newgrf_profile`
console command. The command controls a profiling mode where every sprite
request is measured and logged, and written to a CSV file in the end.

The NewGRF developer tools need to be enabled for the command to function.

View the syntax for the command in-game with the console command
`help newgrf_profile`.

Profiling only works during game or in the editor, it's not possible to
profile across the main menu, world generation, or loading savegames.

The CSV files contain one line per sprite request during the profiling.
They can get very large, especially on large games with many objects from
the GRF. Start profiling short periods such as 3 or 7 days, and watch the
file sizes.

The produced CSV file contains the following fields:

- *Tick* - Game tick counter, this may wrap to zero during recording.
  Mainly useful to distinguish events from separate ticks.
- *Sprite* - Index of the root Action 2 sprite in the GRF file. This is
  the sprite group being resolved.
- *Feature* - NewGRF feature number the sprite group is being resolved for.
  This will be 0xFF for AI purchase selection and ambient sound callbacks.
- *Item* - The id of the item within the GRF. For cargotypes, railtypes,
  roadtypes, and tramtypes, this is the integer representation of the label.
- *CallbackID* - The type of callback being resolved. ID 0 is regular graphics
  lookup. See the `newgrf_callbacks.h` file in the OpenTTD source code for the
  full list of callback IDs.
- *Microseconds* - Total time spent to resolve the Action 2, in microseconds.
- *Depth* - Number of recursive Action 2 lookups were made during resolution.
  Value zero means the sprite group resolved directly.
- *Result* - Result of the callback resolution. For lookups that result in
  a sprite, this is the index of the base action 2 in the GRF file. For
  callbacks that give a numeric result, this is the callback result value.
  For lookups that result in an industry production or tilelayout, this
  is the sprite index of the action 2 defining the production/tilelayout.
