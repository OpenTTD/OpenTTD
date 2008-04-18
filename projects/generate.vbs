Option Explicit

Dim FSO
Set FSO = CreateObject("Scripting.FileSystemObject")

' openttd_vs90.sln    is for MSVC 2008
' openttd_vs90.vcproj is for MSVC 2008
' langs_vs90.vcproj   is for MSVC 2008
' strgen_vs90.vcproj  is for MSVC 2008

' openttd_vs80.sln    is for MSVC 2005
' openttd_vs80.vcproj is for MSVC 2005
' langs_vs80.vcproj   is for MSVC 2005
' strgen_vs80.vcproj  is for MSVC 2005

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
	rekeep.Pattern = "\.h"
	rekeep.Global = True

	' pattern for files to exclude
	Set reskip = New RegExp
	reskip.Pattern = "\.svn|\.hpp\.sq"
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
		If Len(line) > 0 And regexp.Test(line) And line <> "../objs/langs/table/strings.h" Then
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

Function load_main_data(filename)
	Dim res, file, line, deep, skip, first_time
	res = ""
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
						line = "HAVE_THREAD" _
					) Then skip = skip + 1
					deep = deep + 1
				Case "#"
					if deep = skip Then
						line = Replace(line, "# ", "")
						if first_time <> 0 Then
							res = res & "		</Filter>" & vbCrLf
						Else
							first_time = 1
						End If
						res = res & _
						"		<Filter" & vbCrLf & _
						"			Name=" & Chr(34) & line & Chr(34) & vbCrLf & _
						"			>" & vbCrLf
					End If
				Case Else
					If deep = skip Then
						line = Replace(line, "/" ,"\")
						res = res & _
						"			<File" & vbCrLf & _
						"				RelativePath=" & Chr(34) & ".\..\src\" & line & Chr(34) & vbCrLf & _
						"				>" & vbCrLf & _
						"			</File>" & vbCrLf
					End If
			End Select
		End If
	Wend
	res = res & "		</Filter>"
	file.Close()
	load_main_data = res
End Function

Function load_lang_data(dir)
	Dim res, folder, file
	res = ""
	Set folder = FSO.GetFolder(dir)
	For Each file In folder.Files
		file = FSO.GetFileName(file)
		If FSO.GetExtensionName(file) = "txt" Then
			file = Left(file, Len(file) - 4)
			res = res _
			& vbCrLf & "		<File" _
			& vbCrLf & "			RelativePath=" & Chr(34) & "..\src\lang\" & file & ".txt" & Chr(34) _
			& vbCrLf & "			>" _
			& vbCrLf & "			<FileConfiguration" _
			& vbCrLf & "				Name=" & Chr(34) & "Debug|Win32" & Chr(34) _
			& vbCrLf & "				>" _
			& vbCrLf & "				<Tool" _
			& vbCrLf & "					Name=" & Chr(34) & "VCCustomBuildTool" & Chr(34) _
			& vbCrLf & "					Description=" & Chr(34) & "Generating " & file & " language file" & Chr(34) _
			& vbCrLf & "					CommandLine=" & Chr(34) & "..\objs\strgen\strgen.exe -s ..\src\lang -d ..\bin\lang &quot;$(InputPath)&quot;&#x0D;&#x0A;" & Chr(34) _
			& vbCrLf & "					AdditionalDependencies=" & Chr(34) & Chr(34) _
			& vbCrLf & "					Outputs=" & Chr(34) & "..\bin\lang\" & file & ".lng" & Chr(34) _
			& vbCrLf & "				/>" _
			& vbCrLf & "			</FileConfiguration>" _
			& vbCrLf & "		</File>"
		End If
	Next
	load_lang_data = res
End Function

Sub generate(data, dest)
	Dim srcfile, destfile, line
	WScript.Echo "Generating " & FSO.GetFileName(dest) & "..."
	Set srcfile = FSO.OpenTextFile(dest & ".in", 1, 0, 0)
	Set destfile = FSO.CreateTextFile(dest, -1, 0)

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

Dim openttd
openttd = load_main_data(ROOT_DIR &"/source.list")
generate openttd, ROOT_DIR & "/projects/openttd_vs80.vcproj"
generate openttd, ROOT_DIR & "/projects/openttd_vs90.vcproj"

Dim lang
lang = load_lang_data(ROOT_DIR & "/src/lang")
generate lang, ROOT_DIR & "/projects/langs_vs80.vcproj"
generate lang, ROOT_DIR & "/projects/langs_vs90.vcproj"
