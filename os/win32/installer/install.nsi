!define APPNAME "OpenTTD"   ; Define application name
!define APPVERSION "0.5.0"  ; Define application version
!define INSTALLERVERSION 28 ; NEED TO UPDATE THIS FOR EVERY RELEASE!!!

!define APPURLLINK "http://www.openttd.org"
!define APPNAMEANDVERSION "${APPNAME} ${APPVERSION}"
!define APPVERSIONINTERNAL "${APPVERSION}.0" ; Needs to be of the format X.X.X.X

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
VIAddVersionKey "ProductName" "OpenTTD Installer"
VIAddVersionKey "Comments" "Installs ${APPNAMEANDVERSION}"
VIAddVersionKey "CompanyName" "OpenTTD Developers"
VIAddVersionKey "FileDescription" "Installs ${APPNAMEANDVERSION}"
VIAddVersionKey "ProductVersion" "${APPVERSION}"
VIAddVersionKey "InternalName" "InstOpenTTD"
VIAddVersionKey "FileVersion" "${APPVERSION}"
VIAddVersionKey "LegalCopyright" " "
; Main Install settings
Name "${APPNAMEANDVERSION}"

; NOTE: Keep trailing backslash!
InstallDir "$PROGRAMFILES\OpenTTD\"
InstallDirRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "Install Folder"
OutFile "openttd-${APPVERSION}-win32.exe"
CRCCheck force

ShowInstDetails show
ShowUninstDetails show

Var SHORTCUTS
Var CDDRIVE

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME

!define MUI_LICENSEPAGE_RADIOBUTTONS
!insertmacro MUI_DEFAULT MUI_LICENSEPAGE_RADIOBUTTONS_TEXT_ACCEPT "I &accept this agreement"
!insertmacro MUI_DEFAULT MUI_LICENSEPAGE_RADIOBUTTONS_TEXT_DECLINE "I &do not accept this agreement"
!insertmacro MUI_PAGE_LICENSE "..\..\..\COPYING"

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

;-----------------------------------------------------
; New custom page to show UNICODE and MSLU information
Page custom ShowWarningsPage

!define MUI_FINISHPAGE_RUN "$INSTDIR\openttd.exe"
!define MUI_FINISHPAGE_LINK "Visit OpenTTD's homepage"
!define MUI_FINISHPAGE_LINK_LOCATION "${APPURLLINK}"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\readme.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

;--------------------------------------------------------------
; (Core) OpenTTD install section. Copies all internal game data
Section "!OpenTTD" Section1
	; Overwrite files by default, but don't complain on failure
	SetOverwrite try

	; Make savegame folder
	SetOutPath "$INSTDIR\save"
	; Define root variable relative to installer
	!define PATH_ROOT "..\..\..\"

	; Copy language files
	SetOutPath "$INSTDIR\lang\"
	File ${PATH_ROOT}bin\lang\*.lng
	File ${PATH_ROOT}src\lang\english.txt

	; Copy data files
	SetOutPath "$INSTDIR\data\"
	File ${PATH_ROOT}bin\data\*.grf
	File ${PATH_ROOT}bin\data\opntitle.dat
	; Copy scenario files (don't choke if they don't exist)
	SetOutPath "$INSTDIR\scenario\"
	File /nonfatal ${PATH_ROOT}bin\scenario\*.scn

	; Copy the rest of the stuff
	SetOutPath "$INSTDIR\"

	; Copy text files
	File ${PATH_ROOT}changelog.txt
	File ${PATH_ROOT}COPYING
	File ${PATH_ROOT}readme.txt
	File ${PATH_ROOT}known-bugs.txt

	; Copy executable
	File /oname=openttd.exe ${PATH_ROOT}objs\Win32\Release\openttd.exe
	File ${PATH_ROOT}objs\strgen\strgen.exe


	; Delete old files from the main dir. they are now placed in data/ and lang/
	Delete "$INSTDIR\*.lng"
	Delete "$INSTDIR\*.grf"
	Delete "$INSTDIR\sample.cat"
	Delete "$INSTDIR\ttd.exe"


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
	!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

;----------------------------------------------------------------------------------
; TTDLX files install section. Copies all needed TTDLX files from CD or install dir
Section "Copy Game Graphics" Section2
	; Let's copy the files with size approximation
	SetOutPath "$INSTDIR\gm"
	CopyFiles "$CDDRIVE\gm\*.gm" "$INSTDIR\gm\" 1028
	SetOutPath "$INSTDIR\data\"
	CopyFiles "$CDDRIVE\sample.cat" "$INSTDIR\data\sample.cat" 1566
	; Copy Windows files
	CopyFiles "$CDDRIVE\trg1r.grf" "$INSTDIR\data\trg1r.grf" 2365
	CopyFiles "$CDDRIVE\trgcr.grf" "$INSTDIR\data\trgcr.grf" 260
	CopyFiles "$CDDRIVE\trghr.grf" "$INSTDIR\data\trghr.grf" 400
	CopyFiles "$CDDRIVE\trgir.grf" "$INSTDIR\data\trgir.grf" 334
	CopyFiles "$CDDRIVE\trgtr.grf" "$INSTDIR\data\trgtr.grf" 546
	; Copy DOS files
	CopyFiles "$CDDRIVE\trg1.grf" "$INSTDIR\data\trg1.grf" 2365
	CopyFiles "$CDDRIVE\trgc.grf" "$INSTDIR\data\trgc.grf" 260
	CopyFiles "$CDDRIVE\trgh.grf" "$INSTDIR\data\trgh.grf" 400
	CopyFiles "$CDDRIVE\trgi.grf" "$INSTDIR\data\trgi.grf" 334
	CopyFiles "$CDDRIVE\trgt.grf" "$INSTDIR\data\trgt.grf" 546
	SetOutPath "$INSTDIR\"
SectionEnd

;-------------------------------------------
; Install the uninstaller (option is hidden)
Section -FinishSection
	WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "OpenTTD is a fully functional clone of TTD and is very playable."
	!insertmacro MUI_DESCRIPTION_TEXT ${Section2} "Copies the game graphics. Requires TTD (for Windows)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;-----------------------------------------------
; Uninstall section, deletes all installed files
Section "Uninstall"
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Remove the save game folders located at $\"$INSTDIR\save?$\"$\n \
		If you choose Yes, your saved games will be deleted." \
		IDYES RemoveSavedGames IDNO NoRemoveSavedGames
	RemoveSavedGames:
		Delete "$INSTDIR\save\autosave\*"
		RMDir "$INSTDIR\save\autosave"
		Delete "$INSTDIR\save\*"
		RMDir "$INSTDIR\save"
	NoRemoveSavedGames:

	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Remove the scenario folders located at $\"$INSTDIR\scenario?$\"$\n \
		If you choose Yes, your scenarios will be deleted." \
		IDYES RemoveScen IDNO NoRemoveScen
	RemoveScen:
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

	; Clean up OpenTTD dir
	Delete "$INSTDIR\changelog.txt"
	Delete "$INSTDIR\readme.txt"
	Delete "$INSTDIR\known-bugs.txt"
	Delete "$INSTDIR\openttd.exe"
	Delete "$INSTDIR\strgen.exe"
	Delete "$INSTDIR\COPYING"
	Delete "$INSTDIR\INSTALL.LOG"
	Delete "$INSTDIR\crash.log"
	Delete "$INSTDIR\openttd.cfg"
	Delete "$INSTDIR\hs.dat"
	Delete "$INSTDIR\cached_sprites.*"
	Delete "$INSTDIR\save\autosave\network*.tmp" ; temporary network file

	; Data files
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

	Delete "$INSTDIR\data\sample.cat"
	; Windows Data files
	Delete "$INSTDIR\data\trg1r.grf"
	Delete "$INSTDIR\data\trghr.grf"
	Delete "$INSTDIR\data\trgtr.grf"
	Delete "$INSTDIR\data\trgcr.grf"
	Delete "$INSTDIR\data\trgir.grf"
	; Dos Data files
	Delete "$INSTDIR\data\trg1.grf"
	Delete "$INSTDIR\data\trgh.grf"
	Delete "$INSTDIR\data\trgt.grf"
	Delete "$INSTDIR\data\trgc.grf"
	Delete "$INSTDIR\data\trgi.grf"

	; Music
	Delete "$INSTDIR\gm\*.gm"

	; Language files
	Delete "$INSTDIR\lang\*.lng"
	Delete "$INSTDIR\lang\english.txt"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\$SHORTCUTS\Extras\"
	RMDir "$SMPROGRAMS\$SHORTCUTS"
	RMDir "$INSTDIR\gm"
	RMDir "$INSTDIR\lang"
	RMDir "$INSTDIR\data"
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
	!insertmacro MUI_INSTALLOPTIONS_EXTRACT_AS "CDFinder.ini" "CDFinder"

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
	!insertmacro MUI_INSTALLOPTIONS_WRITE "CDFinder" "Field 2" "State" $CDDRIVE          ; TTDLX path
	!insertmacro MUI_INSTALLOPTIONS_WRITE "CDFinder" "Field 3" "Text" $AddWinPrePopulate ; Caption
DoneCD:
	; Initialize the dialog *AFTER* we've changed the text otherwise we won't see the changes
	!insertmacro MUI_INSTALLOPTIONS_INITDIALOG "CDFinder"
	!insertmacro MUI_INSTALLOPTIONS_SHOW
FunctionEnd

;----------------------------------------------------------------
; Custom page function when 'next' is selected for the TTDLX path
Function SelectCDExit
	!insertmacro MUI_INSTALLOPTIONS_READ $CDDRIVE "CDFinder" "Field 2" "State"
	; If trg1r.grf does not exist at the path, retry with DOS version
	IfFileExists $CDDRIVE\trg1r.grf "" DosCD
	IfFileExists $CDDRIVE\trgir.grf "" NoCD
	IfFileExists $CDDRIVE\sample.cat hasCD NoCD
DosCD:
	IfFileExists $CDDRIVE\TRG1.GRF "" NoCD
	IfFileExists $CDDRIVE\TRGI.GRF "" NoCD
	IfFileExists $CDDRIVE\SAMPLE.CAT hasCD NoCD
NoCD:
	MessageBox MB_OK "Setup cannot continue without the Transport Tycoon Deluxe Location!"
	Abort
hasCD:
FunctionEnd

;---------------------------------------------------------------------
; Custom page function to show notices for running OpenTTD
; We have extracted this custom page as Notice in the .onInit function
Function ShowWarningsPage
	!insertmacro MUI_HEADER_TEXT "Installation Complete" "Important notices for OpenTTD usage."
	!insertmacro MUI_INSTALLOPTIONS_EXTRACT_AS "notice.ini" "Notice"
	!insertmacro MUI_INSTALLOPTIONS_INITDIALOG "Notice"

	Call GetWindowsVersion
	Pop $R0

	; Hide the MSLU text if the installer is not run on Win9x systems
	StrCmp $R0 "winnt" 0 Win9x
	!insertmacro MUI_INSTALLOPTIONS_READ $R1 "Notice" "Field 1" "HWND" ; MSLU groupbox
	ShowWindow $R1 0
	!insertmacro MUI_INSTALLOPTIONS_READ $R1 "Notice" "Field 2" "HWND" ; MSLU text
	ShowWindow $R1 0
	!insertmacro MUI_INSTALLOPTIONS_READ $R1 "Notice" "Field 3" "HWND" ; MSLU link
	ShowWindow $R1 0
Win9x:
	ClearErrors
	!insertmacro MUI_INSTALLOPTIONS_SHOW
FunctionEnd

;-------------------------------------------------------------------------------
; Determine windows version, returns "win9x" if Win9x/Me or "winnt" on the stack
Function GetWindowsVersion
	ClearErrors
	StrCpy $R0 "winnt"

	ReadRegStr $R1 HKLM "SOFTWARE\MICROSOFT\WINDOWS NT\CurrentVersion" CurrentVersion
	IfErrors 0 WinNT
	StrCpy $R0 "win9x"
WinNT:
	ClearErrors
	Push $R0
FunctionEnd

Var OLDVERSION
Var UninstallString

;-----------------------------------------------------------------------------------
; NSIS Initialize function, determin if we are going to install/upgrade or uninstall
Function .onInit
	StrCpy $SHORTCUTS "OpenTTD"

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
		"Welcome to ${APPNAMEANDVERSION} Setup.$\n \
		This will allow you to upgrade from version $OLDVERSION."
	SectionSetFlags ${Section2} 0x80 ; set bit 7
	Goto FinishCallback

VersionsAreEqual:
	ReadRegStr $UninstallString HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenTTD" "UninstallString"
	IfFileExists "$UninstallString" "" FinishCallback
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"Setup detected ${APPNAMEANDVERSION} on your system. That's the version this program will install.$\n \
		Are you trying to uninstall it?" \
		IDYES DoUninstall IDNO FinishCallback
DoUninstall: ; You have the same version as this installer.  This allows you to uninstall.
	Exec "$UninstallString"
	Quit

InstallerIsOlder:
	MessageBox MB_OK|MB_ICONSTOP \
		"You have a newer version of ${APPNAME}.$\n \
		Setup will now exit."
	Quit

FinishCallback:
	ClearErrors
FunctionEnd
; eof

