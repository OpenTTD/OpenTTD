# Microsoft Developer Studio Project File - Name="ttd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ttd - Win32 Checked
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ttd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ttd.mak" CFG="ttd - Win32 Checked"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ttd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ttd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "ttd - Win32 Checked" (based on "Win32 (x86) Console Application")
!MESSAGE "ttd - Win32 Release with PNG" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ttd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /Gd /Zp4 /W3 /Zi /Ox /Oa /Ow /Og /Oi /Os /Gf /Gy /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32_EXCEPTION_TRACKER" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /FAcs /FR /Yu"stdafx.h" /J /FD /c
# SUBTRACT CPP /WX /Ot
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib /nologo /subsystem:windows /map /machine:I386 /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ttd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "WITH_SDL" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /D "WITH_PNG" /D "WITH_ZLIB" /D "ENABLE_NETWORK" /YX"stdafx.h" /FD /GZ /c
# SUBTRACT CPP /WX /Fr
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib ws2_32.lib libpng.lib zlibstat.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ttd - Win32 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ttd___Win32_Checked"
# PROP BASE Intermediate_Dir "ttd___Win32_Checked"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Checked"
# PROP Intermediate_Dir "Checked"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "WITH_SDL" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /YX"stdafx.h" /FD /GZ /c
# SUBTRACT BASE CPP /WX /Fr
# ADD CPP /nologo /Gd /Zp4 /ML /W3 /Gm /Zi /Ox /Oa /Ow /Og /Oi /Os /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "WITH_SDL" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /YX"stdafx.h" /FD /c
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none /incremental:no

!ELSEIF  "$(CFG)" == "ttd - Win32 Release with PNG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ttd___Win32_Release_with_PNG"
# PROP BASE Intermediate_Dir "ttd___Win32_Release_with_PNG"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleasePNG"
# PROP Intermediate_Dir "ReleasePNG"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gd /Zp4 /W3 /Zi /Ox /Oa /Ow /Og /Oi /Os /Gf /Gy /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32_EXCEPTION_TRACKER" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /FAcs /FR /Yu"stdafx.h" /J /FD /c
# SUBTRACT BASE CPP /WX /Ot
# ADD CPP /nologo /Gd /Zp4 /W3 /Zi /Ox /Oa /Ow /Og /Oi /Os /Gf /Gy /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "WIN32_EXCEPTION_TRACKER" /D "WIN32_ENABLE_DIRECTMUSIC_SUPPORT" /D "WITH_PNG" /D "WITH_ZLIB" /D "ENABLE_NETWORK" /FAcs /FR /Yu"stdafx.h" /J /FD /c
# SUBTRACT CPP /WX /Ot
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib libpng.lib zlibstat.lib /nologo /subsystem:windows /map /machine:I386 /opt:nowin98
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winmm.lib libpng.lib zlibstat.lib /nologo /subsystem:windows /map /machine:I386 /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "ttd - Win32 Release"
# Name "ttd - Win32 Debug"
# Name "ttd - Win32 Checked"
# Name "ttd - Win32 Release with PNG"

# Begin Group "Source Files"
# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"

# Begin Source File
SOURCE=.\ai.c
# End Source File

# Begin Source File
SOURCE=.\ai_build.c
# End Source File

# Begin Source File
SOURCE=.\ai_new.c
# End Source File

# Begin Source File
SOURCE=.\ai_pathfinder.c
# End Source File

# Begin Source File
SOURCE=.\ai_shared.c
# End Source File

# Begin Source File
SOURCE=.\airport.c
# End Source File

# Begin Source File
SOURCE=.\aystar.c
# End Source File

# Begin Source File
SOURCE=.\command.c
# End Source File

# Begin Source File
SOURCE=.\console.c
# End Source File

# Begin Source File
SOURCE=.\console_cmds.c
# End Source File

# Begin Source File
SOURCE=.\debug.c
# End Source File

# Begin Source File
SOURCE=.\depot.c
# End Source File

# Begin Source File
SOURCE=.\documentation.txt
# End Source File

# Begin Source File
SOURCE=.\economy.c
# End Source File

# Begin Source File
SOURCE=.\engine.c
# End Source File

# Begin Source File
SOURCE=.\fileio.c
# End Source File

# Begin Source File
SOURCE=.\gfx.c
# End Source File

# Begin Source File
SOURCE=.\landscape.c
# End Source File

# Begin Source File
SOURCE=.\map.c
# End Source File

# Begin Source File
SOURCE=.\md5.c
# End Source File

# Begin Source File
SOURCE=.\minilzo.c

!IF  "$(CFG)" == "ttd - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ttd - Win32 Debug"

# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "ttd - Win32 Checked"

# SUBTRACT BASE CPP /YX
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "ttd - Win32 Release with PNG"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File

# Begin Source File
SOURCE=.\misc.c
# End Source File

# Begin Source File
SOURCE=.\mixer.c
# End Source File

# Begin Source File
SOURCE=.\namegen.c
# End Source File

# Begin Source File
SOURCE=.\network.c
# End Source File

# Begin Source File
SOURCE=.\newgrf.c
# End Source File

# Begin Source File
SOURCE=.\npf.c
# End Source File

# Begin Source File
SOURCE=.\oldloader.c
# End Source File

# Begin Source File
SOURCE=.\pathfind.c
# End Source File

# Begin Source File
SOURCE=.\pool.c
# End Source File

# Begin Source File
SOURCE=.\players.c
# End Source File

# Begin Source File
SOURCE=.\queue.c
# End Source File

# Begin Source File
SOURCE=.\saveload.c
# End Source File

# Begin Source File
SOURCE=.\screenshot.c
# End Source File

# Begin Source File
SOURCE=.\sdl.c
# End Source File

# Begin Source File
SOURCE=.\settings.c
# End Source File

# Begin Source File
SOURCE=.\signs.c
# End Source File

# Begin Source File
SOURCE=.\sound.c
# End Source File

# Begin Source File
SOURCE=.\sprite.c
# End Source File

# Begin Source File
SOURCE=.\spritecache.c
# End Source File

# Begin Source File

SOURCE=.\StdAfx.c

!IF  "$(CFG)" == "ttd - Win32 Release"

# ADD CPP /Yc"stdafx.h"

!ELSEIF  "$(CFG)" == "ttd - Win32 Debug"

!ELSEIF  "$(CFG)" == "ttd - Win32 Checked"

!ELSEIF  "$(CFG)" == "ttd - Win32 Release with PNG"

# ADD BASE CPP /Yc"stdafx.h"
# ADD CPP /Yc"stdafx.h"

!ENDIF 

# End Source File

# Begin Source File
SOURCE=.\string.c
# End Source File

# Begin Source File
SOURCE=.\strings.c
# End Source File

# Begin Source File
SOURCE=.\texteff.c
# End Source File

# Begin Source File
SOURCE=.\tile.c
# End Source File

# Begin Source File
SOURCE=.\ttd.c
# End Source File

# Begin Source File
SOURCE=.\ttd.rc
# End Source File

# Begin Source File

SOURCE=.\unix.c

!IF  "$(CFG)" == "ttd - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ttd - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ttd - Win32 Checked"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ttd - Win32 Release with PNG"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File

# Begin Source File
SOURCE=.\vehicle.c
# End Source File

# Begin Source File
SOURCE=.\vehicle_gui.c
# End Source File

# Begin Source File
SOURCE=.\viewport.c
# End Source File

# Begin Source File
SOURCE=.\w32dm.c
# End Source File

# Begin Source File

SOURCE=.\w32dm2.cpp

!IF  "$(CFG)" == "ttd - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ttd - Win32 Debug"

!ELSEIF  "$(CFG)" == "ttd - Win32 Checked"

!ELSEIF  "$(CFG)" == "ttd - Win32 Release with PNG"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File

# Begin Source File
SOURCE=.\widget.c
# End Source File

# Begin Source File
SOURCE=.\win32.c
# End Source File

# Begin Source File
SOURCE=.\window.c
# End Source File

# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"

# Begin Source File
SOURCE=.\ai.h
# End Source File

# Begin Source File
SOURCE=.\aystar.h
# End Source File

# Begin Source File
SOURCE=.\command.h
# End Source File

# Begin Source File
SOURCE=.\console.h
# End Source File

# Begin Source File
SOURCE=.\debug.h
# End Source File

# Begin Source File
SOURCE=.\depot.h
# End Source File

# Begin Source File
SOURCE=.\economy.h
# End Source File

# Begin Source File
SOURCE=.\engine.h
# End Source File

# Begin Source File
SOURCE=.\fileio.h
# End Source File

# Begin Source File
SOURCE=.\functions.h
# End Source File

# Begin Source File
SOURCE=.\gfx.h
# End Source File

# Begin Source File
SOURCE=.\gui.h
# End Source File

# Begin Source File
SOURCE=.\hal.h
# End Source File

# Begin Source File
SOURCE=.\industry.h
# End Source File

# Begin Source File
SOURCE=.\macros.h
# End Source File

# Begin Source File
SOURCE=.\map.h
# End Source File

# Begin Source File
SOURCE=.\md5.h
# End Source File

# Begin Source File
SOURCE=.\network.h
# End Source File

# Begin Source File
SOURCE=.\news.h
# End Source File

# Begin Source File
SOURCE=.\npf.h
# End Source File

# Begin Source File
SOURCE=.\pathfind.h
# End Source File

# Begin Source File
SOURCE=.\pool.h
# End Source File

# Begin Source File
SOURCE=.\player.h
# End Source File

# Begin Source File
SOURCE=.\queue.h
# End Source File

# Begin Source File
SOURCE=.\saveload.h
# End Source File

# Begin Source File
SOURCE=.\signs.h
# End Source File

# Begin Source File
SOURCE=.\sound.h
# End Source File

# Begin Source File
SOURCE=.\station.h
# End Source File

# Begin Source File
SOURCE=.\string.h
# End Source File

# Begin Source File
SOURCE=.\strings.h
# End Source File

# Begin Source File
SOURCE=.\StdAfx.h
# End Source File

# Begin Source File
SOURCE=.\tile.h
# End Source File

# Begin Source File
SOURCE=.\town.h
# End Source File

# Begin Source File
SOURCE=.\ttd.h
# End Source File

# Begin Source File
SOURCE=.\variables.h
# End Source File

# Begin Source File
SOURCE=.\vehicle.h
# End Source File

# Begin Source File
SOURCE=.\vehicle_gui.h
# End Source File

# Begin Source File
SOURCE=.\viewport.h
# End Source File

# Begin Source File
SOURCE=.\window.h
# End Source File

# End Group
# Begin Group "Gui Source codes"

# PROP Default_Filter ""

# Begin Source File
SOURCE=.\aircraft_gui.c
# End Source File

# Begin Source File
SOURCE=.\airport_gui.c
# End Source File

# Begin Source File
SOURCE=.\bridge_gui.c
# End Source File

# Begin Source File
SOURCE=.\dock_gui.c
# End Source File

# Begin Source File
SOURCE=.\engine_gui.c
# End Source File

# Begin Source File
SOURCE=.\graph_gui.c
# End Source File

# Begin Source File
SOURCE=.\industry_gui.c
# End Source File

# Begin Source File
SOURCE=.\intro_gui.c
# End Source File

# Begin Source File
SOURCE=.\main_gui.c
# End Source File

# Begin Source File
SOURCE=.\misc_gui.c
# End Source File

# Begin Source File
SOURCE=.\music_gui.c
# End Source File

# Begin Source File
SOURCE=.\network_gui.c
# End Source File

# Begin Source File
SOURCE=.\news_gui.c
# End Source File

# Begin Source File
SOURCE=.\order_gui.c
# End Source File

# Begin Source File
SOURCE=.\player_gui.c
# End Source File

# Begin Source File
SOURCE=.\rail_gui.c
# End Source File

# Begin Source File
SOURCE=.\road_gui.c
# End Source File

# Begin Source File
SOURCE=.\roadveh_gui.c
# End Source File

# Begin Source File
SOURCE=.\settings_gui.c
# End Source File

# Begin Source File
SOURCE=.\ship_gui.c
# End Source File

# Begin Source File
SOURCE=.\smallmap_gui.c
# End Source File

# Begin Source File
SOURCE=.\station_gui.c
# End Source File

# Begin Source File
SOURCE=.\subsidy_gui.c
# End Source File

# Begin Source File
SOURCE=.\terraform_gui.c
# End Source File

# Begin Source File
SOURCE=.\town_gui.c
# End Source File

# Begin Source File
SOURCE=.\train_gui.c
# End Source File

# End Group
# Begin Group "Landscape"

# PROP Default_Filter ""

# Begin Source File
SOURCE=.\aircraft_cmd.c
# End Source File

# Begin Source File
SOURCE=.\clear_cmd.c
# End Source File

# Begin Source File
SOURCE=.\disaster_cmd.c
# End Source File

# Begin Source File
SOURCE=.\dummy_land.c
# End Source File

# Begin Source File
SOURCE=.\industry_cmd.c
# End Source File

# Begin Source File
SOURCE=.\misc_cmd.c
# End Source File

# Begin Source File
SOURCE=.\order_cmd.c
# End Source File

# Begin Source File
SOURCE=.\rail_cmd.c
# End Source File

# Begin Source File
SOURCE=.\road_cmd.c
# End Source File

# Begin Source File
SOURCE=.\roadveh_cmd.c
# End Source File

# Begin Source File
SOURCE=.\ship_cmd.c
# End Source File

# Begin Source File
SOURCE=.\station_cmd.c
# End Source File

# Begin Source File
SOURCE=.\town_cmd.c
# End Source File

# Begin Source File
SOURCE=.\train_cmd.c
# End Source File

# Begin Source File
SOURCE=.\tree_cmd.c
# End Source File

# Begin Source File
SOURCE=.\tunnelbridge_cmd.c
# End Source File

# Begin Source File
SOURCE=.\unmovable_cmd.c
# End Source File

# Begin Source File
SOURCE=.\water_cmd.c
# End Source File

# End Group
# Begin Group "Tables"

# PROP Default_Filter ""

# Begin Source File
SOURCE=.\table\ai_rail.h
# End Source File

# Begin Source File
SOURCE=.\table\allstrings.h
# End Source File

# Begin Source File
SOURCE=.\table\animcursors.h
# End Source File

# Begin Source File
SOURCE=.\table\autorail.h
# End Source File

# Begin Source File
SOURCE=.\table\build_industry.h
# End Source File

# Begin Source File
SOURCE=.\table\clear_land.h
# End Source File

# Begin Source File
SOURCE=.\table\engines.h
# End Source File

# Begin Source File
SOURCE=.\table\files.h
# End Source File

# Begin Source File
SOURCE=.\table\genland.h
# End Source File

# Begin Source File
SOURCE=.\table\industry_land.h
# End Source File

# Begin Source File
SOURCE=.\table\landscape_const.h
# End Source File

# Begin Source File
SOURCE=.\table\landscape_sprite.h
# End Source File

# Begin Source File
SOURCE=.\table\namegen.h
# End Source File

# Begin Source File
SOURCE=.\table\palettes.h
# End Source File

# Begin Source File
SOURCE=.\table\road_land.h
# End Source File

# Begin Source File
SOURCE=.\table\roadveh.h
# End Source File

# Begin Source File
SOURCE=.\table\sprites.h
# End Source File

# Begin Source File
SOURCE=.\table\station_land.h
# End Source File

# Begin Source File
SOURCE=.\table\strings.h
# End Source File

# Begin Source File
SOURCE=.\table\town_land.h
# End Source File

# Begin Source File
SOURCE=.\table\track_land.h
# End Source File

# Begin Source File
SOURCE=.\table\train_cmd.h
# End Source File

# Begin Source File
SOURCE=.\table\tree_land.h
# End Source File

# Begin Source File
SOURCE=.\table\tunnel_land.h
# End Source File

# Begin Source File
SOURCE=.\table\unmovable_land.h
# End Source File

# Begin Source File
SOURCE=.\table\water_land.h
# End Source File

# End Group
# Begin Group "Network Source Files"

# PROP Default_Filter ""

# Begin Source File
SOURCE=.\callback_table.c
# End Source File

# Begin Source File
SOURCE=.\dedicated.c
# End Source File

# Begin Source File
SOURCE=.\network_client.c
# End Source File

# Begin Source File
SOURCE=.\network_data.c
# End Source File

# Begin Source File
SOURCE=.\network_gamelist.c
# End Source File

# Begin Source File
SOURCE=.\network_server.c
# End Source File

# Begin Source File
SOURCE=.\network_udp.c
# End Source File

# End Group
# Begin Group "Network Header Files"

# PROP Default_Filter ""

# Begin Source File
SOURCE=.\network_client.h
# End Source File

# Begin Source File
SOURCE=.\network_core.h
# End Source File

# Begin Source File
SOURCE=.\network_data.h
# End Source File

# Begin Source File
SOURCE=.\network_gamelist.h
# End Source File

# Begin Source File
SOURCE=.\network_server.h
# End Source File

# Begin Source File
SOURCE=.\network_udp.h
# End Source File

# End Group

# Begin Source File
SOURCE=.\changelog.txt
# End Source File

# Begin Source File
SOURCE=.\openttd.ico
# End Source File

# Begin Source File
SOURCE=.\mainicon.ico
# End Source File

# Begin Source File
SOURCE=.\ReadMe.txt
# End Source File

# End Target
# End Project
