Option Explicit

' $Id$
'
' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

' openttd_vs100.sln             is for MSVC 2010
' openttd_vs100.vcxproj         is for MSVC 2010
' openttd_vs100.vcxproj.filters is for MSVC 2010
' langs_vs100.vcxproj           is for MSVC 2010
' strgen_vs100.vcxproj          is for MSVC 2010
' strgen_vs100.vcxproj.filters  is for MSVC 2010
' generate_vs100.vcxproj        is for MSVC 2010
' version_vs100.vcxproj         is for MSVC 2010

' openttd_vs90.sln              is for MSVC 2008
' openttd_vs90.vcproj           is for MSVC 2008
' langs_vs90.vcproj             is for MSVC 2008
' strgen_vs90.vcproj            is for MSVC 2008
' generate_vs90.vcproj          is for MSVC 2008
' version_vs90.vcproj           is for MSVC 2008

' openttd_vs80.sln              is for MSVC 2005
' openttd_vs80.vcproj           is for MSVC 2005
' langs_vs80.vcproj             is for MSVC 2005
' strgen_vs80.vcproj            is for MSVC 2005
' generate_vs80.vcproj          is for MSVC 2005
' version_vs80.vcproj           is for MSVC 2005

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

Function load_main_data(filename, ByRef vcxproj, ByRef filters, ByRef files)
	Dim res, file, line, deep, skip, first_filter, first_file, filter, cltype, index
	res = ""
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
						line = "PNG" Or _
						line = "WIN32" Or _
						line = "MSVC" Or _
						line = "DIRECTMUSIC" Or _
						line = "AI" Or _
						line = "SSE" Or _
						line = "HAVE_THREAD" _
					) Then skip = skip + 1
					deep = deep + 1
				Case "#"
					if deep = skip Then
						line = Replace(line, "# ", "")
						if first_filter <> 0 Then
							res = res & "		</Filter>" & vbCrLf
							filters = filters & vbCrLf
						Else
							first_filter = 1
						End If
						filter = line
						res = res & _
						"		<Filter" & vbCrLf & _
						"			Name=" & Chr(34) & filter & Chr(34) & vbCrLf & _
						"			>" & vbCrLf
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
						res = res & _
						"			<File" & vbCrLf & _
						"				RelativePath=" & Chr(34) & ".\..\src\" & line & Chr(34) & vbCrLf & _
						"				>" & vbCrLf & _
						"			</File>" & vbCrLf
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
	res = res & "		</Filter>"
	file.Close()
	load_main_data = res
End Function

Function load_lang_data(dir, ByRef vcxproj, ByRef files)
	Dim res, folder, file, first_time
	res = ""
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		If file <> "english.txt" And FSO.GetExtensionName(file) = "txt" Then
			file = Left(file, Len(file) - 4)
			If first_time <> 0 Then
				res = res & vbCrLf
				vcxproj = vcxproj & vbCrLf
				files = files & vbCrLf
			Else
				first_time = 1
			End If
			res = res & _
			"			<File" & vbCrLf & _
			"				RelativePath=" & Chr(34) & "..\src\lang\" & file & ".txt" & Chr(34) & vbCrLf & _
			"				>" & vbCrLf & _
			"				<FileConfiguration" & vbCrLf & _
			"					Name=" & Chr(34) & "Debug|Win32" & Chr(34) & vbCrLf & _
			"					>" & vbCrLf & _
			"					<Tool" & vbCrLf & _
			"						Name=" & Chr(34) & "VCCustomBuildTool" & Chr(34) & vbCrLf & _
			"						Description=" & Chr(34) & "Generating " & file & " language file" & Chr(34) & vbCrLf & _
			"						CommandLine=" & Chr(34) & "..\objs\strgen\strgen.exe -s ..\src\lang -d ..\bin\lang &quot;$(InputPath)&quot;&#x0D;&#x0A;exit 0&#x0D;&#x0A;" & Chr(34) & vbCrLf & _
			"						AdditionalDependencies=" & Chr(34) & "..\src\lang\english.txt;..\objs\strgen\strgen.exe" & Chr(34) & vbCrLf & _
			"						Outputs=" & Chr(34) & "..\bin\lang\" & file & ".lng" & Chr(34) & vbCrLf & _
			"					/>" & vbCrLf & _
			"				</FileConfiguration>" & vbCrLf & _
			"			</File>"
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
	load_lang_data = res
End Function

Function load_settings_data(dir, ByRef vcxproj, ByRef command, ByRef files)
	Dim res, folder, file, first_time
	res = ""
	command = "..\objs\settings\settings_gen.exe -o ..\objs\settings\table\settings.h -b ..\src\table\settings.h.preamble -a ..\src\table\settings.h.postamble"
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		If FSO.GetExtensionName(file) = "ini" Then
			if first_time <> 0 Then
				res = res & vbCrLf
				vcxproj = vcxproj & vbCrLf
				files = files & vbCrLf
			Else
				first_time = 1
			End If
			res = res & _
			"		<File" & vbCrLf & _
			"			RelativePath=" & Chr(34) & "..\src\table\" & file & Chr(34) & vbCrLf & _
			"			>" & vbCrLf & _
			"		</File>"
			vcxproj = vcxproj & _
			"    <None Include=" & Chr(34) & "..\src\table\" & file & Chr(34) & " />"
			command = command & " ..\src\table\" & file
			files = files & _
			"    <None Include=" & Chr(34) & "..\src\table\" & file & Chr(34) & ">" & vbCrLf & _
			"      <Filter>INI</Filter>" & vbCrLf & _
			"    </None>"
		End If
	Next
	load_settings_data = res
End Function

Sub generate(data, dest, data2)
	Dim srcfile, destfile, line
	WScript.Echo "Generating " & FSO.GetFileName(dest) & "..."
	Set srcfile = FSO.OpenTextFile(dest & ".in", 1, 0, 0)
	Set destfile = FSO.CreateTextFile(dest, -1, 0)

	If Not IsNull(data2) Then
		' Everything above the !!FILTERS!! marker
		line = srcfile.ReadLine()
		While line <> "!!FILTERS!!"
			If len(line) > 0 Then destfile.WriteLine(line)
			line = srcfile.ReadLine()
		Wend

		' Our generated content
		destfile.WriteLine(data2)
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

Dim openttd, openttdvcxproj, openttdfilters, openttdfiles
openttd = load_main_data(ROOT_DIR & "/source.list", openttdvcxproj, openttdfilters, openttdfiles)
generate openttd, ROOT_DIR & "/projects/openttd_vs80.vcproj", Null
generate openttd, ROOT_DIR & "/projects/openttd_vs90.vcproj", Null
generate openttdvcxproj, ROOT_DIR & "/projects/openttd_vs100.vcxproj", Null
generate openttdfiles, ROOT_DIR & "/projects/openttd_vs100.vcxproj.filters", openttdfilters

Dim lang, langvcxproj, langfiles
lang = load_lang_data(ROOT_DIR & "/src/lang", langvcxproj, langfiles)
generate lang, ROOT_DIR & "/projects/langs_vs80.vcproj", Null
generate lang, ROOT_DIR & "/projects/langs_vs90.vcproj", Null
generate langvcxproj, ROOT_DIR & "/projects/langs_vs100.vcxproj", Null
generate langfiles, ROOT_DIR & "/projects/langs_vs100.vcxproj.filters", Null

Dim settings, settingsvcxproj, settingscommand, settingsfiles
settings = load_settings_data(ROOT_DIR & "/src/table", settingsvcxproj, settingscommand, settingsfiles)
generate settings, ROOT_DIR & "/projects/settings_vs80.vcproj", settingscommand
generate settings, ROOT_DIR & "/projects/settings_vs90.vcproj", settingscommand
generate settingsvcxproj, ROOT_DIR & "/projects/settings_vs100.vcxproj", settingscommand
generate settingsfiles, ROOT_DIR & "/projects/settings_vs100.vcxproj.filters", Null
