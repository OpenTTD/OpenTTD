@echo off
echo Running SVN version detection script...
rem
rem Requires subversion (`svnversion'), GNU make and some other GNU tools (eg, textutils)
rem installed - a hack, I know, but it seems to work if you have the appropriate tools
rem installed.
rem
cd ..\..
if not exist .svn goto nosvn
make -f os/os2/svn_version.mak
if not %ERRORLEVEL%==0 goto nomake

goto end

:nomake
gmake -f os/os2/svn_version.mak
if not %ERRORLEVEL%==0 goto nomake2
goto end

:nomake2
echo Neither `make` nor `gmake' could be found, SVN version detection unable to
echo run. Default rev.c used...
:nosvn
echo const char _openttd_revision[] = "norev000"; > rev.c
echo const int _revision_number = 0; >> rev.c
echo #ifdef __MORPHOS__ >> rev.c
echo const char morphos_versions_tag[] = "\\0$VER: OpenTTD norev000 (00.00.00) © OpenTTD Team [MorphOS, PowerPC]"; >> rev.c
echo #endif >> rev.c
goto end

:end
cd os\os2
rem end
