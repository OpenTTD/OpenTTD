Option Explicit

' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

Function GetTestList()
	Dim retests, i, tests, dir
	Set retests = New RegExp
	Set GetTestList = CreateObject("Scripting.Dictionary")

	retests.Pattern = "ai/regression/tst_*"
	retests.Global = True
	For i = 0 To WScript.Arguments.Count - 1
		Dim test
		test = "ai/regression/tst_" & WScript.Arguments.Item(i)
		If FSO.FolderExists(test) Then
			retests.Pattern = test
			Exit For
		End If
	Next

	For Each dir In FSO.GetFolder("ai/regression/").SubFolders
		Dim name
		name = "ai/regression/" & dir.Name
		If retests.Test(name) Then
			GetTestList.Add name, name
		End If
	Next
End Function

Function GetParams()
	GetParams = "-snull -mnull -vnull:ticks=30000"
	If WScript.Arguments.Count = 0 Then Exit Function
	If WScript.Arguments.Item(0) <> "-r" Then Exit Function
	GetParams = ""
End Function

Sub FilterFile(filename)
	Dim lines, filter, file

	Set file = FSO.OpenTextFile(filename, 1)
	If Not file.AtEndOfStream Then
		lines = file.ReadAll
	End If
	file.Close

	Set filter = New RegExp
	filter.Global = True
	filter.Multiline = True
	filter.Pattern = "0x(\(nil\)|0+)(x0)?"
	lines = filter.Replace(lines, "0x00000000")
	filter.Pattern = "^dbg: \[script\]"
	lines = filter.Replace(lines, "")
	filter.Pattern = "^ "
	lines = filter.Replace(lines, "ERROR: ")
	filter.Pattern = "ERROR: \[1\] \[P\] "
	lines = filter.Replace(lines, "")
	filter.Pattern = "^dbg: .*\r\n"
	lines = filter.Replace(lines, "")

	Set file = FSO.OpenTextFile(filename, 2)
	file.Write lines
	file.Close
End Sub

Function CompareFiles(filename1, filename2)
	Dim file, lines1, lines2
	Set file = FSO.OpenTextFile(filename1, 1)
	If Not file.AtEndOfStream Then
		lines1 = file.ReadAll
	End IF
	file.Close
	Set file = FSO.OpenTextFile(filename2, 1)
	If Not file.AtEndOfStream Then
		lines2 = file.ReadAll
	End IF
	file.Close
	CompareFiles = (lines1 = lines2)
End Function

Function RunTest(test, params, ret)
	Dim WshShell, oExec, sav, command
	Set WshShell = CreateObject("WScript.Shell")

	' Make sure that only one info.nut is present for each test run. Otherwise openttd gets confused.
	FSO.CopyFile "ai/regression/regression_info.nut", test & "/info.nut"

	sav = test & "/test.sav"
	If Not FSO.FileExists(sav) Then
		sav = "ai/regression/empty.sav"
	End If

	command = ".\openttd -x -c ai/regression/regression.cfg " & params & " -g " & sav & " -d script=2 -d misc=9"
	' 2>&1 must be after >tmp.regression, else stderr is not redirected to the file
	WshShell.Run "cmd /c " & command & " >"& test & "/tmp.regression 2>&1", 0, True

	FilterFile test & "/tmp.regression"

	If CompareFiles(test & "/result.txt", test & "/tmp.regression") Then
		RunTest = "passed!"
	Else
		RunTest = "failed!"
		ret = 1
	End If

	FSO.DeleteFile test & "/info.nut"

	If WScript.Arguments.Count > 0 Then
		If WScript.Arguments.Item(0) = "-k" Then
			Exit Function
		End If
	End If

	FSO.DeleteFile test & "/tmp.regression"
End Function

On Error Resume Next
WScript.StdOut.WriteLine ""
If Err.Number <> 0 Then
	WScript.Echo "This script must be started with cscript."
	WScript.Quit 1
End If
On Error Goto 0

If Not FSO.FileExists("ai/regression/run.vbs") Then
	WScript.Echo "Make sure you are in the root of OpenTTD before starting this script."
	WScript.Quit 1
End If

If FSO.FileExists("scripts/game_start.scr") Then
	FSO.MoveFile "scripts/game_start.scr", "scripts/game_start.scr.regression"
End If

Dim params, test, ret
params = GetParams()
ret = 0

For Each test in GetTestList()
	WScript.StdOut.Write "Running " & test & "... "
	WScript.StdOut.WriteLine RunTest(test, params, ret)
Next

If FSO.FileExists("scripts/game_start.scr.regression") Then
	FSO.MoveFile "scripts/game_start.scr.regression", "scripts/game_start.scr"
End If

WScript.Quit ret
