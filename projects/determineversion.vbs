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

Sub UpdateFile(version, cur_date, filename)
	FSO.CopyFile filename & ".in", filename
	FindReplaceInFile filename, "@@VERSION@@", version
	FindReplaceInFile filename, "@@DATE@@", cur_date
End Sub

Sub UpdateFiles(version)
	Dim cur_date
	cur_date = DatePart("D", Date) & "." & DatePart("M", Date) & "." & DatePart("YYYY", Date)
	UpdateFile version, cur_date, "../src/rev.cpp"
	UpdateFile version, cur_date, "../src/ottdres.rc"
End Sub

Function DetermineSVNVersion()
	Dim WshShell, version, url, oExec
	Set WshShell = CreateObject("WScript.Shell")
	On Error Resume Next

	' Try TortoiseSVN
	' Get the directory where TortoiseSVN (should) reside(s)
	Dim sTortoise
	sTortoise = WshShell.RegRead("HKLM\SOFTWARE\TortoiseSVN\Directory")

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
				Dim line
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
