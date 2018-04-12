Option Explicit

' $Id$
'
' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

Sub FindReplaceInFile(filename, to_find, replacement)
	Dim file, data
	Set file = FSO.OpenTextFile(filename, 1, 0, 0)
	data = file.ReadAll
	file.Close
	data = Replace(data, to_find, replacement)
	Set file = FSO.CreateTextFile(filename, -1, 0)
	file.Write data
	file.Close
End Sub

Sub UpdateFile(modified, isodate, version, cur_date, filename)
	FSO.CopyFile filename & ".in", filename
	FindReplaceInFile filename, "!!MODIFIED!!", modified
	FindReplaceInFile filename, "!!ISODATE!!", isodate
	FindReplaceInFile filename, "!!VERSION!!", version
	FindReplaceInFile filename, "!!DATE!!", cur_date
End Sub

Sub UpdateFiles(version)
	Dim modified, isodate, cur_date
	cur_date = DatePart("D", Date) & "." & DatePart("M", Date) & "." & DatePart("YYYY", Date)

	If InStr(version, Chr(9)) Then
		isodate  = Mid(version, InStr(version, Chr(9)) + 1)
		modified = Mid(isodate, InStr(isodate, Chr(9)) + 1)
		isodate  = Mid(isodate, 1, InStr(isodate, Chr(9)) - 1)
		modified = Mid(modified, 1, InStr(modified, Chr(9)) - 1)
		version  = Mid(version, 1, InStr(version, Chr(9)) - 1)
	Else
		isodate = 0
		modified = 1
	End If

	UpdateFile modified, isodate, version, cur_date, "../src/rev.cpp"
	UpdateFile modified, isodate, version, cur_date, "../src/os/windows/ottdres.rc"
End Sub

Function DetermineVersion()
	Dim WshShell, branch, tag, modified, isodate, oExec, line, hash, shorthash
	Set WshShell = CreateObject("WScript.Shell")
	On Error Resume Next

	modified = 1
	hash = ""
	shorthash = ""
	branch = ""
	isodate = ""
	tag = ""

	' Set the environment to english
	WshShell.Environment("PROCESS")("LANG") = "en"

	Set oExec = WshShell.Exec("git rev-parse --verify HEAD")
	If Err.Number = 0 Then
		' Wait till the application is finished ...
		Do While oExec.Status = 0
		Loop

		If oExec.ExitCode = 0 Then
			hash = oExec.StdOut.ReadLine()
			shorthash = Mid(hash, 1, 8)
			' Make sure index is in sync with disk
			Set oExec = WshShell.Exec("git update-index --refresh")
			If Err.Number = 0 Then
				' StdOut and StdErr share a 4kB buffer so prevent it from filling up as we don't care about the output
				oExec.StdOut.Close
				oExec.StdErr.Close
				' Wait till the application is finished ...
				Do While oExec.Status = 0
					WScript.Sleep 10
				Loop
			End If
			Set oExec = WshShell.Exec("git diff-index --exit-code --quiet HEAD ../")
			If Err.Number = 0 Then
				' Wait till the application is finished ...
				Do While oExec.Status = 0
				Loop

				If oExec.ExitCode = 1 Then
					modified = 2
				End If ' oExec.ExitCode = 1

				Set oExec = WshShell.Exec("git show -s --pretty=format:%ci")
				if Err.Number = 0 Then
					isodate = Mid(oExec.StdOut.ReadLine(), 1, 10)
					isodate = Replace(isodate, "-", "")
				End If ' Err.Number = 0

				' Check branch
				Err.Clear
				Set oExec = WshShell.Exec("git symbolic-ref HEAD")
				If Err.Number = 0 Then
					line = oExec.StdOut.ReadLine()
					branch = Mid(line, InStrRev(line, "/") + 1)
				End If ' Err.Number = 0

				' Check if a tag is currently checked out
				Err.Clear
				Set oExec = WshShell.Exec("git name-rev --name-only --tags --no-undefined HEAD")
				If Err.Number = 0 Then
					' Wait till the application is finished ...
					Do While oExec.Status = 0
					Loop
					If oExec.ExitCode = 0 Then
						tag = oExec.StdOut.ReadLine()
						If Right(tag, 2) = "^0" Then
							tag = Left(tag, Len(tag) - 2)
						End If
					End If ' oExec.ExitCode = 0
				End If ' Err.Number = 0
			End If ' Err.Number = 0
		End If ' oExec.ExitCode = 0
	End If ' Err.Number = 0

	If hash = "" And FSO.FileExists("../.ottdrev") Then
		Dim rev_file
		Set rev_file = FSO.OpenTextFile("../.ottdrev", 1, True, 0)
		DetermineVersion = rev_file.ReadLine()
		rev_file.Close()
	ElseIf hash = "" Then
		DetermineVersion = "norev000"
	Else
		Dim version
		If tag <> "" Then
			version = tag
		ElseIf branch = "master" Then
			version = isodate & "-g" & shorthash
		Else
			version = isodate & "-" & branch & "-g" & shorthash
		End If

		If modified = 2 Then
			version = version & "M"
		End If

		DetermineVersion = version & Chr(9) & isodate & Chr(9) & modified & Chr(9) & hash
	End If
End Function

Function IsCachedVersion(ByVal version)
	Dim cache_file, cached_version
	cached_version = ""
	Set cache_file = FSO.OpenTextFile("../config.cache.version", 1, True, 0)
	If Not cache_file.atEndOfStream Then
		cached_version = cache_file.ReadLine()
	End If
	cache_file.Close

	If InStr(version, Chr(9)) Then
		version = Mid(version, 1, Instr(version, Chr(9)) - 1)
	End If

	If version <> cached_version Then
		Set cache_file = fso.CreateTextFile("../config.cache.version", True)
		cache_file.WriteLine(version)
		cache_file.Close
		IsCachedVersion = False
	Else
		IsCachedVersion = True
	End If
End Function

Function CheckFile(filename)
	CheckFile = FSO.FileExists(filename)
	If CheckFile Then CheckFile = (FSO.GetFile(filename).DateLastModified >= FSO.GetFile(filename & ".in").DateLastModified)
End Function

Dim version
version = DetermineVersion
If Not (IsCachedVersion(version) And CheckFile("../src/rev.cpp") And CheckFile("../src/os/windows/ottdres.rc")) Then
	UpdateFiles version
End If
