# Add source group filters for use in generated projects.

source_group("AI Core" REGULAR_EXPRESSION "src/ai/")
source_group("Blitters" REGULAR_EXPRESSION "src/blitter/")
source_group("Core Source Code" REGULAR_EXPRESSION "src/core/")
source_group("Game Core" REGULAR_EXPRESSION "src/game/")
source_group("MD5" REGULAR_EXPRESSION "src/3rdparty/md5/")
source_group("Misc" REGULAR_EXPRESSION "src/misc/")
source_group("Music" REGULAR_EXPRESSION "src/music/")
source_group("Network Core" REGULAR_EXPRESSION "src/network/core/")
source_group("OSX" REGULAR_EXPRESSION "src/os/macosx/")
source_group("Pathfinder" REGULAR_EXPRESSION "src/pathfinder/")
source_group("Save/Load handlers" REGULAR_EXPRESSION "src/saveload/")
source_group("Sound" REGULAR_EXPRESSION "src/sound/")
source_group("Sprite loaders" REGULAR_EXPRESSION "src/spriteloader/")
source_group("Squirrel" REGULAR_EXPRESSION "src/3rdparty/squirrel/squirrel/")
source_group("Tables" REGULAR_EXPRESSION "src/table/")
source_group("Video" REGULAR_EXPRESSION "src/video/")
source_group("Video/GL" REGULAR_EXPRESSION "src/3rdparty/opengl/")
source_group("Widgets" REGULAR_EXPRESSION "src/widgets/")
source_group("Windows files" REGULAR_EXPRESSION "src/os/windows/|\.rc$")

# Last directive for each file wins, so make sure NPF/YAPF are after the generic pathfinder filter.
source_group("NPF" REGULAR_EXPRESSION "src/pathfinder/npf/")
source_group("YAPF" REGULAR_EXPRESSION "src/pathfinder/yapf/")

source_group("Script" REGULAR_EXPRESSION "src/script/")
source_group("Script API Implementation" REGULAR_EXPRESSION "src/script/api/")
source_group("Script API" REGULAR_EXPRESSION "src/script/api/.*\.hpp$")
source_group("AI API" REGULAR_EXPRESSION "src/script/api/ai_")
source_group("Game API" REGULAR_EXPRESSION "src/script/api/game_")

# Placed last to ensure any of the previous directory filters are overridden.
source_group("Command handlers" REGULAR_EXPRESSION "_cmd\.cpp$")
source_group("Drivers" REGULAR_EXPRESSION "_driver\.hpp$")
source_group("GUI Source Code" REGULAR_EXPRESSION "_gui\.cpp$")
source_group("Map Accessors" REGULAR_EXPRESSION "_map\.(cpp|h)$")
source_group("NewGRF" REGULAR_EXPRESSION "newgrf.*cpp$")
source_group("Squirrel headers" REGULAR_EXPRESSION "src/3rdparty/squirrel/squirrel/.*\.h$")
