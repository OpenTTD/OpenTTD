# Compiling OpenTTD

## Required/optional libraries

The following libraries are used by OpenTTD for:

- zlib: (de)compressing of old (0.3.0-1.0.5) savegames, content downloads,
   heightmaps
- liblzo2: (de)compressing of old (pre 0.3.0) savegames
- liblzma: (de)compressing of savegames (1.1.0 and later)
- libpng: making screenshots and loading heightmaps
- libfreetype: loading generic fonts and rendering them
- libfontconfig: searching for fonts, resolving font names to actual fonts
- libicu: handling of right-to-left scripts (e.g. Arabic and Persian) and
   natural sorting of strings (Linux only)
- libSDL2: hardware access (video, sound, mouse) (not required for Windows or macOS)

OpenTTD does not require any of the libraries to be present, but without
liblzma you cannot open most recent savegames and without zlib you cannot
open most older savegames or use the content downloading system.
Without libSDL/liballegro on non-Windows and non-macOS machines you have
no graphical user interface; you would be building a dedicated server.

## Windows:

You need Microsoft Visual Studio 2015 Update 3 or newer.

You can download the free Visual Studio Community Edition from Microsoft at
https://visualstudio.microsoft.com/vs/community/.

OpenTTD needs the Platform SDK, if it isn't installed already. This can be
done during installing Visual Studio, by selecting
`Visual C++ MFC for x86 and x64` (and possibly
`Visual C++ ATL for x86 and x64` depending on your version). If not, you
can get download it as [MS Windows Platform SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk).

Install the SDK by following the instructions as given.

Dependencies for OpenTTD on Windows are handled via
[vcpkg](https://github.com/Microsoft/vcpkg/). First you need to install vcpkg
by following the `Quick Start` instructions of their
[README](https://github.com/Microsoft/vcpkg/blob/master/README.md).

After this, you can install the dependencies OpenTTD needs. We advise to use
the `static` versions, and OpenTTD currently needs the following dependencies:

- liblzma
- libpng
- lzo
- zlib

To install both the x64 (64bit) and x86 (32bit) variants (though only one is necessary), you can use:

```ps
.\vcpkg install liblzma:x64-windows-static libpng:x64-windows-static lzo:x64-windows-static zlib:x64-windows-static
.\vcpkg install liblzma:x86-windows-static libpng:x86-windows-static lzo:x86-windows-static zlib:x86-windows-static
```

Open the relevant project file and it should build automatically.
- VS 2015: projects/openttd_vs140.sln
- VS 2017: projects/openttd_vs141.sln
- VS 2019: projects/openttd_vs142.sln

Set the build mode to `Release` in
`Build > Configuration manager > Active solution configuration`.
You can now compile.

If everything works well the binary should be in `objs\Win[32|64]\Release\openttd.exe`
and in `bin\openttd.exe`

The OpenTTD wiki may provide additional help with [compiling for Windows](https://wiki.openttd.org/Compiling_on_Windows_using_Microsoft_Visual_C%2B%2B_2015).

You can also build OpenTTD with MSYS2/MinGW-w64 or Cygwin/MinGW using the Makefile.  The OpenTTD wiki may provide additional help with [MSYS2](https://wiki.openttd.org/Compiling_on_Windows_using_MSYS2)

## Linux, Unix, Solaris:

OpenTTD can be built with GNU '`make`'. On non-GNU systems it is called '`gmake`'.
However, for the first build one has to do a '`./configure`' first.

The OpenTTD wiki may provide additional help with:

- [compiling for Linux and *BSD](https://wiki.openttd.org/Compiling_on_%28GNU/%29Linux_and_*BSD)
- [compiling for Solaris](https://wiki.openttd.org/Compiling_on_Solaris)


## macOS:

Use '`make`' or Xcode (which will then call make for you)
This will give you a binary for your CPU type (PPC/Intel)
However, for the first build one has to do a '`./configure`' first.
To make a universal binary type '`./configure --enable-universal`'
instead of '`./configure`'.

The OpenTTD wiki may provide additional help with [compiling for macOS](https://wiki.openttd.org/Compiling_on_Mac_OS_X).

## Haiku:

Use '`make`', but do a '`./configure`' before the first build.

The OpenTTD wiki may provide additional help with [compiling for Haiku](https://wiki.openttd.org/Compiling_on_Haiku).

## OS/2:

A comprehensive GNU build environment is required to build the OS/2 version.

The OpenTTD wiki may provide additional help with [compiling for OS/2](https://wiki.openttd.org/Compiling_on_OS/2).

## Supported compilers

The following compilers are tested with and known to compile OpenTTD:

- Microsoft Visual C++ (MSVC) 2015, 2017 and 2019.
- GNU Compiler Collection (GCC) 4.8 - 9.
- Clang/LLVM 3.9 - 8

The following compilers are known not to compile OpenTTD:

In general, this is because these old versions do not (fully) support modern
C++11 language features.

- Microsoft Visual C++ (MSVC) 2013 and earlier.
- GNU Compiler Collection (GCC) 4.7 and earlier.
- Clang/LLVM 3.8 and earlier.

If any of these, or any other, compilers can compile OpenTTD, let us know.
Pull requests to support more compilers are welcome.

## Compilation of base sets

To recompile the extra graphics needed to play with the original Transport
Tycoon Deluxe graphics you need GRFCodec (which includes NFORenum) as well.
GRFCodec can be found at https://www.openttd.org/download-grfcodec.
The compilation of these extra graphics does generally not happen, unless
you remove the graphics file using '`make maintainer-clean`'.

Re-compilation of the base sets, thus also use of '`--maintainer-clean`' can
leave the repository in a modified state as different grfcodec versions can
cause binary differences in the resulting grf. Also translations might have
been added for the base sets which are not yet included in the base set
information files. Use the configure option '`--without-grfcodec`' to avoid
modification of the base set files by the build process.
