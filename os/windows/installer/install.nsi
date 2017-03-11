# Version numbers to update
!define APPV_MAJOR 1
!define APPV_MINOR 8
!define APPV_MAINT 0
!define APPV_BUILD 0
!define APPV_EXTRA "-beta1"

!define APPNAME "OpenTTD"   ; Define application name
!define APPVERSION "${APPV_MAJOR}.${APPV_MINOR}.${APPV_MAINT}${APPV_EXTRA}"  ; Define application version
!define APPVERSIONINTERNAL "${APPV_MAJOR}.${APPV_MINOR}.${APPV_MAINT}.${APPV_BUILD}" ; Define application version in X.X.X.X
!define INSTALLERVERSION ${APPV_MAJOR}${APPV_MINOR}${APPV_MAINT}${APPV_BUILD}
!include ${VERSION_INCLUDE}

!define APPURLLINK "http://www.openttd.org"
!define APPNAMEANDVERSION "${APPNAME} ${APPVERSION}"

!define OPENGFX_BASE_VERSION "1.2.0"
!define OPENSFX_BASE_VERSION "0.8.0"
!define OPENMSX_BASE_VERSION "1.0.0"

!define MUI_ICON "..\..\..\media\openttd.ico"
!define MUI_UNICON "..\..\..\media\openttd.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "top.bmp"

BrandingText "OpenTTD Installer"
SetCompressor LZMA

; Version Info
Var AddWinPrePopulate
VIProductVersion "${APPVERSIONINTERNAL}"
VIAddVersionKey "ProductName" "OpenTTD ${APPBITS}-bit Installer for Windows ${EXTRA_VERSION}"
VIAddVersionKey "Comments" "Installs ${APPNAMEANDVERSION}"
VIAddVersionKey "CompanyName" "OpenTTD Developers"
VIAddVersionKey "FileDescription" "Installs ${APPNAMEANDVERSION}"
VIAddVersionKey "ProductVersion" "${APPVERSION}"
VIAddVersionKey "InternalName" "InstOpenTTD-${APPARCH}"
VIAddVersionKey "FileVersion" "${APPVERSION}-${APPARCH}"
VIAddVersionKey "LegalCopyright" " "
; Main Install settings
Name "${APPNAMEANDVERSION} ${APPBITS}-bit for Windows ${EXTRA_VERSION}"

; NOTE: Keep trailing backslash!
InstallDirRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Install Folder"
OutFile "openttd-${APPVERSION}-${APPARCH}.exe"
CRCCheck force

ShowInstDetails show
ShowUninstDetails show

RequestExecutionLevel admin

Var SHORTCUTS
Var CDDRIVE

; Modern interface settings
!include "MUI2.nsh"
!include "InstallOptions.nsh"
!include "WinVer.nsh"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE_3LINES
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\..\COPYING"

!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_COMPONENTS

;---------------------------------
; Custom page for finding TTDLX CD
Page custom SelectCDEnter SelectCDExit ": TTD folder"

!insertmacro MUI_PAGE_DIRECTORY

;Start Menu Folder Page Configuration
!define MUI_STARTMENUPAGE_DEFAULTFOLDER $SHORTCUTS
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKEY_LOCAL_MACHINE"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Shortcut Folder"

!insertmacro MUI_PAGE_STARTMENU "OpenTTD" $SHORTCUTS

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN_TEXT "Run ${APPNAMEANDVERSION} now!"
!define MUI_FINISHPAGE_RUN "$INSTDIR\openttd.exe"
!define MUI_FINISHPAGE_LINK "Visit the OpenTTD site for more information"
!define MUI_FINISHPAGE_LINK_LOCATION "${APPURLLINK}"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\readme.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_WELCOMEFINISHPAGE_CUSTOMFUNCTION_INIT DisableBack

!insertmacro MUI_PAGE_FINISH
!define MUI_PAGE_HEADER_TEXT "Uninstall ${APPNAMEANDVERSION}"
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

;--------------------------------------------------------------
; (Core) OpenTTD install section. Copies all internal game data
Section "!OpenTTD" Section1
	; Make sure to be upgraded OpenTTD is not running
	Call CheckOpenTTDRunning

	; Overwrite files by default, but don't complain on failure
	SetOverwrite try

	SetShellVarContext all

	; Define root variable relative to installer
	!define PATH_ROOT "..\..\..\"

	; Copy language files
	SetOutPath "$INSTDIR\lang\"
	File ${PATH_ROOT}bin\lang\english.lng

	; Copy AI files
	SetOutPath "$INSTDIR\ai\"
	File ${PATH_ROOT}bin\ai\compat_*.nut

	; Copy Game Script files
	SetOutPath "$INSTDIR\game\"
	File ${PATH_ROOT}bin\game\compat_*.nut

	; Copy data files
	SetOutPath "$INSTDIR\baseset\"
	File ${PATH_ROOT}bin\baseset\*.grf
	File ${PATH_ROOT}bin\baseset\*.obg
	File ${PATH_ROOT}bin\baseset\*.obm
	File ${PATH_ROOT}bin\baseset\*.obs
	File ${PATH_ROOT}bin\baseset\opntitle.dat

	; Copy the scripts
	SetOutPath "$INSTDIR\scripts\"
	File ${PATH_ROOT}bin\scripts\*.*
	Push "$INSTDIR\scripts\readme.txt"
	Call unix2dos

	; Copy some documention files
	SetOutPath "$INSTDIR\docs\"
	File ${PATH_ROOT}docs\multiplayer.txt
	Push "$INSTDIR\docs\multiplayer.txt"
	Call unix2dos

	; Copy the rest of the stuff
	SetOutPath "$INSTDIR\"

	; Copy text files
	File ${PATH_ROOT}changelog.txt
	Push "$INSTDIR\changelog.txt"
	Call unix2dos
	File ${PATH_ROOT}COPYING
	Push "$INSTDIR\COPYING"
	Call unix2dos
	File ${PATH_ROOT}readme.txt
	Push "$INSTDIR\readme.txt"
	Call unix2dos
	File ${PATH_ROOT}known-bugs.txt
	Push "$INSTDIR\known-bugs.txt"
	Call unix2dos

	; Copy executable
	File /oname=openttd.exe ${BINARY_DIR}\openttd.exe


	; Delete old files from the main dir. they are now placed in baseset/ and lang/
	Delete "$INSTDIR\*.lng"
	Delete "$INSTDIR\*.grf"
	Delete "$INSTDIR\sample.cat"
	Delete "$INSTDIR\ttd.exe"
	Delete "$INSTDIR\data\opntitle.dat"
	Delete "$INSTDIR\data\2ccmap.grf"
	Delete "$INSTDIR\data\airports.grf"
	Delete "$INSTDIR\data\autorail.grf"
	Delete "$INSTDIR\data\canalsw.grf"
	Delete "$INSTDIR\data\dosdummy.grf"
	Delete "$INSTDIR\data\elrailsw.grf"
	Delete "$INSTDIR\data\nsignalsw.grf"
	Delete "$INSTDIR\data\openttd.grf"
	Delete "$INSTDIR\data\roadstops.grf"
	Delete "$INSTDIR\data\trkfoundw.grf"
	Delete "$INSTDIR\data\openttdd.grf"
	Delete "$INSTDIR\data\openttdw.grf"
	Delete "$INSTDIR\data\orig_win.obg"
	Delete "$INSTDIR\data\orig_dos.obg"
	Delete "$INSTDIR\data\orig_dos_de.obg"
	Delete "$INSTDIR\data\orig_win.obs"
	Delete "$INSTDIR\data\orig_dos.obs"
	Delete "$INSTDIR\data\no_sound.obs"

	; Create the Registry Entries
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Comments" "Visit ${APPURLLINK}"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "DisplayIcon" "$INSTDIR\openttd.exe,0"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "DisplayName" "OpenTTD ${APPVERSION}"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "DisplayVersion" "${APPVERSION}"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "HelpLink" "${APPURLLINK}"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Install Folder" "$INSTDIR"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Publisher" "OpenTTD"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Shortcut Folder" "$SHORTCUTS"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "UninstallString" "$INSTDIR\uninstall.exe"
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "URLInfoAbout" "${APPURLLINK}"
	; This key sets the Version DWORD that new installers will check against
	WriteRegDWORD HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Version" ${INSTALLERVERSION}

	!insertmacro MUI_STARTMENU_WRITE_BEGIN "OpenTTD"
	CreateShortCut "$DESKTOP\OpenTTD.lnk" "$INSTDIR\openttd.exe"
	CreateDirectory "$SMPROGRAMS\$SHORTCUTS"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\OpenTTD.lnk" "$INSTDIR\openttd.exe"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Uninstall.lnk" "$INSTDIR\uninstall.exe"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Readme.lnk" "$INSTDIR\Readme.txt"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Changelog.lnk" "$INSTDIR\Changelog.txt"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Known-bugs.lnk" "$INSTDIR\known-bugs.txt"
	CreateDirectory "$SMPROGRAMS\$SHORTCUTS\Docs"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Docs\Multiplayer.lnk" "$INSTDIR\docs\multiplayer.txt"
	CreateDirectory "$SMPROGRAMS\$SHORTCUTS\Scripts"
	CreateShortCut "$SMPROGRAMS\$SHORTCUTS\Scripts\Readme.lnk" "$INSTDIR\scripts\readme.txt"
	!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

;--------------------------------------------------------------
; OpenTTD translation install section. Copies only translations
Section "OpenTTD translations" Section6
	; Overwrite files by default, but don't complain on failure
	SetOverwrite try

	; Copy language files
	SetOutPath "$INSTDIR\lang\"
	File ${PATH_ROOT}bin\lang\*.lng
SectionEnd

;----------------------------------------------------------------------------------
; OpenGFX files install section. Downloads OpenGFX and installs it
Section "Download OpenGFX (free graphics set)" Section3
	SetOverwrite try

	NSISdl::download "http://binaries.openttd.org/installer/opengfx-${OPENGFX_BASE_VERSION}.7z" "$INSTDIR\baseset\opengfx.7z"
	Pop $R0 ;Get the return value
	StrCmp $R0 "success" +3
		MessageBox MB_OK "Downloading of OpenGFX failed"
		Goto Done

	; Let's extract the files
	SetOutPath "$INSTDIR\baseset\"
	NSIS7z::Extract "$INSTDIR\baseset\opengfx.7z"

	Delete "$INSTDIR\baseset\opengfx.7z"
	SetOutPath "$INSTDIR\"
Done:

SectionEnd

;----------------------------------------------------------------------------------
; OpenSFX files install section. Downloads OpenSFX and installs it
Section "Download OpenSFX (free sound set)" Section4
	SetOverwrite try

	NSISdl::download "http://binaries.openttd.org/installer/opensfx-${OPENSFX_BASE_VERSION}.7z" "$INSTDIR\baseset\opensfx.7z"
	Pop $R0 ;Get the return value
	StrCmp $R0 "success" +3
		MessageBox MB_OK "Downloading of OpenSFX failed"
		Goto Done

	; Let's extract the files
	SetOutPath "$INSTDIR\baseset\"
	NSIS7z::Extract "$INSTDIR\baseset\opensfx.7z"

	Delete "$INSTDIR\baseset\opensfx.7z"
	SetOutPath "$INSTDIR\"
Done:

SectionEnd

;----------------------------------------------------------------------------------
; OpenMSX files install section. Downloads OpenMSX and installs it
Section "Download OpenMSX (free music set)" Section5
	SetOverwrite try

	NSISdl::download "http://binaries.openttd.org/installer/openmsx-${OPENMSX_BASE_VERSION}.7z" "$INSTDIR\baseset\openmsx.7z"
	Pop $R0 ;Get the return value
	StrCmp $R0 "success" +3
		MessageBox MB_OK "Downloading of OpenMSX failed"
		Goto Done

	; Let's extract the files
	SetOutPath "$INSTDIR\baseset\"
	NSIS7z::Extract "$INSTDIR\baseset\openmsx.7z"

	Delete "$INSTDIR\baseset\openmsx.7z"
	SetOutPath "$INSTDIR\"
Done:

SectionEnd

;----------------------------------------------------------------------------------
; TTDLX files install section. Copies all needed TTDLX files from CD or install dir
Section /o "Copy data from Transport Tycoon Deluxe CD-ROM" Section2
	SetOverwrite try
	; Let's copy the files with size approximation
	SetOutPath "$INSTDIR\baseset"
	CopyFiles "$CDDRIVE\gm\*.gm" "$INSTDIR\baseset\" 1028
	CopyFiles "$CDDRIVE\sample.cat" "$INSTDIR\baseset\sample.cat" 1566
	; Copy Windows files
	CopyFiles "$CDDRIVE\trg1r.grf" "$INSTDIR\baseset\trg1r.grf" 2365
	CopyFiles "$CDDRIVE\trgcr.grf" "$INSTDIR\baseset\trgcr.grf" 260
	CopyFiles "$CDDRIVE\trghr.grf" "$INSTDIR\baseset\trghr.grf" 400
	CopyFiles "$CDDRIVE\trgir.grf" "$INSTDIR\baseset\trgir.grf" 334
	CopyFiles "$CDDRIVE\trgtr.grf" "$INSTDIR\baseset\trgtr.grf" 546
	; Copy DOS files
	CopyFiles "$CDDRIVE\trg1.grf" "$INSTDIR\baseset\trg1.grf" 2365
	CopyFiles "$CDDRIVE\trgc.grf" "$INSTDIR\baseset\trgc.grf" 260
	CopyFiles "$CDDRIVE\trgh.grf" "$INSTDIR\baseset\trgh.grf" 400
	CopyFiles "$CDDRIVE\trgi.grf" "$INSTDIR\baseset\trgi.grf" 334
	CopyFiles "$CDDRIVE\trgt.grf" "$INSTDIR\baseset\trgt.grf" 546
	SetOutPath "$INSTDIR\"
SectionEnd

;-------------------------------------------
; Install the uninstaller (option is hidden)
Section -FinishSection
	WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "Minimal OpenTTD installation in English. You need at least one of the game graphics and sound sets installed."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section6} "Translations of OpenTTD."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section3} "Download the free OpenGFX game graphics set. This download is about 3 MiB."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section4} "Download the free OpenSFX game sound set. This download is about 10 MiB."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section5} "Download the free OpenMSX game music set. This download is about 200 KiB."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} "Copies the game graphics, sounds and music from the Transport Tycoon Deluxe CD."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;-----------------------------------------------
; Uninstall section, deletes all installed files
Section "Uninstall"
	SetShellVarContext all

	IfFileExists "$INSTDIR\save" 0 NoRemoveSavedGames
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Remove the save game folders located at $\"$INSTDIR\save$\"?$\n \
		If you choose Yes, your saved games will be deleted." \
		IDYES RemoveSavedGames IDNO NoRemoveSavedGames
	RemoveSavedGames:
		Delete "$INSTDIR\save\autosave\*"
		RMDir "$INSTDIR\save\autosave"
		Delete "$INSTDIR\save\*"
		RMDir "$INSTDIR\save"
	NoRemoveSavedGames:

	IfFileExists "$INSTDIR\scenario" 0 NoRemoveScen
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Remove the scenario folders located at $\"$INSTDIR\scenario$\"?$\n \
		If you choose Yes, your scenarios will be deleted." \
		IDYES RemoveScen IDNO NoRemoveScen
	RemoveScen:
		Delete "$INSTDIR\scenario\heightmap*"
		RMDir "$INSTDIR\scenario\heightmap"
		Delete "$INSTDIR\scenario\*"
		RMDir "$INSTDIR\scenario"
	NoRemoveScen:

	; Remove from registry...
	!insertmacro MUI_STARTMENU_GETFOLDER "OpenTTD" $SHORTCUTS
	ReadRegStr $SHORTCUTS HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Shortcut Folder"

	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD"

	; Delete self
	Delete "$INSTDIR\uninstall.exe"

	; Delete Shortcuts
	Delete "$DESKTOP\OpenTTD.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\OpenTTD.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Uninstall.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Readme.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Changelog.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Known-bugs.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Docs\Multiplayer.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Docs\32bpp.lnk"
	Delete "$SMPROGRAMS\$SHORTCUTS\Scripts\Readme.lnk"

	; Clean up OpenTTD dir
	Delete "$INSTDIR\changelog.txt"
	Delete "$INSTDIR\readme.txt"
	Delete "$INSTDIR\known-bugs.txt"
	Delete "$INSTDIR\openttd.exe"
	Delete "$INSTDIR\COPYING"
	Delete "$INSTDIR\INSTALL.LOG"
	Delete "$INSTDIR\crash.log"
	Delete "$INSTDIR\crash.dmp"
	Delete "$INSTDIR\openttd.cfg"
	Delete "$INSTDIR\hs.dat"
	Delete "$INSTDIR\cached_sprites.*"
	Delete "$INSTDIR\save\autosave\network*.tmp" ; temporary network file

	; AI files
	Delete "$INSTDIR\ai\compat_*.nut"

	; Game Script files
	Delete "$INSTDIR\game\compat_*.nut"

	; Baseset files
	Delete "$INSTDIR\baseset\opntitle.dat"
	Delete "$INSTDIR\baseset\openttd.grf"
	Delete "$INSTDIR\baseset\orig_extra.grf"
	Delete "$INSTDIR\baseset\orig_win.obg"
	Delete "$INSTDIR\baseset\orig_dos.obg"
	Delete "$INSTDIR\baseset\orig_dos_de.obg"
	Delete "$INSTDIR\baseset\orig_win.obs"
	Delete "$INSTDIR\baseset\orig_dos.obs"
	Delete "$INSTDIR\baseset\no_sound.obs"
	Delete "$INSTDIR\baseset\sample.cat"
	Delete "$INSTDIR\baseset\trg1r.grf"
	Delete "$INSTDIR\baseset\trghr.grf"
	Delete "$INSTDIR\baseset\trgtr.grf"
	Delete "$INSTDIR\baseset\trgcr.grf"
	Delete "$INSTDIR\baseset\trgir.grf"
	Delete "$INSTDIR\baseset\trg1.grf"
	Delete "$INSTDIR\baseset\trgh.grf"
	Delete "$INSTDIR\baseset\trgt.grf"
	Delete "$INSTDIR\baseset\trgc.grf"
	Delete "$INSTDIR\baseset\trgi.grf"
	Delete "$INSTDIR\baseset\*.gm"

	Delete "$INSTDIR\data\sample.cat"
	Delete "$INSTDIR\data\trg1r.grf"
	Delete "$INSTDIR\data\trghr.grf"
	Delete "$INSTDIR\data\trgtr.grf"
	Delete "$INSTDIR\data\trgcr.grf"
	Delete "$INSTDIR\data\trgir.grf"
	Delete "$INSTDIR\data\trg1.grf"
	Delete "$INSTDIR\data\trgh.grf"
	Delete "$INSTDIR\data\trgt.grf"
	Delete "$INSTDIR\data\trgc.grf"
	Delete "$INSTDIR\data\trgi.grf"
	Delete "$INSTDIR\gm\*.gm"

	; Downloaded OpenGFX/OpenSFX/OpenMSX
	Delete "$INSTDIR\baseset\opengfx\*"
	RMDir  "$INSTDIR\baseset\opengfx"
	Delete "$INSTDIR\baseset\opensfx\*"
	RMDir  "$INSTDIR\baseset\opensfx"
	Delete "$INSTDIR\baseset\openmsx\*"
	RMDir  "$INSTDIR\baseset\openmsx"

	Delete "$INSTDIR\data\opengfx\*"
	RMDir  "$INSTDIR\data\opengfx"
	Delete "$INSTDIR\data\opensfx\*"
	RMDir  "$INSTDIR\data\opensfx"
	Delete "$INSTDIR\gm\openmsx\*"
	RMDir  "$INSTDIR\gm\openmsx"

	; Language files
	Delete "$INSTDIR\lang\*.lng"

	; Scripts
	Delete "$INSTDIR\scripts\*.*"

	; Documentation
	Delete "$INSTDIR\docs\*.*"

	; Base sets for music
	Delete "$INSTDIR\gm\orig_win.obm"
	Delete "$INSTDIR\gm\no_music.obm"
	Delete "$INSTDIR\baseset\orig_win.obm"
	Delete "$INSTDIR\baseset\no_music.obm"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\$SHORTCUTS\Extras\"
	RMDir "$SMPROGRAMS\$SHORTCUTS\Scripts\"
	RMDir "$SMPROGRAMS\$SHORTCUTS\Docs\"
	RMDir "$SMPROGRAMS\$SHORTCUTS"
	RMDir "$INSTDIR\ai"
	RMDir "$INSTDIR\game"
	RMDir "$INSTDIR\data"
	RMDir "$INSTDIR\baseset"
	RMDir "$INSTDIR\gm"
	RMDir "$INSTDIR\lang"
	RMDir "$INSTDIR\scripts"
	RMDir "$INSTDIR\docs"
	RMDir "$INSTDIR"

SectionEnd

;------------------------------------------------------------
; Custom page function to find the TTDLX CD/install location
Function SelectCDEnter
	SectionGetFlags ${Section2} $0
	IntOp $1 $0 & 0x80 ; bit 7 set by upgrade, no need to copy files
	IntCmp $1 1 DoneCD ; Upgrade doesn't need copy files

	IntOp $0 $0 & 1
	IntCmp $0 1 NoAbort
	Abort
NoAbort:

	GetTempFileName $R0
	!insertmacro MUI_HEADER_TEXT "Locate TTD" "Setup needs the location of Transport Tycoon Deluxe in order to continue."
	!insertmacro INSTALLOPTIONS_EXTRACT_AS "CDFinder.ini" "CDFinder"

	ClearErrors
	; Now, let's populate $CDDRIVE
	ReadRegStr $R0 HKLM "SOFTWARE\Fish Technology Group\Transport Tycoon Deluxe" "HDPath"
	IfErrors NoTTD
	StrCmp $CDDRIVE "" 0 Populated
	StrCpy $CDDRIVE $R0
Populated:
	StrCpy $AddWinPrePopulate "Setup has detected your TTD folder. Don't change the folder. Simply press Next."
	Goto TruFinish
NoTTD:
	StrCpy $AddWinPrePopulate "Setup couldn't find TTD. Please enter the path where the graphics files from TTD are stored and press Next to continue."
TruFinish:
	ClearErrors
	!insertmacro INSTALLOPTIONS_WRITE "CDFinder" "Field 2" "State" $CDDRIVE          ; TTDLX path
	!insertmacro INSTALLOPTIONS_WRITE "CDFinder" "Field 3" "Text" $AddWinPrePopulate ; Caption
DoneCD:
	; Initialize the dialog *AFTER* we've changed the text otherwise we won't see the changes
	!insertmacro INSTALLOPTIONS_INITDIALOG "CDFinder"
	!insertmacro INSTALLOPTIONS_SHOW
FunctionEnd

;----------------------------------------------------------------
; Custom page function when 'next' is selected for the TTDLX path
Function SelectCDExit
	!insertmacro INSTALLOPTIONS_READ $CDDRIVE "CDFinder" "Field 2" "State"
	; If trg1r.grf does not exist at the path, retry with DOS version
	IfFileExists $CDDRIVE\trg1r.grf "" DosCD
	IfFileExists $CDDRIVE\trgir.grf "" NoCD
	IfFileExists $CDDRIVE\sample.cat hasCD NoCD
DosCD:
	IfFileExists $CDDRIVE\TRG1.GRF "" NoCD
	IfFileExists $CDDRIVE\TRGI.GRF "" NoCD
	IfFileExists $CDDRIVE\SAMPLE.CAT hasCD NoCD
NoCD:
	MessageBox MB_OK "Setup cannot continue without the Transport Tycoon Deluxe location!"
	Abort
hasCD:
FunctionEnd

;-------------------------------------------------------------------------------
; Determine windows version, returns "win9x" if Win9x/Me/2000/XP SP2- or "winnt" for the rest on the stack
Function GetWindowsVersion
	GetVersion::WindowsPlatformArchitecture
	Pop $R0
	IntCmp $R0 64 WinNT 0
	ClearErrors
	StrCpy $R0 "win9x"
	${If} ${IsNT}
		${If} ${IsWinXP}
		${AndIf} ${AtLeastServicePack} 3
		${OrIf} ${AtLeastWin2003}
			GoTo WinNT
		${EndIf}
	${EndIf}
	GoTo Done
WinNT:
	StrCpy $R0 "winnt"
Done:
	Push $R0
FunctionEnd

;-------------------------------------------------------------------------------
; Check whether we're not running an installer for 64 bits on 32 bits and vice versa
Function CheckProcessorArchitecture
	GetVersion::WindowsPlatformArchitecture
	Pop $R0
	IntCmp $R0 64 Win64 0
	ClearErrors
	IntCmp ${APPBITS} 64 0 Done
	MessageBox MB_YESNO|MB_ICONSTOP "You are trying to install the 64-bit OpenTTD on a 32-bit operating system. This is not going to work. Please download the correct version. Do you really want to continue?" IDYES Done IDNO Abort
	GoTo Done
Win64:
	ClearErrors
	IntCmp ${APPBITS} 64 Done 0
	MessageBox MB_YESNO|MB_ICONINFORMATION "You are trying to install the 32-bit OpenTTD on a 64-bit operating system. This is not advised, but will work with reduced capabilities. We suggest that you download the correct version. Do you really want to continue?" IDYES Done IDNO Abort
	GoTo Done
Abort:
	Quit
Done:
FunctionEnd

;-------------------------------------------------------------------------------
; Check whether we're not running an installer for NT on 9x and vice versa
Function CheckWindowsVersion
	Call GetWindowsVersion
	Pop $R0
	StrCmp $R0 "win9x" 0 WinNT
	ClearErrors
	StrCmp ${APPARCH} "win9x" Done 0
	MessageBox MB_YESNO|MB_ICONSTOP "You are trying to install the Windows XP SP3, Vista and 7 version on Windows 95, 98, ME, 2000 and XP without SP3. This is will not work. Please download the correct version. Do you really want to continue?" IDYES Done IDNO Abort
	GoTo Done
WinNT:
	ClearErrors
	StrCmp ${APPARCH} "win9x" 0 Done
	MessageBox MB_YESNO|MB_ICONEXCLAMATION "You are trying to install the Windows 95, 98, 2000 and XP without SP3 version on Windows XP SP3, Vista or 7. This is not advised, but will work with reduced capabilities. We suggest that you download the correct version. Do you really want to continue?" IDYES Done IDNO Abort
Abort:
	Quit
Done:
FunctionEnd

;-------------------------------------------------------------------------------
; Check whether OpenTTD is running
Function CheckOpenTTDRunning
	IfFileExists "$INSTDIR\openttd.exe" 0 Done
Retry:
	FindProcDLL::FindProc "openttd.exe"
	Pop $R0
	IntCmp $R0 1 0 Done
	ClearErrors
	Delete "$INSTDIR\openttd.exe"
	IfErrors 0 Done
	ClearErrors
	MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "OpenTTD is running. Please close it and retry." IDRETRY Retry
	Abort
Done:
FunctionEnd

;-------------------------------------------------------------------------------
; strips all CRs
; and then converts all LFs into CRLFs
; (this is roughly equivalent to "cat file | dos2unix | unix2dos")
;
; usage:
;    Push "infile"
;    Call unix2dos
;
; beware that this function destroys $0 $1 $2
Function unix2dos
	ClearErrors

	Pop $2
	Rename $2 $2.U2D
	FileOpen $1 $2 w

	FileOpen $0 $2.U2D r

	Push $2 ; save name for deleting

	IfErrors unix2dos_done

	; $0 = file input (opened for reading)
	; $1 = file output (opened for writing)

unix2dos_loop:
	; read a byte (stored in $2)
	FileReadByte $0 $2
	IfErrors unix2dos_done ; EOL
	; skip CR
	StrCmp $2 13 unix2dos_loop
	; if LF write an extra CR
	StrCmp $2 10 unix2dos_cr unix2dos_write

unix2dos_cr:
	FileWriteByte $1 13

unix2dos_write:
	; write byte
	FileWriteByte $1 $2
	; read next byte
	Goto unix2dos_loop

unix2dos_done:
	; close files
	FileClose $0
	FileClose $1

	; delete original
	Pop $0
	Delete $0.U2D

FunctionEnd


Var OLDVERSION
Var UninstallString

;-----------------------------------------------------------------------------------
; NSIS Initialize function, determine if we are going to install/upgrade or uninstall
Function .onInit
	StrCpy $SHORTCUTS "OpenTTD"

	SectionSetSize ${Section3} 6144
	SectionSetSize ${Section4} 13312
	SectionSetSize ${Section5} 1024

	SectionSetFlags 0 17

	; Starts Setup - let's look for an older version of OpenTTD
	ReadRegDWORD $R8 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Version"

	IfErrors ShowWelcomeMessage ShowUpgradeMessage
ShowWelcomeMessage:
	ReadRegStr $R8 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Version"
	; In the event someone still has OpenTTD 0.1, this will detect that (that installer used a string instead of dword entry)
	IfErrors FinishCallback

ShowUpgradeMessage:
	IntCmp ${INSTALLERVERSION} $R8 VersionsAreEqual InstallerIsOlder  WelcomeToSetup
WelcomeToSetup:
	; An older version was found.  Let's let the user know there's an upgrade that will take place.
	ReadRegStr $OLDVERSION HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "DisplayVersion"
	; Gets the older version then displays it in a message box
	MessageBox MB_OK|MB_ICONINFORMATION \
		"Welcome to ${APPNAMEANDVERSION} Setup.$\nThis will allow you to upgrade from version $OLDVERSION."
	SectionSetFlags ${Section2} 0x80 ; set bit 7
	SectionSetFlags ${Section3} 0x80 ; set bit 7
	SectionSetFlags ${Section4} 0x80 ; set bit 7
	SectionSetFlags ${Section5} 0x80 ; set bit 7
	Goto FinishCallback

VersionsAreEqual:
	ReadRegStr $UninstallString HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "UninstallString"
	IfFileExists "$UninstallString" "" FinishCallback
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Setup detected ${APPNAMEANDVERSION} on your system. This is the same version that this program will install.$\nAre you trying to uninstall it?" \
		IDYES DoUninstall IDNO FinishCallback
DoUninstall: ; You have the same version as this installer.  This allows you to uninstall.
	Exec "$UninstallString"
	Quit

InstallerIsOlder:
	MessageBox MB_OK|MB_ICONSTOP \
		"You have a newer version of ${APPNAME}.$\nSetup will now exit."
	Quit

FinishCallback:
	ClearErrors
	Call CheckProcessorArchitecture
	Call CheckWindowsVersion
FunctionEnd
; eof

