# Microsoft Developer Studio Project File - Name="langs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=langs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "langs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "langs.mak" CFG="langs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "langs - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project

# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "langs___Win32_Debug"
# PROP BASE Intermediate_Dir "langs___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Target_Dir ""

# Begin Target
# Name "langs - Win32 Debug"

# Begin Source File

SOURCE=.\lang\american.txt
# Begin Custom Build
InputPath=.\lang\american.txt

"lang\american.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\american.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\catalan.txt
# Begin Custom Build
InputPath=.\lang\catalan.txt

"lang\catalan.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\catalan.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\czech.txt
# Begin Custom Build
InputPath=.\lang\czech.txt

"lang\czech.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\czech.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\danish.txt
# Begin Custom Build
InputPath=.\lang\danish.txt

"lang\danish.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\danish.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\dutch.txt
# Begin Custom Build
InputPath=.\lang\dutch.txt

"lang\dutch.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\dutch.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\english.txt
# Begin Custom Build
InputPath=.\lang\english.txt

"lang\english.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\finnish.txt
# Begin Custom Build
InputPath=.\lang\finnish.txt

"lang\finnish.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\finnish.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\french.txt
# Begin Custom Build
InputPath=.\lang\french.txt

"lang\french.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\french.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\galician.txt
# Begin Custom Build
InputPath=.\lang\galician.txt

"lang\galician.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\galician.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\german.txt
# Begin Custom Build
InputPath=.\lang\german.txt

"lang\german.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\german.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\hungarian.txt
# Begin Custom Build
InputPath=.\lang\hungarian.txt

"lang\hungarian.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\hungarian.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\icelandic.txt
# Begin Custom Build
InputPath=.\lang\icelandic.txt

"lang\icelandic.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\icelandic.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\italian.txt
# Begin Custom Build
InputPath=.\lang\italian.txt

"lang\italian.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\italian.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\norwegian.txt
# Begin Custom Build
InputPath=.\lang\norwegian.txt

"lang\norwegian.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\norwegian.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\origveh.txt
# Begin Custom Build
InputPath=.\lang\origveh.txt

"lang\origveh.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\origveh.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\polish.txt
# Begin Custom Build
InputPath=.\lang\polish.txt

"lang\polish.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\polish.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\portuguese.txt
# Begin Custom Build
InputPath=.\lang\portuguese.txt

"lang\portuguese.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\portuguese.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\romanian.txt
# Begin Custom Build
InputPath=.\lang\romanian.txt

"lang\romanian.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\romanian.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\slovak.txt
# Begin Custom Build
InputPath=.\lang\slovak.txt

"lang\slovak.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\slovak.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\spanish.txt
# Begin Custom Build
InputPath=.\lang\spanish.txt

"lang\spanish.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\spanish.txt

# End Custom Build
# End Source File

# Begin Source File
SOURCE=.\lang\swedish.txt
# Begin Custom Build
InputPath=.\lang\swedish.txt

"lang\swedish.lng" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	strgen\debug\strgen.exe lang\swedish.txt

# End Custom Build
# End Source File

# End Target
# End Project
