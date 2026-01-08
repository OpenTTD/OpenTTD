# OpenTTD Libretro Core

This directory contains the implementation for running OpenTTD as a libretro core, allowing it to be played through RetroArch or other libretro-compatible frontends.

## Building

To build OpenTTD as a libretro core:

```bash
mkdir build && cd build
cmake -DOPTION_LIBRETRO=ON ..
cmake --build .
```

The resulting core will be located at `build/libretro/openttd_libretro.dll` (Windows), `openttd_libretro.so` (Linux), or `openttd_libretro.dylib` (macOS).

## Features

### Video
- **Software Rendering**: 32bpp XRGB8888 software rendering
- **OpenGL Hardware Rendering**: OpenGL 2.0/3.2 Core/GLES 2.0 hardware acceleration
- Configurable resolution up to 1920x1080

### Audio
- 44.1kHz stereo audio output
- Sound effects mixed through OpenTTD's mixer
- Music playback support (requires music data files)

### Input
- **Mouse**: Full mouse support with left/right click
- **Touch**: Pointer device support for touch screens
- **Keyboard**: Full keyboard support for all OpenTTD controls
- **Gamepad**: Mapped controls for navigation and actions:
  - Left Analog: Move cursor
  - Right Analog: Scroll viewport
  - A: Left click / Select
  - B: Right click / Cancel
  - X: Open menu
  - Y: Toggle fast forward
  - L/R: Zoom out/in
  - Start: Pause
  - Select: Toggle build menu

### VFS (Virtual File System)
- Supports libretro VFS interface for portable file access
- Falls back to standard file I/O if VFS not available

## Core Options

| Option | Description | Default |
|--------|-------------|---------|
| `openttd_resolution` | Internal rendering resolution | 1280x720 |
| `openttd_renderer` | Software or OpenGL rendering | OpenGL |
| `openttd_zoom_level` | Default zoom level | 1x |
| `openttd_music_volume` | Music volume | 75% |
| `openttd_sound_volume` | Sound effects volume | 75% |
| `openttd_fast_forward_speed` | Fast forward multiplier | 4x |
| `openttd_autosave_interval` | Autosave frequency | Quarterly |

## Data Files

OpenTTD requires game data files to run. Place them in your RetroArch system directory under an `openttd` subdirectory:

```
<system_dir>/openttd/
├── baseset/
│   ├── opengfx/
│   ├── opensfx/
│   └── openmsx/
├── ai/
├── game/
└── newgrf/
```

You can download the open source base sets from:
- OpenGFX: https://www.openttd.org/downloads/opengfx-releases/latest
- OpenSFX: https://www.openttd.org/downloads/opensfx-releases/latest
- OpenMSX: https://www.openttd.org/downloads/openmsx-releases/latest

## Loading Content

The core supports loading:
- **Savegames** (`.sav` files)
- **Scenarios** (`.scn` files)

You can also start the core without content to access the main menu and create a new game.

## Architecture

The libretro port consists of several components:

- `libretro.h` - Official libretro API header
- `libretro_core.cpp/h` - Main libretro interface implementation
- `libretro_v.cpp/h` - Video driver integration
- `libretro_s.cpp/h` - Sound driver integration
- `libretro_m.cpp/h` - Music driver integration

## Known Limitations

- Save states are not currently supported (OpenTTD savegames are used instead)
- Multiplayer is not supported in libretro mode
- Some UI elements may need adjustment for smaller resolutions
- NewGRF downloading requires network access through the frontend

## License

This libretro implementation is part of OpenTTD and is licensed under the GNU General Public License version 2.
