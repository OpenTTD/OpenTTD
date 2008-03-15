Option Explicit

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

Sub FindReplaceInFile(filename, to_find, replacement)
	Dim file, data
	Set file = FSO.OpenTextFile(filename, 1, 0, 0)
	data = file.ReadAll
	file.Close
	data = Replace(data, to_find, replacement)
	Set file = FSO.CreateTextFile(FileName, -1, 0)
	file.Write data
	file.Close
End Sub

Sub UpdateFile(revision, version, cur_date, filename)
	FSO.CopyFile filename & ".in", filename
	FindReplaceInFile filename, "@@REVISION@@", revision
	FindReplaceInFile filename, "@@VERSION@@", version
	FindReplaceInFile filename, "@@DATE@@", cur_date
End Sub

Sub UpdateFiles(version)
	Dim WshShell, cur_date, revision, oExec
	Set WshShell = CreateObject("WScript.Shell")
	cur_date = DatePart("D", Date) & "." & DatePart("M", Date) & "." & DatePart("YYYY", Date)
	revision = 0
	Select Case Mid(version, 1, 1)
		Case "r" ' svn
			revision = Mid(version, 2)
			If InStr(revision, "M") Then
				revision = Mid(revision, 1, InStr(revision, "M") - 1)
			End If
			If InStr(revision, "-") Then
				revision = Mid(revision, 1, InStr(revision, "-") - 1)
			End If
		Case "h" ' mercurial (hg)
			Set oExec = WshShell.Exec("hg log -k " & Chr(34) & "svn" & Chr(34) & " -l 1 --template " & Chr(34) & "{desc}\n" & Chr(34) & " ../src")
			If Err.Number = 0 Then
				revision = Mid(OExec.StdOut.ReadLine(), 7)
				revision = Mid(revision, 1, InStr(revision, ")") - 1)
			End If
		Case "g" ' git
			Set oExec = WshShell.Exec("git log --pretty=format:%s --grep=" & Chr(34) & "^(svn r[0-9]*)" & Chr(34) & " -1 ../src")
			if Err.Number = 0 Then
				revision = Mid(oExec.StdOut.ReadLine(), 7)
				revision = Mid(revision, 1, InStr(revision, ")") - 1)
			End If
	End Select

	UpdateFile revision, version, cur_date, "../src/rev.cpp"
	UpdateFile revision, version, cur_date, "../src/ottdres.rc"
End Sub

Function ReadRegistryKey(shive, subkey, valuename, architecture)
	Dim hiveKey, objCtx, objLocator, objServices, objReg, Inparams, Outparams

	' First, get the Registry Provider for the requested architecture
	Set objCtx = CreateObject("WbemScripting.SWbemNamedValueSet")
	objCtx.Add "__ProviderArchitecture", architecture ' Must be 64 of 32
	Set objLocator = CreateObject("Wbemscripting.SWbemLocator")
	Set objServices = objLocator.ConnectServer("","root\default","","",,,,objCtx)
	Set objReg = objServices.Get("StdRegProv")

	' Check the hive and give it the right value
	Select Case shive
		Case "HKCR", "HKEY_CLASSES_ROOT"
			hiveKey = &h80000000
		Case "HKCU", "HKEY_CURRENT_USER"
			hiveKey = &H80000001
		Case "HKLM", "HKEY_LOCAL_MACHINE"
			hiveKey = &h80000002
		Case "HKU", "HKEY_USERS"
			hiveKey = &h80000003
		Case "HKCC", "HKEY_CURRENT_CONFIG"
			hiveKey = &h80000005
		Case "HKDD", "HKEY_DYN_DATA" ' Only valid for Windows 95/98
			hiveKey = &h80000006
		Case Else
			MsgBox "Hive not valid (ReadRegistryKey)"
	End Select

	Set Inparams = objReg.Methods_("GetStringValue").Inparameters
	Inparams.Hdefkey = hiveKey
	Inparams.Ssubkeyname = subkey
	Inparams.Svaluename = valuename
	Set Outparams = objReg.ExecMethod_("GetStringValue", Inparams,,objCtx)

	ReadRegistryKey = Outparams.SValue
End Function

Function DetermineSVNVersion()
	Dim WshShell, version, url, oExec, line
	Set WshShell = CreateObject("WScript.Shell")
	On Error Resume Next

	' Try TortoiseSVN
	' Get the directory where TortoiseSVN (should) reside(s)
	Dim sTortoise
	' First, try with 32-bit architecture
	sTortoise = ReadRegistryKey("HKLM", "SOFTWARE\TortoiseSVN", "Directory", 32)
	If sTortoise = Nothing Then
		' No 32-bit version of TortoiseSVN installed, try 64-bit version (doesn't hurt on 32-bit machines, it returns nothing or is ignored)
		sTortoise = ReadRegistryKey("HKLM", "SOFTWARE\TortoiseSVN", "Directory", 64)
	End If

	' If TortoiseSVN is installed, try to get the revision number
	If sTortoise <> Nothing Then
		Dim file
		' Write some "magic" to a temporary file so we can acquire the svn revision/state
		Set file = FSO.CreateTextFile("tsvn_tmp", -1, 0)
		file.WriteLine "r$WCREV$$WCMODS?M:$"
		file.WriteLine "$WCURL$"
		file.Close
		Set oExec = WshShell.Exec(sTortoise & "\bin\SubWCRev.exe ../src tsvn_tmp tsvn_tmp")
		' Wait till the application is finished ...
		Do
			OExec.StdOut.ReadLine()
		Loop While Not OExec.StdOut.atEndOfStream

		Set file = FSO.OpenTextFile("tsvn_tmp", 1, 0, 0)
		version = file.ReadLine
		url = file.ReadLine
		file.Close

		Set file = FSO.GetFile("tsvn_tmp")
		file.Delete
	End If

	' Looks like there is no TortoiseSVN installed either. Then we don't know it.
	If InStr(version, "$") Then
		' Reset error and version
		Err.Clear
		version = "norev000"
		' Do we have subversion installed? Check immediatelly whether we've got a modified WC.
		Set oExec = WshShell.Exec("svnversion ../src")
		If Err.Number = 0 Then
			Dim modified
			If InStr(OExec.StdOut.ReadLine(), "M") Then
				modified = "M"
			Else
				modified = ""
			End If

			' Set the environment to english
			WshShell.Environment("PROCESS")("LANG") = "en"

			' And use svn info to get the correct revision and branch information.
			Set oExec = WshShell.Exec("svn info ../src")
			If Err.Number = 0 Then
				Do
					line = OExec.StdOut.ReadLine()
					If InStr(line, "URL") Then
						url = line
					End If
					If InStr(line, "Last Changed Rev") Then
						version = "r" & Mid(line, 19) & modified
					End If
				Loop While Not OExec.StdOut.atEndOfStream
			End If
		End If
	End If

	If version <> "norev000" Then
		If InStr(url, "branches") Then
			url = Mid(url, InStr(url, "branches") + 8)
			url = Mid(url, 1, InStr(2, url, "/") - 1)
			version = version & Replace(url, "/", "-")
		End If
	Else
		' svn detection failed, reset error and try git
		Err.Clear
		Set oExec = WshShell.Exec("git rev-parse --verify --short=8 HEAD")
		If Err.Number = 0 Then
			version = "g" & oExec.StdOut.ReadLine()
			Set oExec = WshShell.Exec("git diff-index --exit-code --quiet HEAD ../src")
			Do While oExec.Status = 0 And Err.Number = 0
			Loop
			If Err.Number = 0 And oExec.ExitCode = 1 Then
				version = version & "M"
			End If

			Set oExec = WshShell.Exec("git symbolic-ref HEAD")
			If Err.Number = 0 Then
				line = oExec.StdOut.ReadLine()
				line = Mid(line, InStrRev(line, "/")+1)
				If line <> "master" Then
					version = version & "-" & line
				End If
			End If
		Else
			' try mercurial (hg)
			Err.Clear
			Set oExec = WshShell.Exec("hg tip")
			If Err.Number = 0 Then
				version = "h" & Mid(OExec.StdOut.ReadLine(), 19, 8)
				Set oExec = WshShell.Exec("hg status ../src")
				If Err.Number = 0 Then
					Do
						line = OExec.StdOut.ReadLine()
						If Mid(line, 1, 1) <> "?" Then
							version = version & "M"
							Exit Do
						End If
					Loop While Not OExec.StdOut.atEndOfStream
				End If
				Set oExec = WshShell.Exec("hg branch")
				If Err.Number = 0 Then
						line = OExec.StdOut.ReadLine()
						If line <> "default" Then
							version = version & "-" & line
						End If
				End If
			End If
		End If
	End If

	DetermineSVNVersion = version
End Function

Function IsCachedVersion(version)
	Dim cache_file, cached_version
	cached_version = ""
	Set cache_file = FSO.OpenTextFile("../config.cache.version", 1, True, 0)
	If Not cache_file.atEndOfStream Then
		cached_version = cache_file.ReadLine()
	End If
	cache_file.Close

	If version <> cached_version Then
		Set cache_file = fso.CreateTextFile("../config.cache.version", True)
		cache_file.WriteLine(version)
		cache_file.Close
		IsCachedVersion = False
	Else
		IsCachedVersion = True
	End If
End Function

Dim version
version = DetermineSVNVersion
If Not (IsCachedVersion(version) And FSO.FileExists("../src/rev.cpp") And FSO.FileExists("../src/ottdres.rc")) Then
	UpdateFiles version
End If
