Option Explicit

' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

Dim inputfile, outputfile, langpath, extra_grf
inputfile  = WScript.Arguments(0)
outputfile = WScript.Arguments(1)
langpath   = WScript.Arguments(2)

If WScript.Arguments.Length > 3 Then
	extra_grf = WScript.Arguments(3)
End If

Function GetExtraGrfHash
	Dim WSO, exe, line

	Set WSO = WScript.CreateObject("WScript.Shell")
	Set exe = WSO.Exec("certutil -hashfile " & extra_grf & " MD5")

	Do Until exe.StdOut.AtEndOfStream
		line = exe.StdOut.ReadLine
		If Len(line) = 32 Then GetExtraGrfHash = line
	Loop

	Set WSO = Nothing
End Function

' Simple insertion sort, copied from translations.awk
Sub ISort(a)
	Dim i, j, n, hold
	n = UBound(a)

	For i = 1 To n
		j = i
		hold = a(j)
		Do While a(j - 1) > hold
			j = j - 1
			a(j + 1) = a(j)

			If j = 0 Then Exit Do
		Loop
		a(j) = hold
	Next
End Sub

Sub Lookup(ini_key, str_id, outfile)
	Dim folder, file, line, p, lang, i

	' Ensure only complete string matches
	str_id = str_id & " "

	Set folder = FSO.GetFolder(langpath)

	Dim output()
	ReDim output(folder.Files.Count)

	For Each file In folder.Files
		If UCase(FSO.GetExtensionName(file.Name)) = "TXT" Then
			Dim f
			Set f = CreateObject("ADODB.Stream")
			f.Charset = "utf-8"
			f.LineSeparator = 10 ' Assume lines end with \n even for \r\n files
			f.Open
			f.LoadFromFile(file.Path)

			Do Until f.EOS
				line = Replace(f.ReadText(-2), Chr(13), "") ' Read a line and remove any \r

				If InStr(1, line, "##isocode ") = 1 Then
					p = Split(line)
					lang = p(1)
				ElseIf InStr(1, line, str_id) = 1 Then
					p = Split(line, ":", 2)
					If lang = "en_GB" Then
						output(i) = ini_key & "       = " & p(1)
					Else
						output(i) = ini_key & "." & lang & " = " & p(1)
					End If
					i = i + 1
				End If
			Loop

			f.Close
		End If
	Next

	ReDim Preserve output(i - 1)
	ISort output

	For Each line In output
		outfile.WriteText line, 1
	Next

End Sub

Dim line, p

Dim infile
Set infile = FSO.OpenTextFile(inputfile)

Dim outfile
Set outfile = CreateObject("ADODB.Stream")
outfile.Charset = "utf-8"
outfile.Open

Do Until infile.atEndOfStream

	line = infile.ReadLine()

	If InStr(1, line, "ORIG_EXTRA.GRF ") = 1 Then
		p = Split(line, "=")
		If Trim(p(1)) = "" Then
			outfile.WriteText "ORIG_EXTRA.GRF    = " & GetExtraGrfHash(), 1
		Else
			outfile.WriteText line, 1
		End If
	ElseIf InStr(1, line, "!! ") = 1 Then
		p = Split(line)
		Lookup p(1), p(2), outfile
	Else
		outfile.WriteText line, 1
	End If

Loop

' UTF-8 Text ADO Stream includes BOM, so we need to remove it
Dim outfile_noBOM
Set outfile_noBOM = CreateObject("ADODB.Stream")
outfile_noBOM.Type = 1
outfile_noBOM.Open

' Copy Text stream to Binary stream, skiping the BOM
outfile.Position = 3
outfile.CopyTo outfile_noBOM
outfile.Close

' Write the Binary stream
outfile_noBOM.SaveToFile outputfile, 2
outfile_noBOM.Close
