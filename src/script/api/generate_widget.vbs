Option Explicit

' $Id$
'
' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO, filename, skiptillend, eof
Set FSO = CreateObject("Scripting.FileSystemObject")

filename = "script_window.hpp"
skiptillend = False
eof = vbCrLf

If Not FSO.FileExists(filename) Then
	WScript.Echo filename & " not found"
	WScript.Quit 1
End If

Function GetFiles(pattern)
	Dim parent, re, files, f
	Set files = CreateObject("Scripting.Dictionary")
	Set re = New RegExp

	parent = FSO.GetParentFolderName(pattern)
	pattern = FSO.GetFileName(pattern)

	' Convert pattern to a valid regex
	re.Global = True
	re.Pattern = "\."
	pattern = re.Replace(pattern, "\.")
	re.Pattern = "\*"
	pattern = re.Replace(pattern, ".*")
	re.Pattern = pattern

	' Get the file list
	For Each f In FSO.GetFolder(parent).Files
		If re.Test(f.Path) Then
			f = parent & "/" & f.Name
			files.Add f, f
		End If
	Next

	' Sort the file list
	Set GetFiles = CreateObject("Scripting.Dictionary")
	While files.Count <> 0
		Dim first
		first = ""
		For Each f in files
			If first = "" Or StrComp(first, f) = 1 Then first = f
		Next
		GetFiles.Add first, first
		files.Remove(First)
	Wend
End Function

Sub Generate(line, file)
	Dim re, add_indent, enum_pattern, file_pattern, f, active, active_comment, comment, rm_indent
	Set re = New RegExp

	re.Global = True
	re.Pattern = "[^	]*"
	add_indent = re.Replace(line, "")
	re.Global = False
	re.Pattern = ".*@enum *"
	line = Split(re.Replace(line, ""))
	enum_pattern = line(0)
	file_pattern = line(1)
	For Each f In GetFiles(file_pattern).Items
		active = 0
		active_comment = 0
		comment = ""
		file.Write add_indent & "/* automatically generated from " & f & " */" & eof
		Set f = FSO.OpenTextFile(f, 1)
		While Not f.AtEndOfStream
			re.Pattern = rm_indent
			line = re.Replace(f.ReadLine, "")

			' Remember possible doxygen comment before enum declaration
			re.Pattern = "/\*\*"
			If active = 0 And re.Test(line) Then
				comment = add_indent & line
				active_comment = 1
			ElseIf active_comment = 1 Then
				comment = comment & vbCrLf & add_indent & line
			End If

			' Check for enum match
			re.Pattern = "^	*enum *" & enum_pattern & " *\{"
			If re.Test(line) Then
				re.Global = True
				re.Pattern = "[^	]*"
				rm_indent = re.Replace(line, "")
				re.Global = False
				active = 1
				If active_comment > 0 Then file.Write comment & eof
				active_comment = 0
				comment = ""
			End If

			' Forget doxygen comment, if no enum follows
			If active_comment = 2 And line <> "" Then
				active_comment = 0
				comment = ""
			End If
			re.Pattern = "\*/"
			If active_comment = 1 And re.Test(line) Then active_comment = 2

			If active <> 0 Then
				re.Pattern = "^	*[A-Za-z0-9_]* *[,=]"
				If re.Test(line) Then
					Dim parts
					' Transform enum values
					re.Pattern = " *=[^,]*"
					line = re.Replace(line, "")
					re.Pattern = " *//"
					line = re.Replace(line, " //")

					re.Pattern = "^(	*)([A-Za-z0-9_]+),(.*)"
					Set parts = re.Execute(line)

					With parts.Item(0).SubMatches
						If .Item(2) = "" Then
							file.Write add_indent & .Item(0) & .Item(1) & String(45 - Len(.Item(1)), " ") & "= ::" & .Item(1) & "," & eof
						Else
							file.Write add_indent & .Item(0) & .Item(1) & String(45 - Len(.Item(1)), " ") & "= ::" & .Item(1) & "," & String(44 - Len(.Item(1)), " ") & .Item(2) & eof
						End If
					End With
				ElseIf line = "" Then
					file.Write eof
				Else
					file.Write add_indent & line & eof
				End If
			End If

			re.Pattern = "^	*\};"
			If re.Test(line) Then
				If active <> 0 Then file.Write eof
				active = 0
			End If
			Wend
		f.Close
	Next
End Sub

Sub Parse(line, file)
	Dim re
	Set re = New RegExp

	re.pattern = "@enum"
	If re.Test(line) Then
		file.Write line & eof
		Generate line, file
		skiptillend = True
		Exit Sub
	End If

	re.pattern = "@endenum"
	If re.Test(line) Then
		file.Write line & eof
		skiptillend = False
		Exit Sub
	End If

	If Not skiptillend Then
		file.Write line & eof
	End If
End Sub

Dim file, source, lines, i

WScript.Echo "Starting to parse " & filename
Set file = FSO.OpenTextFile(filename, 1)
If Not file.AtEndOfStream Then
	source = file.ReadAll
End IF
file.Close

lines = Split(source, eof)
If UBound(lines) = 0 Then
	eof = vbLf
	lines = Split(source, eof)
End If

Set file = FSO.OpenTextFile(filename, 2)
For i = LBound(lines) To UBound(lines) - 1 ' Split adds an extra line, we must ignore it
	Parse lines(i), file
Next
file.Close
WScript.Echo "Done"
