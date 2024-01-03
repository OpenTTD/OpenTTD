# OpenTTD directory structure

OpenTTD uses its own directory to store its required 3rd party base set files
(see section 4.1 'Required 3rd party files') and non-compulsory extension and
configuration files.

See below for their proper place within this OpenTTD main data directory.

The main OpenTTD directories can be found in various locations, depending on
your operating system:

1. The current working directory (from where you started OpenTTD)

    For non-Windows operating systems OpenTTD will not scan for files in this
    directory if it is your personal directory, i.e. '~/', or when it is the
    root directory, i.e. '/'.

2. Your personal directory
    - Windows:
        - `C:\My Documents\OpenTTD` (95, 98, ME)
        - `C:\Documents and Settings\<username>\My Documents\OpenTTD` (2000, XP)
        - `C:\Users\<username>\Documents\OpenTTD` (7, 8.1, 10, 11)
    - macOS: `~/Documents/OpenTTD`
    - Linux: `$XDG_DATA_HOME/openttd` which is usually `~/.local/share/openttd`
       when built with XDG base directory support, otherwise `~/.openttd`
3. The shared directory
    - Windows:
        - `C:\Documents and Settings\All Users\Shared Documents\OpenTTD` (2000, XP)
        - `C:\Users\Public\Documents\OpenTTD` (7, 8.1, 10, 11)
    - macOS: `/Library/Application Support/OpenTTD`
    - Linux: not available
4. The binary directory (where the OpenTTD executable is)
    - Windows: `C:\Program Files\OpenTTD`
    - Linux: `/usr/games`
5. The installation directory (Linux only)
    - Linux: `/usr/share/games/openttd`
6. The application bundle (macOS only)

    It includes the OpenTTD files (grf+lng) and it will work as long as they
    are not touched

Different types of data or extensions go into different subdirectories of the
chosen main OpenTTD directory:

| data type           | directory         | additional info             |
| ------------------- | ----------------- | --------------------------- |
| Config File         | (no subdirectory) |                             |
| Screenshots         | screenshot        |                             |
| Base Graphics       | baseset           | (or a subdirectory thereof) |
| Sound Sets          | baseset           | (or a subdirectory thereof) |
| NewGRFs             | newgrf            | (or a subdirectory thereof) |
| 32bpp Sets          | newgrf            | (or a subdirectory thereof) |
| Music Sets          | baseset           | (or a subdirectory thereof) |
| AIs                 | ai                | (or a subdirectory thereof) |
| AI Libraries        | ai/library        | (or a subdirectory thereof) |
| Game Scripts (GS)   | game              | (or a subdirectory thereof) |
| GS Libraries        | game/library      | (or a subdirectory thereof) |
| Savegames           | save              |                             |
| Automatic Savegames | save/autosave     |                             |
| Scenarios           | scenario          |                             |

The (automatically created) directory content_download is for OpenTTD's internal
use and no files should be added to it or its subdirectories manually.

## Notes:

- Linux in the previous list means .deb, but most paths should be similar for
   others.
- The previous search order is also used for NewGRFs and openttd.cfg.
- If openttd.cfg is not found, then it will be created using the 2, 4, 1, 3,
   5 order. When built with XDG base directory support, openttd.cfg will be
   created in $XDG_CONFIG_HOME/openttd which is usually ~/.config/openttd.
- Savegames will be relative to the config file only if there is no save/
   directory in paths with higher priority than the config file path, but
   autosaves and screenshots will always be relative to the config file.
   Unless the configuration file is in $XDG_CONFIG_HOME/openttd, then all
   other files will be saved under $XDG_DATA_HOME/openttd.

## The preferred setup:

Place 3rd party files in shared directory (or in personal directory if you do
not have write access on shared directory) and have your openttd.cfg config
file in personal directory (where the game will then also place savegames and
screenshots).

## Portable installations (portable media)

You can install OpenTTD on external media so you can take it with you, i.e.
using a USB key, or a USB HDD, etc.
Create a directory where you shall store the game in (i.e. OpenTTD/).
Copy the binary (OpenTTD.exe, OpenTTD.app, openttd, etc), baseset/ and your
openttd.cfg to this directory.
You can copy binaries for any operating system into this directory, which will
allow you to play the game on nearly any computer you can attach the external
media to.
As always - additional grf files are stored in the newgrf/ dir (for details,
again, see section 4.1).

## Files in tar (archives)

OpenTTD can read files that are in an uncompressed tar (archive), which
makes it easy to bundle files belonging to the same script, NewGRF or base
set. Music sets are the only exception as they cannot be stored in a tar
file due to being played by external applications.

OpenTTD sees each tar archive as the 'root' of its search path. This means that
having a file with the same path in two different tar files means that one
cannot be opened, after all only one file will be found first. As such it is
advisable to put an uniquely named folder in the root of the tar and put all the
content in that folder. For example, all downloaded content has a path that
concatenates the name of the content and the version, which makes the path
unique. For custom tar files it is advised to do this as well.

The normal files are also referred to by their relative path from the search
directory, this means that also normal files could hide files in a tar as
long as the relative path from the search path of the normal file is the
same as the path in the tar file. Again it is advised to have an unique path
to the normal file so they do not collide with the files from other tar
files.

## Configuration file

The configuration file for OpenTTD (openttd.cfg) is in a simple Windows-like
.INI format. It is mostly undocumented. Almost all settings can be changed
ingame by using the 'Advanced Settings' window.

When you cannot find openttd.cfg you should look in the directories as
described in this document. If you do not have an openttd.cfg OpenTTD will
create one after closing.

