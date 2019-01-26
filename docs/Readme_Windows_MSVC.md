# Compiling OpenTTD using Microsoft Visual C++

Last updated: 2018-12-27

## Supported MSVC compilers

OpenTTD includes projects for Visual Studio 2015 Update 3 or more recent.
You can download the free Visual Studio Community Edition from Microsoft at
https://visualstudio.microsoft.com/vs/community/.

## Required files

### Microsoft platform files

OpenTTD needs the Platform SDK, if it isn't installed already. This can be
done during installing Visual Studio, by selecting
`Visual C++ MFC for x86 and x64` (and possibly
`Visual C++ ATL for x86 and x64` depending on your version). If not, you
can get it at this location:

- [MS Windows Platform SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

Install the SDK by following the instructions as given.

### OpenTTD dependencies

Dependencies for OpenTTD on Windows are handled via
[vcpkg](https://github.com/Microsoft/vcpkg/). First you need to install vcpkg
by following the `Quick Start` intructions of their
[README](https://github.com/Microsoft/vcpkg/blob/master/README.md).

After this, you can install the dependencies OpenTTD needs. We advise to use
the `static` versions, and OpenTTD currently needs the following dependencies:

- freetype
- liblzma
- libpng
- lzo
- zlib

To install both the x64 (64bit) and x86 (32bit) variants, you can use:

```ps
.\vcpkg install freetype:x64-windows-static liblzma:x64-windows-static libpng:x64-windows-static lzo:x64-windows-static zlib:x64-windows-static
.\vcpkg install freetype:x86-windows-static liblzma:x86-windows-static libpng:x86-windows-static lzo:x86-windows-static zlib:x86-windows-static
```

## TTD Graphics files

See section 4.1 of README.md for the required 3rdparty files and how to install them.

## Compiling

Open the appropriate `sln` (Solution) file for your version of Visual Studio:

- VS 2015: projects/openttd_vs140.sln
- VS 2017: projects/openttd_vs141.sln
- VS 2019: projects/openttd_vs142.sln

Set the build mode to `Release` in
`Build > Configuration manager > Active solution configuration`.
You can now compile.

If everything works well the binary should be in `objs\Win[32|64]\Release\openttd.exe`
and in `bin\openttd.exe`

## Problems

If compilation fails, double-check that you are using the latest github
source. If it still doesn't work, check in on IRC (irc://irc.oftc.net/openttd),
to ask for help.
