@echo off
echo Running SVN version detection script...
rem
rem Requires subversion (`svnversion') to be installed
rem
cd ..\..
if not "%RELEASE%"=="" goto forcerelease
if not exist .svn goto nosvn
svnversion -n . > os\os2\svnver.tmp
if not "%ERRORLEVEL%"=="0" goto nosvn

copy os\os2\svnver1.c+os\os2\svnver.tmp+os\os2\svnver2.c rev.c /a /y > nul 2> nul
goto end

:forcerelease
echo Forcing release string "%RELEASE%"...
echo const char _openttd_revision[] = "%RELEASE%"; > rev.c
echo const int _revision_number = 0; >> rev.c
goto end

:nosvn
echo Error executing `svnversion' or no SVN data detected
echo const char _openttd_revision[] = "norev000"; > rev.c
echo const int _revision_number = 0; >> rev.c
goto end

:end
cd os\os2
del svnver.tmp > nul 2> nul
rem end
