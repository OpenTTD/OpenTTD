Option Explicit

' This file is part of OpenTTD.
' OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
' OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
' See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

Dim FSO
Dim enum_size, enums, enum_value_size, enum_value
Dim enum_string_to_error_size, enum_string_to_error_mapping_string, enum_string_to_error_mapping_error
Dim enum_error_to_string_size, enum_error_to_string_mapping, const_size, const_value
Dim struct_size, structs, method_size, methods, static_method_size, static_methods
Dim super_cls, cls, api_selected, cls_in_api, start_squirrel_define_on_next_line, has_fileheader, cls_level
Dim apis, filename, doxygen_skip, squirrel_stuff, is_public, cls_param(2), comment, in_enum

Set FSO = CreateObject("Scripting.FileSystemObject")

Function CompareFiles(filename1, filename2)
	Dim file, lines1, lines2

	If Not FSO.FileExists(filename1) Then
		CompareFiles = False
		Exit Function
	End If
	Set file = FSO.OpenTextFile(filename1, 1)
	If Not file.AtEndOfStream Then
		lines1 = file.ReadAll
	End IF
	file.Close

	If Not FSO.FileExists(filename2) Then
		CompareFiles = False
		Exit Function
	End If
	Set file = FSO.OpenTextFile(filename2, 1)
	If Not file.AtEndOfStream Then
		lines2 = file.ReadAll
	End IF
	file.Close

	CompareFiles = (lines1 = lines2)
End Function

Function IsEmptyFile(filename)
	Dim file
	Set file = FSO.OpenTextFile(filename, 1)
	IsEmptyFile = file.AtEndOfStream
	file.Close
End Function

Function DumpClassTemplates(name, file)
	Dim re, realname
	Set re = New RegExp

	re.Pattern = "^Script"
	realname = re.Replace(name, "")

	file.WriteLine "	template <> inline "       & name & " *GetParam(ForceType<"       & name & " *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return  (" & name & " *)instance; }"
	file.WriteLine "	template <> inline "       & name & " &GetParam(ForceType<"       & name & " &>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return *(" & name & " *)instance; }"
	file.WriteLine "	template <> inline const " & name & " *GetParam(ForceType<const " & name & " *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return  (" & name & " *)instance; }"
	file.WriteLine "	template <> inline const " & name & " &GetParam(ForceType<const " & name & " &>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return *(" & name & " *)instance; }"
	If name = "ScriptEvent" Then
		file.WriteLine "	template <> inline int Return<" & name & " *>(HSQUIRRELVM vm, " & name & " *res) { if (res == nullptr) { sq_pushnull(vm); return 1; } Squirrel::CreateClassInstanceVM(vm, " & Chr(34) & realname & Chr(34) & ", res, nullptr, DefSQDestructorCallback<" & name & ">, true); return 1; }"
	ElseIf name = "ScriptText" Then
		file.WriteLine ""
		file.WriteLine "	template <> inline Text *GetParam(ForceType<Text *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) {"
		file.WriteLine "		if (sq_gettype(vm, index) == OT_INSTANCE) {"
		file.WriteLine "			return GetParam(ForceType<ScriptText *>(), vm, index, ptr);"
		file.WriteLine "		}"
		file.WriteLine "		if (sq_gettype(vm, index) == OT_STRING) {"
		file.WriteLine "			return new RawText(GetParam(ForceType<const char *>(), vm, index, ptr));"
		file.WriteLine "		}"
		file.WriteLine "		return nullptr;"
		file.WriteLine "	}"
	Else
		file.WriteLine "	template <> inline int Return<" & name & " *>(HSQUIRRELVM vm, " & name & " *res) { if (res == nullptr) { sq_pushnull(vm); return 1; } res->AddRef(); Squirrel::CreateClassInstanceVM(vm, " & Chr(34) & realname & Chr(34) & ", res, nullptr, DefSQDestructorCallback<" & name & ">, true); return 1; }"
	End If
End Function

Function DumpFileheader(api, file)
	Dim re
	Set re = New RegExp
	file.WriteLine "/*"
	file.WriteLine " * This file is part of OpenTTD."
	file.WriteLine " * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2."
	file.WriteLine " * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."
	file.WriteLine " * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>."
	file.WriteLine " */"
	file.WriteLine ""
	file.WriteLine "/* THIS FILE IS AUTO-GENERATED; PLEASE DO NOT ALTER MANUALLY */"
	file.WriteLine ""
	file.WriteLine "#include " & Chr(34) & "../" & filename & Chr(34)
	If api <> "Template" Then
		re.Pattern = "script_"
		filename = re.Replace(filename, "template_")
		file.WriteLine "#include " & Chr(34) & "../template/" & filename & ".sq" & Chr(34)
	End If
End Function

Function ResetReader()
	enum_size = 0
	enums.RemoveAll
	enum_value_size = 0
	enum_value.RemoveAll
	enum_string_to_error_size = 0
	enum_string_to_error_mapping_string.RemoveAll
	enum_string_to_error_mapping_error.RemoveAll
	enum_error_to_string_size = 0
	enum_error_to_string_mapping.RemoveAll
	const_size = 0
	const_value.RemoveAll
	struct_size = 0
	structs.RemoveAll
	method_size = 0
	methods.RemoveAll
	static_method_size = 0
	static_methods.RemoveAll
	cls = ""
	start_squirrel_define_on_next_line = False
	cls_level = 0
	cls_in_api = ""
End Function

Sub SquirrelExportParse(api, line, file)
	Dim re
	Set re = New RegExp

	re.Pattern = "@file"
	If re.Test(line) Then
		filename = Split(line)(2)
		re.Pattern = "^" & apis & "_"
		filename = re.Replace(filename, "script_")
	End If

	' Ignore special doxygen blocks
	re.Pattern = "^#ifndef DOXYGEN_API"
	If re.Test(line) Then
		doxygen_skip = "next"
		Exit Sub
	End If
	re.Pattern = "^#ifdef DOXYGEN_API"
	If re.Test(line) Then
		doxygen_skip = "true"
		Exit Sub
	End If
	re.Pattern = "^#endif /\* DOXYGEN_API \*/"
	If re.Test(line) Then
		doxygen_skip = "false"
		Exit Sub
	End If
	re.Pattern = "^#else"
	If re.Test(line) Then
		If doxygen_skip = "next" Then
			doxygen_skip = "true"
		Else
			doxygen_skip = "false"
		End If
		Exit Sub
	End If
	If doxygen_skip = "true" Then Exit Sub

	re.Pattern = "^([	 ]*)\* @api"
	If re.Test(line) Then
		' By default, classes are not selected
		If cls_level = 0 Then api_selected = "false"

		re.Pattern = "^([	 ]*)"
		line = re.Replace(line, "")
		re.Pattern = "\* @api "
		line = re.Replace(line, "")

		If api = "Template" Then
			api_selected = "true"
			If line = "none" Or line = "-all" Then api_selected = "false"
			Exit Sub
		End If

		If line = "none" Then
			api_selected = "false"
		ElseIf line = "-all" Then
			api_selected = "false"
		Else
			re.Pattern = "-" & apis
			If re.Test(line) Then
				api_selected = "false"
			Else
				re.Pattern = apis
				If re.Test(line) Then
					api_selected = "true"
				End If
			End If
		End If
		Exit Sub
	End If

	' Remove the old squirrel stuff
	re.Pattern = "#ifdef DEFINE_SQUIRREL_CLASS"
	If re.Test(line) Then
		squirrel_stuff = True
		Exit Sub
	End If
	re.Pattern = "^#endif /\* DEFINE_SQUIRREL_CLASS \*/"
	If re.Test(line) Then
		If squirrel_stuff Then squirrel_stuff = False
		Exit Sub
	End If
	If squirrel_stuff Then Exit Sub

	' Ignore forward declarations of classes
	re.Pattern = "^(	*)class(.*);"
	If re.Test(line) Then Exit Sub
	' We only want to have public functions exported for now
	re.Pattern = "^(	*)class"
	If re.Test(line) Then
		line = Split(line)
		If cls_level = 0 Then
			If api_selected = "" Then
				WScript.Echo "Class '" & line(1) & "' has no @api. It won't be published to any API."
				api_selected = "false"
			End If
			is_public = False
			cls_param(0) = ""
			cls_param(1) = 1
			cls_param(2) = "x"
			cls_in_api = api_selected
			api_selected = ""
			cls = line(1)
			re.Pattern = "public|protected|private"
			If UBound(line) > 2 Then
				If re.Test(line(3)) Then
					super_cls = line(4)
				Else
					super_cls = line(3)
				End If
			End If
		ElseIf cls_level = 1 Then
			If api_selected = "" Then api_selected = cls_in_api

			If api_selected = "true" Then
				struct_size = struct_size + 1
				structs.Item(struct_size) = cls & "::" & line(1)
			End If
			api_selected = ""
		End If
		cls_level = cls_level + 1
		Exit Sub
	End If
	re.Pattern = "^(	*)public"
	If re.Test(line) Then
		If cls_level = 1 Then is_public = True
		Exit Sub
	End If
	re.Pattern = "^(	*)protected"
	If re.Test(line) Then
		If cls_level = 1 Then is_public = False
		Exit Sub
	End If
	re.Pattern = "^(	*)private"
	If re.Test(line) Then
		If cls_level = 1 Then is_public = False
		Exit Sub
	End If

	' Ignore the comments
	re.Pattern = "^#"
	If re.Test(line) Then Exit Sub
	re.Pattern = "/\*.*\*/"
	If re.Test(line) Then
		comment = False
		Exit Sub
	End If
	re.Pattern = "/\*"
	If re.Test(line) Then
		comment = True
		Exit Sub
	End If
	re.Pattern = "\*/"
	If re.Test(line) Then
		comment = False
		Exit Sub
	End If
	If comment Then Exit Sub

	' We need to make specialized conversions for structs
	re.Pattern = "^(	*)struct"
	If re.Test(line) Then
		cls_level = cls_level + 1

		' Check if we want to publish this struct
		If api_selected = "" Then api_selected = cls_in_api
		If api_selected = "false" Then
			api_selected = ""
			Exit Sub
		End If
		api_selected = ""

		If Not is_public Then Exit Sub
		If cls_level <> 1 Then Exit Sub

		struct_size = struct_size + 1
		structs.Item(struct_size) = cls & "::" & Split(line)(1)
		Exit Sub
	End If

	' We need to make specialized conversions for enums
	re.Pattern = "^(	*)enum"
	If re.Test(line) Then
		cls_level = cls_level + 1

		' Check if we want to publish this enum
		If api_selected = "" Then api_selected = cls_in_api
		If api_selected = "false" Then
			api_selected = ""
			Exit Sub
		End If
		api_selected = ""

		If Not is_public Then Exit Sub

		in_enum = True
		enum_size = enum_size + 1
		enums.Item(enum_size) = cls & "::" & Split(line)(1)
		Exit Sub
	End If

	' Maybe the end of the class, if so we can start with the Squirrel export pretty soon
	re.Pattern = "};"
	If re.Test(line) Then
		cls_level = cls_level - 1
		If cls_level <> 0 Then
			in_enum = False
			Exit Sub
		End If

		If cls = "" Then Exit Sub
		start_squirrel_define_on_next_line = True
		Exit Sub
	End If

	' Empty/white lines. When we may do the Squirrel export, do that export.
	re.Pattern = "^([ 	]*)$"
	If re.Test(line) Then
		Dim namespace_opened, api_cls, api_super_cls, i, mlen, spaces

		If Not start_squirrel_define_on_next_line Then Exit Sub

		If cls_in_api <> "true" Then
			ResetReader
			Exit Sub
		End If
		If Not has_fileheader Then
			DumpFileHeader api, file
			has_fileheader = True
		End If

		is_public = False
		namespace_opened = False

		re.Pattern = "^Script"
		api_cls = re.Replace(cls, api)
		api_super_cls = re.Replace(super_cls, api)

		file.WriteLine ""

		If api = "Template" Then
			' First check whether we have enums to print
			If enum_size <> 0 Then
				If Not namespace_opened Then
					file.WriteLine "namespace SQConvert {"
					namespace_opened = True
				End If
				file.WriteLine "	/* Allow enums to be used as Squirrel parameters */"
				For i = 1 To enum_size
					file.WriteLine "	template <> inline " & enums.Item(i) & " GetParam(ForceType<" & enums.Item(i) & ">, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger tmp; sq_getinteger(vm, index, &tmp); return (" & enums.Item(i) & ")tmp; }"
					file.WriteLine "	template <> inline int Return<" & enums.Item(i) & ">(HSQUIRRELVM vm, " & enums.Item(i) & " res) { sq_pushinteger(vm, (int32)res); return 1; }"
				Next
			End If

			' Then check whether we have structs/classes to print
			If struct_size <> 0 Then
				If Not namespace_opened Then
					file.WriteLine "namespace SQConvert {"
					namespace_opened = True
				End If
				file.WriteLine "	/* Allow inner classes/structs to be used as Squirrel parameters */"
				For i = 1 To struct_size
					DumpClassTemplates structs.Item(i), file
				Next
			End If

			If Not namespace_opened Then
				file.WriteLine "namespace SQConvert {"
				namespace_opened = True
			Else
				file.WriteLine ""
			End If
			file.WriteLine "	/* Allow " & cls & " to be used as Squirrel parameter */"
			DumpClassTemplates cls, file

			file.WriteLine "} // namespace SQConvert"

			ResetReader
			Exit Sub
		End If

		file.WriteLine ""
		file.WriteLine "template <> const char *GetClassName<" & cls & ", ST_" & UCase(api) & ">() { return " & Chr(34) & api_cls & Chr(34) & "; }"
		file.WriteLine ""

		' Then do the registration functions of the class.
		file.WriteLine "void SQ" & api_cls & "_Register(Squirrel *engine)"
		file.WriteLine "{"
		file.WriteLine "	DefSQClass<" & cls & ", ST_" & UCase(api) & "> SQ" & api_cls & "(" & Chr(34) & api_cls & Chr(34) & ");"
		If super_cls = "Text" Or super_cls = "ScriptObject" Or super_cls = "AIAbstractiveList::Valuator" Then
			file.WriteLine "	SQ" & api_cls & ".PreRegister(engine);"
		Else
			file.WriteLine "	SQ" & api_cls & ".PreRegister(engine, " & Chr(34) & api_super_cls & Chr(34) & ");"
		End If
		If super_cls <> "ScriptEvent" Then
			If cls_param(2) = "v" Then
				file.WriteLine "	SQ" & api_cls & ".AddSQAdvancedConstructor(engine);"
			Else
				file.WriteLine "	SQ" & api_cls & ".AddConstructor<void (" & cls & "::*)(" & cls_param(0) & "), " & cls_param(1) & ">(engine, " & Chr(34) & cls_param(2) & Chr(34) & ");"
			End If
		End If
		file.WriteLine ""

		' Enum values
		mlen = 0
		For i = 1 To enum_value_size
			If mlen <= Len(enum_value.Item(i)) Then mlen = Len(enum_value.Item(i))
		Next
		For i = 1 To enum_value_size
			file.WriteLine "	SQ" & api_cls & ".DefSQConst(engine, " & cls & "::" & enum_value.Item(i) & ", " & Space(mlen - Len(enum_value.Item(i))) & Chr(34) & enum_value.Item(i) & Chr(34) & ");"
		Next
		If enum_value_size <> 0 Then file.WriteLine ""

		' Const values
		mlen = 0
		For i = 1 To const_size
			If mlen <= Len(const_value.Item(i)) Then mlen = Len(const_value.Item(i))
		Next
		For i = 1 To const_size
			file.WriteLine "	SQ" & api_cls & ".DefSQConst(engine, " & cls & "::" & const_value.Item(i) & ", " & Space(mlen - Len(const_value.Item(i))) & Chr(34) & const_value.Item(i) & Chr(34) & ");"
		Next
		If const_size <> 0 Then file.WriteLine ""

		' Mapping of OTTD strings to errors
		mlen = 0
		For i = 1 To enum_string_to_error_size
			If mlen <= Len(enum_string_to_error_mapping_string.Item(i)) Then mlen = Len(enum_string_to_error_mapping_string.Item(i))
		Next
		For i = 1 To enum_string_to_error_size
			file.WriteLine "	ScriptError::RegisterErrorMap(" & enum_string_to_error_mapping_string.Item(i) & ", " & Space(mlen - Len(enum_string_to_error_mapping_string.Item(i))) & cls & "::" & enum_string_to_error_mapping_error.Item(i) & ");"
		Next
		If enum_string_to_error_size <> 0 Then file.WriteLine ""

		' Mapping of errors to human 'readable' strings.
		mlen = 0
		For i = 1 To enum_error_to_string_size
			If mlen <= Len(enum_error_to_string_mapping.Item(i)) Then mlen = Len(enum_error_to_string_mapping.Item(i))
		Next
		For i = 1 To enum_error_to_string_size
			file.WriteLine "	ScriptError::RegisterErrorMapString(" & cls & "::" & enum_error_to_string_mapping.Item(i) & ", " & Space(mlen - Len(enum_error_to_string_mapping.Item(i))) & Chr(34) & enum_error_to_string_mapping.Item(i) & Chr(34) & ");"
		Next
		If enum_error_to_string_size <> 0 Then file.WriteLine ""

		' Static methods
		mlen = 0
		For i = 1 To static_method_size
			If mlen <= Len(static_methods.Item(i)(0)) Then mlen = Len(static_methods.Item(i)(0))
		Next
		For i = 1 To static_method_size
			If static_methods.Item(i)(2) = "v" Then
				spaces = mlen - Len(static_methods.Item(i)(0)) - 8
				If spaces < 0 Then spaces = 0
				file.WriteLine "	SQ" & api_cls & ".DefSQAdvancedStaticMethod(engine, &" & cls & "::" & static_methods.Item(i)(0) & ", " & Space(spaces) & Chr(34) & static_methods.Item(i)(0) & Chr(34) & ");"
			Else
				file.WriteLine "	SQ" & api_cls & ".DefSQStaticMethod(engine, &" & cls & "::" & static_methods.Item(i)(0) & ", " & Space(mlen - Len(static_methods.Item(i)(0))) & Chr(34) & static_methods.Item(i)(0) & Chr(34) & ", " & Space(mlen - Len(static_methods.Item(i)(0))) & static_methods.Item(i)(1) & ", " & Chr(34) & static_methods.Item(i)(2) & Chr(34) & ");"
			End If
		Next
		If static_method_size <> 0 Then file.WriteLine ""

		' Non-static methods
		mlen = 0
		For i = 1 To method_size
			If mlen <= Len(methods.Item(i)(0)) Then mlen = Len(methods.Item(i)(0))
		Next
		For i = 1 To method_size
			If methods.Item(i)(2) = "v" Then
				spaces = mlen - Len(methods.Item(i)(0)) - 8
				If spaces < 0 Then spaces = 0
				file.WriteLine "	SQ" & api_cls & ".DefSQAdvancedMethod(engine, &" & cls & "::" & methods.Item(i)(0) & ", " & Space(spaces) & Chr(34) & methods.Item(i)(0) & Chr(34) & ");"
			Else
				file.WriteLine "	SQ" & api_cls & ".DefSQMethod(engine, &" & cls & "::" & methods.Item(i)(0) & ", " & Space(mlen - Len(methods.Item(i)(0))) & Chr(34) & methods.Item(i)(0) & Chr(34) & ", " & Space(mlen - Len(methods.Item(i)(0))) & methods.Item(i)(1) & ", " & Chr(34) & methods.Item(i)(2) & Chr(34) & ");"
			End If
		Next
		If method_size <> 0 Then file.WriteLine ""

		file.WriteLine "	SQ" & api_cls & ".PostRegister(engine);"
		file.WriteLine "}"

		ResetReader

		Exit Sub
	End If

	' Skip non-public functions
	If Not is_public Then Exit Sub

	' Add enums
	If in_enum Then
		enum_value_size = enum_value_size + 1
		re.Pattern = "[, 	]"
		re.Global = True
		enum_value.Item(enum_value_size) = re.Replace(split(line)(0), "")

		' Check if this a special error enum
		re.Pattern = ".*::ErrorMessages"
		If re.Test(enums.Item(enum_size)) Then
			' syntax:
			' enum ErrorMessages {
			'	ERR_SOME_ERROR,	// [STR_ITEM1, STR_ITEM2, ...]
			' }

			' Set the mappings
			re.Pattern = "\[.*\]"
			If re.Test(line) Then
				Dim mappings
				mappings = re.Execute(line)(0).Value
				re.Pattern = "[\[ 	\]]"
				mappings = re.Replace(mappings, "")

				mappings = Split(mappings, ",")
				For i = LBound(mappings) To UBound(mappings)
					enum_string_to_error_size = enum_string_to_error_size + 1
					enum_string_to_error_mapping_string.Item(enum_string_to_error_size) = mappings(i)
					enum_string_to_error_mapping_error.Item(enum_string_to_error_size) = enum_value.Item(enum_value_size)
				Next

				enum_error_to_string_size = enum_error_to_string_size + 1
				enum_error_to_string_mapping.Item(enum_error_to_string_size) = enum_value.Item(enum_value_size)
			End If
		End If
		re.Global = False
		Exit Sub
	End If

	' Add a const (non-enum) value
	re.Pattern = "^[ 	]*static const \w+ \w+ = -?\(?\w*\)?\w+;"
	If re.Test(line) Then
		const_size = const_size + 1
		const_value.Item(const_size) = Split(line)(3)
		Exit Sub
	End If

	' Add a method to the list
	re.Pattern = "^.*\(.*\).*$"
	If re.Test(line) Then
		Dim is_static, param_s, func, funcname, params, types
		If cls_level <> 1 Then Exit Sub
		re.Pattern = "~"
		If re.Test(line) Then
			If api_selected <> "" Then
				WScript.Echo "Destructor for '" & cls & "' has @api. Tag ignored."
				api_selected = ""
			End If
			Exit Sub
		End If

		re.Pattern = "static"
		is_static = re.Test(line)
		re.Pattern = "\bvirtual\b"
		line = re.Replace(line, "")
		re.Pattern = "\bstatic\b"
		line = re.Replace(line, "")
		re.Pattern = "\bconst\b"
		line = re.Replace(line, "")
		re.Pattern = "{.*"
		line = re.Replace(line, "")
		param_s = line
		re.Pattern = "\*"
		line = re.Replace(line, "")
		re.Pattern = "\(.*"
		line = re.Replace(line, "")
		re.Pattern = "^[ 	]*"
		line = re.Replace(line, "")

		re.Pattern = ".*\("
		param_s = re.Replace(param_s, "")
		re.Pattern = "\).*"
		param_s = re.Replace(param_s, "")

		func = Split(line)
		If UBound(func) > 0 Then
			funcname = func(1)
		Else
			funcname = ""
		End If
		If func(0) = cls And funcname = "" Then
			If api_selected <> "" Then
				WScript.Echo "Constructor for '" & cls & "' has @api. Tag ignored."
				api_selected = ""
			End If
			cls_param(0) = param_s
			If param_s = "" Then Exit Sub
		ElseIf funcname = "" Then
			Exit Sub
		End If

		params = Split(param_s, ",")
		If is_static Then
			types = "."
		Else
			types = "x"
		End If
		For i = LBound(params) To UBound(params)
			Do ' null loop for logic short-circuit
				re.Pattern = "^[ 	]*"
				params(i) = re.Replace(params(i), "")
				re.Pattern = "\*|&"
				If re.Test(params(i)) Then
					re.Pattern = "^char"
					If re.test(params(i)) Then
						' Many types can be converted to string, so use '.', not 's'. (handled by our glue code)
						types = types & "."
						Exit Do
					End If
					re.Pattern = "^void"
					If re.test(params(i)) Then
						types = types & "p"
						Exit Do
					End If
					re.Pattern = "^Array"
					If re.test(params(i)) Then
						types = types & "a"
						Exit Do
					End If
					re.Pattern = "^struct Array"
					If re.test(params(i)) Then
						types = types & "a"
						Exit Do
					End If
					re.Pattern = "^Text"
					If re.test(params(i)) Then
						types = types & "."
						Exit Do
					End If
					types = types & "x"
					Exit Do
				End If
				re.Pattern = "^bool"
				If re.Test(params(i)) Then
					types = types & "b"
					Exit Do
				End If
				re.Pattern = "^HSQUIRRELVM"
				If re.Test(params(i)) Then
					types = "v"
					Exit Do
				End If
				types = types & "i"
			Loop While False ' end of null loop
		Next
		i = i + 1

		' Check if we want to publish this function
		If api_selected = "" Then api_selected = cls_in_api
		If api_selected = "false" Then
			api_selected = ""
			Exit Sub
		End If
		api_selected = ""

		If func(0) = cls And funcname = "" Then
			cls_param(1) = i
			cls_param(2) = types
			Exit Sub
		End If
		If Left(funcname, 1) = "_" And types <> "v" Then Exit Sub
		If is_static Then
			static_method_size = static_method_size + 1
			static_methods.Item(static_method_size) = Array(funcname, i, types)
			Exit Sub
		End If
		method_size = method_size + 1
		methods.Item(method_size) = Array(funcname, i, types)
		Exit Sub
	End If
End Sub

Sub SquirrelExport(api, srcfilename, dstfilename)
	Dim src, dst, line
	Set src = FSO.OpenTextFile(srcfilename, 1)
	Set dst = FSO.OpenTextFile(dstfilename, 2, True)

	enum_size = 0
	Set enums = CreateObject("Scripting.Dictionary")
	enum_value_size = 0
	Set enum_value = CreateObject("Scripting.Dictionary")
	enum_string_to_error_size = 0
	Set enum_string_to_error_mapping_string = CreateObject("Scripting.Dictionary")
	Set enum_string_to_error_mapping_error = CreateObject("Scripting.Dictionary")
	enum_error_to_string_size = 0
	Set enum_error_to_string_mapping = CreateObject("Scripting.Dictionary")
	const_size = 0
	Set const_value = CreateObject("Scripting.Dictionary")
	struct_size = 0
	Set structs = CreateObject("Scripting.Dictionary")
	method_size = 0
	Set methods = CreateObject("Scripting.Dictionary")
	static_method_size = 0
	Set static_methods = CreateObject("Scripting.Dictionary")
	super_cls = ""
	cls = ""
	api_selected = ""
	cls_in_api = ""
	start_squirrel_define_on_next_line = False
	has_fileheader = False
	cls_level = 0
	apis = LCase(api)
	If apis = "gs" Then apis = "game"

	While Not src.AtEndOfStream
		line = src.ReadLine
		SquirrelExportParse api, line, dst
	Wend

	src.Close
	dst.Close
End Sub

Function SortDict(dict)
	Set SortDict = CreateObject("Scripting.Dictionary")
	While dict.Count <> 0
		Dim first, i
		first = ""
		For Each i in dict
			If first = "" Or StrComp(first, i) = 1 Then first = i
		Next
		SortDict.Add first, first
		dict.Remove(first)
	Wend
End Function

Sub ExportInstanceParse(apiuc, apilc, line, file)
	Dim re, fname, f, files, r, regs
	Set re = New RegExp

	re.Pattern = "\.hpp\.sq"
	If re.Test(line) Then
		re.Pattern = "template"
		If re.Test(line) Then
			file.WriteLine line
		End If
		Exit Sub
	End If

	re.Pattern = "SQ" & apiuc & "Controller_Register"
	If re.Test(line) Then
		file.WriteLine line
		Exit Sub
	End If

	re.Pattern = "SQ" & apiuc & ".*_Register"
	If re.Test(line) Then Exit Sub

	re.Pattern = "Note: this line is a marker in squirrel_export.sh. Do not change!"
	If re.Test(line) Then
		file.WriteLine line
		Set files = CreateObject("Scripting.Dictionary")
		For Each fname In FSO.GetFolder(".").Files
			Do ' null loop for logic short-circuit
				re.Pattern = ".*_(.*)\.hpp\.sq"
				If Not re.Test(fname) Then Exit Do
				Set f = FSO.OpenTextFile(fname, 1)
				fname = fname.Name
				re.Pattern = "^void SQ" & apiuc & ".*Register\(Squirrel \*engine\)$"
				While Not f.AtEndOfStream
					If re.Test(f.ReadLine) And Not files.Exists(fname) Then
						files.Add fname, fname
					End If
				Wend
				f.Close
			Loop While False ' end of null loop
		Next
		Set files = SortDict(files)
		For Each f in files
			file.WriteLine "#include " & Chr(34) & "../script/api/" & apilc & "/" & f & Chr(34)
		Next
		Exit Sub
	End If

	re.Pattern = "/\* Register all classes \*/"
	If re.Test(line) Then
		file.WriteLine line
		Set regs = CreateObject("Scripting.Dictionary")
		' List needs to be registered with squirrel before all List subclasses
		file.WriteLine "	SQ" & apiuc & "List_Register(this->engine);"
		For Each fname In FSO.GetFolder(".").Files
			Do ' null loop for logic short-circuit
				re.Pattern = ".*_(.*)\.hpp\.sq"
				If Not re.Test(fname) Then Exit Do
				Set f = FSO.OpenTextFile(fname, 1)
				While Not f.AtEndOfStream
					Do ' null loop for logic short-circuit
						r = f.ReadLine
						re.Pattern = "^void SQ" & apiuc & ".*Register\(Squirrel \*engine\)$"
						If Not re.Test(r) Then Exit Do
						re.Pattern = "SQ" & apiuc & "List_Register"
						If re.Test(r) Then Exit Do
						re.Pattern = "^.*void "
						r = re.Replace(r, "")
						re.Pattern = "Squirrel \*"
						r = re.Replace(r, "this->")
						re.Pattern = "$"
						r = re.Replace(r, ";")
						re.Pattern = "_Register"
						r = re.Replace(r, "0000Register")
						If Not regs.Exists(r) Then regs.Add r, r
					Loop While False ' end of null loop
				Wend
				f.Close
			Loop While False ' end of null loop
		Next
		Set regs = SortDict(regs)
		re.Pattern = "0000Register"
		For Each r in regs.Items
			r = re.Replace(r, "_Register")
			If r <> "SQ" & apiuc & "Controller_Register(this->engine);" Then file.WriteLine "	" & r
		Next
		Exit Sub
	End If

	file.WriteLine line
End Sub

Sub ExportInstance(apiuc, apilc, srcfilename, dstfilename)
	Dim src, dst, line
	Set src = FSO.OpenTextFile(srcfilename, 1)
	Set dst = FSO.OpenTextFile(dstfilename, 2, True)

	While Not src.AtEndOfStream
		line = src.ReadLine
		ExportInstanceParse apiuc, apilc, line, dst
	Wend

	src.Close
	dst.Close
End Sub

' Recursive entry point
Sub Main
	Dim WSH, scriptdir, apilc, re, api, apiuc, f, bf
	Set WSH = CreateObject("WScript.Shell")
	Set re = New RegExp

	' This must be called from within a src/???/api directory.
	scriptdir = FSO.GetParentFolderName(WScript.ScriptFullName)
	apilc = WSH.CurrentDirectory
	re.Pattern = "\\api"
	apilc = re.Replace(apilc, "")
	re.Pattern = ".*\\"
	apilc = re.Replace(apilc, "")

	' Check if we are in the root directory of the API, as then we generate all APIs
	If apilc = "script" Then
		For Each api In FSO.GetFolder(".").SubFolders
			WScript.Echo "Generating for API '" & api.Name & "' ..."
			WSH.CurrentDirectory = api
			Main
		Next
		WScript.Quit 0
	End If

	Select Case apilc
		Case "template" apiuc = "Template"
		Case "ai"       apiuc = "AI"
		Case "game"     apiuc = "GS"
		Case Else
			WScript.Echo "Unknown API type."
			Exit Sub
	End Select

	For Each f in FSO.GetFolder("..").Files
		Do ' null loop for logic short-circuit
			re.Pattern = ".*\.hpp"
			If Not re.Test(f) Then Exit Do
			' ScriptController has custom code, and should not be generated
			If f.Name = "script_controller.hpp" Then Exit Do
			re.Pattern = "script_"
			bf = re.Replace(f.name, apilc & "_")
			SquirrelExport apiuc, f, bf & ".tmp"
			If IsEmptyFile(bf & ".tmp") Then
				If FSO.FileExists(bf & ".sq") Then
					WScript.Echo "Deleted: " & bf & ".sq"
					FSO.DeleteFile bf & ".sq"
				End If
				FSO.DeleteFile bf & ".tmp"
			ElseIf Not FSO.FileExists(bf & ".sq") Or Not CompareFiles(bf & ".sq", bf & ".tmp") Then
				If FSO.FileExists(bf & ".sq") Then FSO.DeleteFile bf & ".sq"
				FSO.MoveFile bf & ".tmp", bf & ".sq"
				WScript.Echo "Updated: " & bf & ".sq"
			Else
				FSO.DeleteFile bf & ".tmp"
			End If
		Loop While False ' end of null loop
	Next

	' Remove .hpp.sq if .hpp doesn't exist anymore
	For Each f in FSO.GetFolder(".").Files
		Do ' null loop for logic short-circuit
			re.Pattern = ".*\.hpp\.sq"
			If Not re.Test(f) Then Exit Do
			f = f.Name
			re.Pattern = "\.hpp\.sq$"
			f = re.Replace(f, ".hpp")
			re.Pattern = apilc & "_"
			f = re.Replace(f, "script_")
			If Not FSO.FileExists("..\" & f) Then
				WScript.Echo "Deleted: " & f & ".sq"
				'FSO.DeleteFile f & ".sq"
			End If
		Loop While False ' end of null loop
	Next

	If apilc = "template" Then Exit Sub

	' Add stuff to ${apilc}_instance.cpp
	f = "..\..\..\" & apilc & "\" & apilc & "_instance.cpp"
	ExportInstance apiuc, apilc, f, f & ".tmp"
	If Not FSO.FileExists(f) Or Not CompareFiles(f, f & ".tmp") Then
		If FSO.FileExists(f) Then FSO.DeleteFile f
		FSO.MoveFile f & ".tmp", f
		WScript.Echo "Updated: " & f
	Else
		FSO.DeleteFile f & ".tmp"
	End If
End Sub

Main
