Option Explicit

' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

' openttd_vs142.sln             is for MSVC 2019
' openttd_vs142.vcxproj         is for MSVC 2019
' openttd_vs142.vcxproj.filters is for MSVC 2019
' langs_vs142.vcxproj           is for MSVC 2019
' strgen_vs142.vcxproj          is for MSVC 2019
' strgen_vs142.vcxproj.filters  is for MSVC 2019
' generate_vs142.vcxproj        is for MSVC 2019
' version_vs142.vcxproj         is for MSVC 2019
' basesets_vs142.vcxproj        is for MSVC 2019

' openttd_vs141.sln             is for MSVC 2017
' openttd_vs141.vcxproj         is for MSVC 2017
' openttd_vs141.vcxproj.filters is for MSVC 2017
' langs_vs141.vcxproj           is for MSVC 2017
' strgen_vs141.vcxproj          is for MSVC 2017
' strgen_vs141.vcxproj.filters  is for MSVC 2017
' generate_vs141.vcxproj        is for MSVC 2017
' version_vs141.vcxproj         is for MSVC 2017
' basesets_vs141.vcxproj        is for MSVC 2017

' openttd_vs140.sln             is for MSVC 2015
' openttd_vs140.vcxproj         is for MSVC 2015
' openttd_vs140.vcxproj.filters is for MSVC 2015
' langs_vs140.vcxproj           is for MSVC 2015
' strgen_vs140.vcxproj          is for MSVC 2015
' strgen_vs140.vcxproj.filters  is for MSVC 2015
' generate_vs140.vcxproj        is for MSVC 2015
' version_vs140.vcxproj         is for MSVC 2015
' basesets_vs140.vcxproj        is for MSVC 2015

Sub safety_check(filename)
	Dim file, line, regexp, list

	' Define regexp
	Set regexp = New RegExp
	regexp.Pattern = "#|ottdres.rc|win32.cpp|win32_v.cpp"
	regexp.Global = True

	' We use a dictionary to check duplicates
	Set list = CreateObject("Scripting.Dictionary")

	Set file = FSO.OpenTextFile(filename, 1, 0, 0)
	While Not file.AtEndOfStream
		line = Replace(file.ReadLine, Chr(9), "") ' Remove tabs
		If Len(line) > 0 And Not regexp.Test(line) Then
			line = FSO.GetFileName(line)
			if list.Exists(line) Then
				WScript.Echo " !! ERROR !!" _
				& vbCrLf & "" _
				& vbCrLf & "The filename '" & line & "' is already used in this project." _
				& vbCrLf & "Because MSVC uses one single directory for all object files, it" _
				& vbCrLf & "cannot handle filenames with the same name inside the same project." _
				& vbCrLf & "Please rename either one of the file and try generating again." _
				& vbCrLf & "" _
				& vbCrLf & " !! ERROR !!"
				WScript.Quit(1)
			End If
			list.Add line, line
		End If
	Wend
	file.Close
End Sub

Sub get_files(srcdir, dir, list)
	Dim file, filename
	Dim rekeep, reskip

	' pattern for files to keep
	Set rekeep = New RegExp
	rekeep.Pattern = "\.h(pp)?$"
	rekeep.Global = True

	' pattern for files to exclude
	Set reskip = New RegExp
	reskip.Pattern = "\.svn"
	reskip.Global = True

	For Each file in dir.Files
		filename = Replace(file.path, srcdir, "") ' Remove */src/
		filename = Replace(filename, "\", "/") ' Replace separators
		If rekeep.Test(filename) And Not reskip.Test(filename) Then
			list.Add filename, filename
		End If
	Next
End Sub

Sub get_dir_files(srcdir, dir, list)
	Dim folder
	' Get files
	get_files srcdir, dir, list

	' Recurse in subfolders
	For Each folder in dir.SubFolders
		get_dir_files srcdir, folder, list
	Next
End Sub

Sub headers_check(filename, dir)
	Dim source_list_headers, src_dir_headers, regexp, line, file, str

	' Define regexp for source.list parsing
	Set regexp = New RegExp
	regexp.Pattern = "\.h"
	regexp.Global = True

	' Parse source.list and store headers in a dictionary
	Set source_list_headers = CreateObject("Scripting.Dictionary")
	Set file = FSO.OpenTextFile(filename, 1, 0, 0)
	While Not file.AtEndOfStream
		line = Replace(file.ReadLine, Chr(9), "") ' Remove tabs
		If Len(line) > 0 And regexp.Test(line) And line <> "../objs/langs/table/strings.h" And line <> "../objs/settings/table/settings.h" Then
			source_list_headers.Add line, line
		End If
	Wend
	file.Close()

	' Get header files in /src/
	Set src_dir_headers = CreateObject("Scripting.Dictionary")
	get_dir_files dir, FSO.GetFolder(dir), src_dir_headers

	' Finding files in source.list but not in /src/
	For Each line In source_list_headers
		If Not src_dir_headers.Exists(line) Then
			str = str & "< " & line & vbCrLf
		End If
	Next

	' Finding files in /src/ but not in source.list
	For Each line In src_dir_headers
		If Not source_list_headers.Exists(line) Then
			str = str & "> " & line & vbCrLf
		End If
	Next

	' Display the missing files if any
	If str <> "" Then
		str = "The following headers are missing in source.list and not in /src/ or vice versa." _
		& vbCrLf & str
		WScript.Echo str
	End If
End Sub

Sub load_main_data(filename, ByRef vcxproj, ByRef filters, ByRef files)
	Dim file, line, deep, skip, first_filter, first_file, filter, cltype, index
	index = 0
	' Read the source.list and process it
	Set file = FSO.OpenTextFile(filename, 1, 0, 0)
	While Not file.AtEndOfStream
		line = Replace(file.ReadLine, Chr(9), "") ' Remove tabs
		If Len(line) > 0 Then
			Select Case Split(line, " ")(0)
				Case "#end"
					If deep = skip Then skip = skip - 1
					deep = deep - 1
				Case "#else"
					If deep = skip Then
						skip = skip - 1
					ElseIf deep - 1 = skip Then
						skip = skip + 1
					End If
				Case "#if"
					line = Replace(line, "#if ", "")
					If deep = skip And ( _
						line = "SDL" Or _
						line = "SDL2" Or _
						line = "PNG" Or _
						line = "WIN32" Or _
						line = "MSVC" Or _
						line = "DIRECTMUSIC" Or _
						line = "AI" Or _
						line = "USE_SSE" Or _
						line = "USE_XAUDIO2" Or _
						line = "USE_THREADS" _
					) Then skip = skip + 1
					deep = deep + 1
				Case "#"
					if deep = skip Then
						line = Replace(line, "# ", "")
						if first_filter <> 0 Then
							filters = filters & vbCrLf
						Else
							first_filter = 1
						End If
						filter = line
						filters = filters & _
						"    <Filter Include="& Chr(34) & filter & Chr(34) & ">" & vbCrLf & _
						"      <UniqueIdentifier>{c76ff9f1-1e62-46d8-8d55-" & String(12 - Len(CStr(index)), "0") & index & "}</UniqueIdentifier>" & vbCrLf & _
						"    </Filter>"
						index = index + 1
					End If
				Case Else
					If deep = skip Then
						line = Replace(line, "/" ,"\")
						if first_file <> 0 Then
							vcxproj = vcxproj & vbCrLf
							files = files & vbCrLf
						Else
							first_file = 1
						End If
						Select Case Split(Line, ".")(1)
							Case "cpp"
								cltype = "ClCompile"
							Case "rc"
								cltype = "ResourceCompile"
							Case Else
								cltype = "ClInclude"
						End Select
						vcxproj = vcxproj & "    <" & cltype & " Include="& Chr(34) & "..\src\" & line & Chr(34) & " />"
						files = files & _
						"    <" & cltype & " Include="& Chr(34) & "..\src\" & line & Chr(34) & ">" & vbCrLf & _
						"      <Filter>" & filter & "</Filter>" & vbCrLf & _
						"    </" & cltype & ">"
					End If
			End Select
		End If
	Wend
	file.Close()
End Sub

Sub load_lang_data(dir, ByRef vcxproj, ByRef files)
	Dim folder, file, first_time
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		If file <> "english.txt" And FSO.GetExtensionName(file) = "txt" Then
			file = Left(file, Len(file) - 4)
			If first_time <> 0 Then
				vcxproj = vcxproj & vbCrLf
				files = files & vbCrLf
			Else
				first_time = 1
			End If
			vcxproj = vcxproj & _
			"    <CustomBuild Include=" & Chr(34) & "..\src\lang\" & file & ".txt" & Chr(34) & ">" & vbCrLf & _
			"      <Message Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">Generating " & file & " language file</Message>" & vbCrLf & _
			"      <Command Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">..\objs\strgen\strgen.exe -s ..\src\lang -d ..\bin\lang " & Chr(34) & "%(FullPath)" & Chr(34) & "</Command>" & vbCrLf & _
			"      <AdditionalInputs Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">..\src\lang\english.txt;..\objs\strgen\strgen.exe;%(AdditionalInputs)</AdditionalInputs>" & vbCrLf & _
			"      <Outputs Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">..\bin\lang\" & file & ".lng;%(Outputs)</Outputs>" & vbCrLf & _
			"    </CustomBuild>"
			files = files & _
			"    <CustomBuild Include=" & Chr(34) & "..\src\lang\" & file & ".txt" & Chr(34) & ">" & vbCrLf & _
			"      <Filter>Translations</Filter>" & vbCrLf & _
			"    </CustomBuild>"
		End If
	Next
End Sub

Sub load_settings_data(dir, ByRef vcxproj, ByRef command, ByRef files)
	Dim folder, file, first_time
	command = "..\objs\settings\settings_gen.exe -o ..\objs\settings\table\settings.h -b ..\src\table\settings.h.preamble -a ..\src\table\settings.h.postamble"
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		If FSO.GetExtensionName(file) = "ini" Then
			if first_time <> 0 Then
				vcxproj = vcxproj & vbCrLf
				files = files & vbCrLf
			Else
				first_time = 1
			End If
			vcxproj = vcxproj & _
			"    <None Include=" & Chr(34) & "..\src\table\" & file & Chr(34) & " />"
			command = command & " ..\src\table\" & file
			files = files & _
			"    <None Include=" & Chr(34) & "..\src\table\" & file & Chr(34) & ">" & vbCrLf & _
			"      <Filter>INI</Filter>" & vbCrLf & _
			"    </None>"
		End If
	Next
End Sub

Sub load_baseset_data(dir, langdir, ByRef vcxproj, ByRef files, ByRef langs)
	Dim folder, file, ext, first_time
	Set folder = FSO.GetFolder(langdir)
	langs = "    <Langs>"
	For Each file In folder.Files
		If first_time <> 0 Then
			langs = langs & ";"
		Else
			first_time = 1
		End If
		file = FSO.GetFileName(file)
		ext = FSO.GetExtensionName(file)
		langs = langs & "..\src\lang\" & file
	Next
	langs = langs & "</Langs>"
	first_time = 0
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		ext = FSO.GetExtensionName(file)
		If ext = "obm" Or ext = "obg" Or ext = "obs" Then
			If first_time <> 0 Then
				vcxproj = vcxproj & vbCrLf
				files = files & vbCrLf
			Else
				first_time = 1
			End If
			vcxproj = vcxproj & _
			"    <CustomBuild Include=" & Chr(34) & "..\media\baseset\" & file & Chr(34) & ">" & vbCrLf & _
			"      <Message Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">Generating " & file & " baseset metadata file</Message>" & vbCrLf & _
			"      <Command Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">cscript //nologo ..\media\baseset\translations.vbs " & Chr(34) & "%(FullPath)" & Chr(34) & " " & Chr(34) & "$(OutputPath)" & file & Chr(34) & " ..\src\lang ..\bin\baseset\orig_extra.grf</Command>" & vbCrLf & _
			"      <AdditionalInputs Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">$(Langs);..\bin\baseset\orig_extra.grf;%(AdditionalInputs)</AdditionalInputs>" & vbCrLf & _
			"      <Outputs Condition=" & Chr(34) & "'$(Configuration)|$(Platform)'=='Debug|Win32'" & Chr(34) & ">..\bin\baseset\" & file & ";%(Outputs)</Outputs>" & vbCrLf & _
			"    </CustomBuild>"
			files = files & _
			"    <CustomBuild Include=" & Chr(34) & "..\media\baseset\" & file & Chr(34) & ">" & vbCrLf & _
			"      <Filter>Baseset Metadata</Filter>" & vbCrLf & _
			"    </CustomBuild>"
		End If
	Next
End Sub

Sub generate(data, dest, data2)
	Dim srcfile, destfile, line, regexp
	WScript.Echo "Generating " & FSO.GetFileName(dest) & "..."
	Set srcfile = FSO.OpenTextFile(dest & ".in", 1, 0, 0)
	Set destfile = FSO.CreateTextFile(dest, -1, 0)

	If Not IsNull(data2) Then
		' Everything above the !!FILTERS!! marker
		Set regexp = New RegExp
		regexp.Pattern = "!!FILTERS!!"
		regexp.Global = True

		line = srcfile.ReadLine()
		While Not regexp.Test(line)
			If len(line) > 0 Then destfile.WriteLine(line)
			line = srcfile.ReadLine()
		Wend

		' Our generated content
		line = regexp.Replace(line, data2)
		destfile.WriteLine(line)
	End If

	' Everything above the !!FILES!! marker
	line = srcfile.ReadLine()
	While line <> "!!FILES!!"
		If len(line) > 0 Then destfile.WriteLine(line)
		line = srcfile.ReadLine()
	Wend

	' Our generated content
	destfile.WriteLine(data)

	' Everything below the !!FILES!! marker
	While Not srcfile.AtEndOfStream
		line = srcfile.ReadLine()
		If len(line) > 0 Then destfile.WriteLine(line)
	Wend
	srcfile.Close()
	destfile.Close()
End Sub

Dim ROOT_DIR
ROOT_DIR = FSO.GetFolder("..").Path
If Not FSO.FileExists(ROOT_DIR & "/source.list") Then
	ROOT_DIR = FSO.GetFolder(".").Path
End If
If Not FSO.FileExists(ROOT_DIR & "/source.list") Then
	WScript.Echo "Can't find source.list, needed in order to make this run." _
	& vbCrLf & "Please go to either the project dir, or the root dir of a clean SVN checkout."
	WScript.Quit(1)
End If

safety_check ROOT_DIR & "/source.list"
headers_check ROOT_DIR & "/source.list", ROOT_DIR & "\src\" ' Backslashes needed for DoFiles

Dim openttdvcxproj, openttdfilters, openttdfiles
load_main_data ROOT_DIR & "/source.list", openttdvcxproj, openttdfilters, openttdfiles
generate openttdvcxproj, ROOT_DIR & "/projects/openttd_vs140.vcxproj", Null
generate openttdfiles, ROOT_DIR & "/projects/openttd_vs140.vcxproj.filters", openttdfilters
generate openttdvcxproj, ROOT_DIR & "/projects/openttd_vs141.vcxproj", Null
generate openttdfiles, ROOT_DIR & "/projects/openttd_vs141.vcxproj.filters", openttdfilters
generate openttdvcxproj, ROOT_DIR & "/projects/openttd_vs142.vcxproj", Null
generate openttdfiles, ROOT_DIR & "/projects/openttd_vs142.vcxproj.filters", openttdfilters

Dim langvcxproj, langfiles
load_lang_data ROOT_DIR & "/src/lang", langvcxproj, langfiles
generate langvcxproj, ROOT_DIR & "/projects/langs_vs140.vcxproj", Null
generate langfiles, ROOT_DIR & "/projects/langs_vs140.vcxproj.filters", Null
generate langvcxproj, ROOT_DIR & "/projects/langs_vs141.vcxproj", Null
generate langfiles, ROOT_DIR & "/projects/langs_vs141.vcxproj.filters", Null
generate langvcxproj, ROOT_DIR & "/projects/langs_vs142.vcxproj", Null
generate langfiles, ROOT_DIR & "/projects/langs_vs142.vcxproj.filters", Null

Dim settingsvcxproj, settingscommand, settingsfiles
load_settings_data ROOT_DIR & "/src/table", settingsvcxproj, settingscommand, settingsfiles
generate settingsvcxproj, ROOT_DIR & "/projects/settings_vs140.vcxproj", settingscommand
generate settingsfiles, ROOT_DIR & "/projects/settings_vs140.vcxproj.filters", Null
generate settingsvcxproj, ROOT_DIR & "/projects/settings_vs141.vcxproj", settingscommand
generate settingsfiles, ROOT_DIR & "/projects/settings_vs141.vcxproj.filters", Null
generate settingsvcxproj, ROOT_DIR & "/projects/settings_vs142.vcxproj", settingscommand
generate settingsfiles, ROOT_DIR & "/projects/settings_vs142.vcxproj.filters", Null

Dim basesetvcxproj, basesetfiles, basesetlangs
load_baseset_data ROOT_DIR & "/media/baseset", ROOT_DIR & "/src/lang", basesetvcxproj, basesetfiles, basesetlangs
generate basesetvcxproj, ROOT_DIR & "/projects/basesets_vs140.vcxproj", basesetlangs
generate basesetfiles, ROOT_DIR & "/projects/basesets_vs140.vcxproj.filters", Null
generate basesetvcxproj, ROOT_DIR & "/projects/basesets_vs141.vcxproj", basesetlangs
generate basesetfiles, ROOT_DIR & "/projects/basesets_vs141.vcxproj.filters", Null
generate basesetvcxproj, ROOT_DIR & "/projects/basesets_vs142.vcxproj", basesetlangs
generate basesetfiles, ROOT_DIR & "/projects/basesets_vs142.vcxproj.filters", Null
