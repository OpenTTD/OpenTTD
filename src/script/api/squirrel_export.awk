# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

#
# Awk script to automatically generate the code needed
# to export the script APIs to Squirrel.
#
# Note that arrays are 1 based...
#

# Simple insertion sort.
function array_sort(ARRAY, ELEMENTS, temp, i, j)
{
	for (i = 2; i <= ELEMENTS; i++)
		for (j = i; ARRAY[j - 1] > ARRAY[j]; --j) {
			temp = ARRAY[j]
			ARRAY[j] = ARRAY[j - 1]
			ARRAY[j - 1] = temp
	}
	return
}

function dump_class_templates(name)
{
	realname = name
	gsub("^Script", "", realname)

	print "	template <> inline "       name " *GetParam(ForceType<"       name " *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return  (" name " *)instance; }" CR
	print "	template <> inline "       name " &GetParam(ForceType<"       name " &>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return *(" name " *)instance; }" CR
	print "	template <> inline const " name " *GetParam(ForceType<const " name " *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return  (" name " *)instance; }" CR
	print "	template <> inline const " name " &GetParam(ForceType<const " name " &>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQUserPointer instance; sq_getinstanceup(vm, index, &instance, 0); return *(" name " *)instance; }" CR
	if (name == "ScriptEvent") {
		print "	template <> inline int Return<" name " *>(HSQUIRRELVM vm, " name " *res) { if (res == NULL) { sq_pushnull(vm); return 1; } Squirrel::CreateClassInstanceVM(vm, \"" realname "\", res, NULL, DefSQDestructorCallback<" name ">, true); return 1; }" CR
	} else if (name == "ScriptText") {
		print "" CR
		print "	template <> inline Text *GetParam(ForceType<Text *>, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) {" CR
		print "		if (sq_gettype(vm, index) == OT_INSTANCE) {" CR
		print "			return GetParam(ForceType<ScriptText *>(), vm, index, ptr);" CR
		print "		}" CR
		print "		if (sq_gettype(vm, index) == OT_STRING) {" CR
		print "			return new RawText(GetParam(ForceType<const char *>(), vm, index, ptr));" CR
		print "		}" CR
		print "		return NULL;" CR
		print "	}" CR
	} else {
		print "	template <> inline int Return<" name " *>(HSQUIRRELVM vm, " name " *res) { if (res == NULL) { sq_pushnull(vm); return 1; } res->AddRef(); Squirrel::CreateClassInstanceVM(vm, \"" realname "\", res, NULL, DefSQDestructorCallback<" name ">, true); return 1; }" CR
	}
}

function dump_fileheader()
{
	# Break the Id tag, so SVN doesn't replace it
	print "/* $I" "d$ */" CR
	print "" CR
	print "/*" CR
	print " * This file is part of OpenTTD." CR
	print " * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2." CR
	print " * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." CR
	print " * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>." CR
	print " */" CR
	print "" CR
	print "/* THIS FILE IS AUTO-GENERATED; PLEASE DO NOT ALTER MANUALLY */" CR
	print "" CR
	print "#include \"../" filename "\"" CR
	if (api != "Template") {
		gsub("script_", "template_", filename)
		print "#include \"../template/" filename ".sq\"" CR
	}
}

function reset_reader()
{
	enum_size = 0
	enum_value_size = 0
	enum_string_to_error_size = 0
	enum_error_to_string_size = 0
	struct_size = 0
	method_size = 0
	static_method_size = 0
	cls = ""
	start_squirrel_define_on_next_line = "false"
	cls_level = 0
	cls_in_api = ""
}

BEGIN {
	enum_size = 0
	enum_value_size = 0
	enum_string_to_error_size = 0
	enum_error_to_string_size = 0
	const_size = 0
	struct_size = 0
	method_size = 0
	static_method_size = 0
	super_cls = ""
	cls = ""
	api_selected = ""
	cls_in_api = ""
	start_squirrel_define_on_next_line = "false"
	has_fileheader = "false"
	cls_level = 0
	apis = tolower(api)
	if (apis == "gs") apis = "game"
}

{ CR = (match($0, "\\r$") > 0 ? "\r" : "") }

/@file/ {
	filename = $3
	gsub("^" apis "_", "script_", filename)
}

# Ignore special doxygen blocks
/^#ifndef DOXYGEN_API/          { doxygen_skip = "next"; next; }
/^#ifdef DOXYGEN_API/           { doxygen_skip = "true"; next; }
/^#endif \/\* DOXYGEN_API \*\// { doxygen_skip = "false"; next; }
/^#else/                         {
	if (doxygen_skip == "next") {
		doxygen_skip = "true";
	} else {
		doxygen_skip = "false";
	}
	next;
}
{ if (doxygen_skip == "true") next }

/^([	 ]*)\* @api/ {
	# By default, classes are not selected
	if (cls_level == 0) api_selected = "false"

	gsub("^([	 ]*)", "", $0)
	gsub("* @api ", "", $0)

	if (api == "Template") {
		api_selected = "true"
		if ($0 == "none" CR || $0 == "-all" CR) api_selected = "false"
		next
	}

	if ($0 == "none" CR) {
		api_selected = "false"
	} else if ($0 == "-all" CR) {
		api_selected = "false"
	} else if (match($0, "-" apis)) {
		api_selected = "false"
	} else if (match($0, apis)) {
		api_selected = "true"
	}

	next
}

# Remove the old squirrel stuff
/#ifdef DEFINE_SQUIRREL_CLASS/ { squirrel_stuff = "true";  next; }
/^#endif \/\* DEFINE_SQUIRREL_CLASS \*\// { if (squirrel_stuff == "true") { squirrel_stuff = "false"; next; } }
{ if (squirrel_stuff == "true") next; }

# Ignore forward declarations of classes
/^(	*)class(.*);/ { next; }
# We only want to have public functions exported for now
/^(	*)class/     {
	if (cls_level == 0) {
		if (api_selected == "") {
			print "Class '"$2"' has no @api. It won't be published to any API." > "/dev/stderr"
			api_selected = "false"
		}
		public = "false"
		cls_param[0] = ""
		cls_param[1] = 1
		cls_param[2] = "x"
		cls_in_api = api_selected
		api_selected = ""
		cls = $2
		if (match($4, "public") || match($4, "protected") || match($4, "private")) {
			super_cls = $5
		} else {
			super_cls = $4
		}
	} else if (cls_level == 1) {
		if (api_selected == "") api_selected = cls_in_api

		if (api_selected == "true") {
			struct_size++
			structs[struct_size] = cls "::" $2
		}
		api_selected = ""
	}
	cls_level++
	next
}
/^(	*)public/    { if (cls_level == 1) public = "true";  next; }
/^(	*)protected/ { if (cls_level == 1) public = "false"; next; }
/^(	*)private/   { if (cls_level == 1) public = "false"; next; }

# Ignore the comments
/^#/             { next; }
/\/\*.*\*\//     { comment = "false"; next; }
/\/\*/           { comment = "true";  next; }
/\*\//           { comment = "false"; next; }
{ if (comment == "true") next }

# We need to make specialized conversions for structs
/^(	*)struct/ {
	cls_level++

	# Check if we want to publish this struct
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		next
	}
	api_selected = ""

	if (public == "false") next
	if (cls_level != 1) next

	struct_size++
	structs[struct_size] = cls "::" $2

	next
}

# We need to make specialized conversions for enums
/^(	*)enum/ {
	cls_level++

	# Check if we want to publish this enum
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		next
	}
	api_selected = ""

	if (public == "false") next

	in_enum = "true"
	enum_size++
	enums[enum_size] = cls "::" $2

	next
}

# Maybe the end of the class, if so we can start with the Squirrel export pretty soon
/};/ {
	cls_level--
	if (cls_level != 0) {
		in_enum = "false";
		next;
	}
	if (cls == "") {
		next;
	}
	start_squirrel_define_on_next_line = "true"
	next;
}

# Empty/white lines. When we may do the Squirrel export, do that export.
/^([ 	]*)\r*$/ {
	if (start_squirrel_define_on_next_line == "false") next

	if (cls_in_api != "true") {
		reset_reader()
		next
	}
	if (has_fileheader == "false") {
		dump_fileheader()
		has_fileheader = "true"
	}

	spaces = "                                                               ";
	public = "false"
	namespace_opened = "false"

	api_cls = cls
	gsub("^Script", api, api_cls)
	api_super_cls = super_cls
	gsub("^Script", api, api_super_cls)

	print "" CR

	if (api == "Template") {
		# First check whether we have enums to print
		if (enum_size != 0) {
			if (namespace_opened == "false") {
				print "namespace SQConvert {" CR
				namespace_opened = "true"
			}
			print "	/* Allow enums to be used as Squirrel parameters */" CR
			for (i = 1; i <= enum_size; i++) {
				print "	template <> inline " enums[i] " GetParam(ForceType<" enums[i] ">, HSQUIRRELVM vm, int index, SQAutoFreePointers *ptr) { SQInteger tmp; sq_getinteger(vm, index, &tmp); return (" enums[i] ")tmp; }" CR
				print "	template <> inline int Return<" enums[i] ">(HSQUIRRELVM vm, " enums[i] " res) { sq_pushinteger(vm, (int32)res); return 1; }" CR
				delete enums[i]
			}
		}

		# Then check whether we have structs/classes to print
		if (struct_size != 0) {
			if (namespace_opened == "false") {
				print "namespace SQConvert {" CR
				namespace_opened = "true"
			}
			print "	/* Allow inner classes/structs to be used as Squirrel parameters */" CR
			for (i = 1; i <= struct_size; i++) {
				dump_class_templates(structs[i])
				delete structs[i]
			}
		}

		if (namespace_opened == "false") {
			print "namespace SQConvert {" CR
			namespace_opened = "true"
		} else {
			print "" CR
		}
		print "	/* Allow " cls " to be used as Squirrel parameter */" CR
		dump_class_templates(cls)

		print "} // namespace SQConvert" CR

		reset_reader()
		next
	}

	print "" CR
	print "template <> const char *GetClassName<" cls ", ST_" toupper(api) ">() { return \"" api_cls "\"; }" CR
	print "" CR

	# Then do the registration functions of the class. */
	print "void SQ" api_cls "_Register(Squirrel *engine)" CR
	print "{" CR
	print "	DefSQClass<" cls ", ST_" toupper(api) "> SQ" api_cls "(\"" api_cls "\");" CR
	if (super_cls == "Text" || super_cls == "ScriptObject" || super_cls == "AIAbstractList::Valuator") {
		print "	SQ" api_cls ".PreRegister(engine);" CR
	} else {
		print "	SQ" api_cls ".PreRegister(engine, \"" api_super_cls "\");" CR
	}
	if (super_cls != "ScriptEvent") {
		if (cls_param[2] == "v") {
			print "	SQ" api_cls ".AddSQAdvancedConstructor(engine);" CR
		} else {
			print "	SQ" api_cls ".AddConstructor<void (" cls "::*)(" cls_param[0] "), " cls_param[1]">(engine, \"" cls_param[2] "\");" CR
		}
	}
	print "" CR

	# Enum values
	mlen = 0
	for (i = 1; i <= enum_value_size; i++) {
		if (mlen <= length(enum_value[i])) mlen = length(enum_value[i])
	}
	for (i = 1; i <= enum_value_size; i++) {
		print "	SQ" api_cls ".DefSQConst(engine, " cls "::" enum_value[i] ", " substr(spaces, 1, mlen - length(enum_value[i])) "\""  enum_value[i] "\");" CR
		delete enum_value[i]
	}
	if (enum_value_size != 0) print "" CR

	# Const values
	mlen = 0
	for (i = 1; i <= const_size; i++) {
		if (mlen <= length(const_value[i])) mlen = length(const_value[i])
	}
	for (i = 1; i <= const_size; i++) {
		print "	SQ" api_cls ".DefSQConst(engine, " cls "::" const_value[i] ", " substr(spaces, 1, mlen - length(const_value[i])) "\""  const_value[i] "\");" CR
		delete const_value[i]
	}
	if (const_size != 0) print "" CR

	# Mapping of OTTD strings to errors
	mlen = 0
	for (i = 1; i <= enum_string_to_error_size; i++) {
		if (mlen <= length(enum_string_to_error_mapping_string[i])) mlen = length(enum_string_to_error_mapping_string[i])
	}
	for (i = 1; i <= enum_string_to_error_size; i++) {
		print "	ScriptError::RegisterErrorMap(" enum_string_to_error_mapping_string[i] ", " substr(spaces, 1, mlen - length(enum_string_to_error_mapping_string[i]))  cls "::" enum_string_to_error_mapping_error[i] ");" CR

		delete enum_string_to_error_mapping_string[i]
	}
	if (enum_string_to_error_size != 0) print "" CR

	# Mapping of errors to human 'readable' strings.
	mlen = 0
	for (i = 1; i <= enum_error_to_string_size; i++) {
		if (mlen <= length(enum_error_to_string_mapping[i])) mlen = length(enum_error_to_string_mapping[i])
	}
	for (i = 1; i <= enum_error_to_string_size; i++) {
		print "	ScriptError::RegisterErrorMapString(" cls "::" enum_error_to_string_mapping[i] ", " substr(spaces, 1, mlen - length(enum_error_to_string_mapping[i])) "\"" enum_error_to_string_mapping[i] "\");" CR
		delete enum_error_to_string_mapping[i]
	}
	if (enum_error_to_string_size != 0) print "" CR

	# Static methods
	mlen = 0
	for (i = 1; i <= static_method_size; i++) {
		if (mlen <= length(static_methods[i, 0])) mlen = length(static_methods[i, 0])
	}
	for (i = 1; i <= static_method_size; i++) {
		if (static_methods[i, 2] == "v") {
			print "	SQ" api_cls ".DefSQAdvancedStaticMethod(engine, &" cls "::" static_methods[i, 0] ", " substr(spaces, 1, mlen - length(static_methods[i, 0]) - 8) "\""  static_methods[i, 0] "\");" CR
		} else {
			print "	SQ" api_cls ".DefSQStaticMethod(engine, &" cls "::" static_methods[i, 0] ", " substr(spaces, 1, mlen - length(static_methods[i, 0])) "\""  static_methods[i, 0] "\", " substr(spaces, 1, mlen - length(static_methods[i, 0])) "" static_methods[i, 1] ", \"" static_methods[i, 2] "\");" CR
		}
		delete static_methods[i]
	}
	if (static_method_size != 0) print "" CR

	# Non-static methods
	mlen = 0
	for (i = 1; i <= method_size; i++) {
		if (mlen <= length(methods[i, 0])) mlen = length(methods[i, 0])
	}
	for (i = 1; i <= method_size; i++) {
		if (methods[i, 2] == "v") {
			print "	SQ" api_cls ".DefSQAdvancedMethod(engine, &" cls "::" methods[i, 0] ", " substr(spaces, 1, mlen - length(methods[i, 0]) - 8) "\""  methods[i, 0] "\");" CR
		} else {
			print "	SQ" api_cls ".DefSQMethod(engine, &" cls "::" methods[i, 0] ", " substr(spaces, 1, mlen - length(methods[i, 0])) "\""  methods[i, 0] "\", " substr(spaces, 1, mlen - length(methods[i, 0])) "" methods[i, 1] ", \"" methods[i, 2] "\");" CR
		}
		delete methods[i]
	}
	if (method_size != 0) print "" CR

	print "	SQ" api_cls ".PostRegister(engine);" CR
	print "}" CR

	reset_reader()

	next
}

# Skip non-public functions
{ if (public == "false") next }

# Add enums
{
	if (in_enum == "true") {
		enum_value_size++
		sub(",", "", $1)
		sub("\r", "", $1)
		enum_value[enum_value_size] = $1

		# Check if this a special error enum
		if (match(enums[enum_size], ".*::ErrorMessages") != 0) {
			# syntax:
			# enum ErrorMessages {
			#	ERR_SOME_ERROR,	// [STR_ITEM1, STR_ITEM2, ...]
			# }

			# Set the mappings
			if (match($0, "\\[.*\\]") != 0) {
				mappings = substr($0, RSTART, RLENGTH);
				gsub("([\\[[:space:]\\]])", "", mappings);

				split(mappings, mapitems, ",");
				for (i = 1; i <= length(mapitems); i++) {
					enum_string_to_error_size++
					enum_string_to_error_mapping_string[enum_string_to_error_size] = mapitems[i]
					enum_string_to_error_mapping_error[enum_string_to_error_size] = $1
				}

				enum_error_to_string_size++
				enum_error_to_string_mapping[enum_error_to_string_size] = $1
			}
		}
		next
	}
}

# Add a const (non-enum) value
/^[ 	]*static const \w+ \w+ = -?\(?\w*\)?\w+;/ {
	const_size++
	const_value[const_size] = $4
	next
}

# Add a method to the list
/^.*\(.*\).*\r*$/ {
	if (cls_level != 1) next
	if (match($0, "~")) {
		if (api_selected != "") {
			print "Destructor for '"cls"' has @api. Tag ignored." > "/dev/stderr"
			api_selected = ""
		}
		next
	}

	is_static = match($0, "static")
	gsub("\\yvirtual\\y", "", $0)
	gsub("\\ystatic\\y", "", $0)
	gsub("\\yconst\\y", "", $0)
	gsub("{.*", "", $0)
	param_s = $0
	gsub("\\*", "", $0)
	gsub("\\(.*", "", $0)

	sub(".*\\(", "", param_s)
	sub("\\).*", "", param_s)

	funcname = $2
	if ($1 == cls && funcname == "") {
		if (api_selected != "") {
			print "Constructor for '"cls"' has @api. Tag ignored." > "/dev/stderr"
			api_selected = ""
		}
		cls_param[0] = param_s
		if (param_s == "") next
	} else if (funcname == "") next

	split(param_s, params, ",")
	if (is_static) {
		types = "."
	} else {
		types = "x"
	}
	for (len = 1; params[len] != ""; len++) {
		sub("^[ 	]*", "", params[len])
		if (match(params[len], "\\*") || match(params[len], "&")) {
			if (match(params[len], "^char")) {
				# Many types can be converted to string, so use '.', not 's'. (handled by our glue code)
				types = types "."
			} else if (match(params[len], "^void")) {
				types = types "p"
			} else if (match(params[len], "^Array")) {
				types = types "a"
			} else if (match(params[len], "^struct Array")) {
				types = types "a"
			} else if (match(params[len], "^Text")) {
				types = types "."
			} else {
				types = types "x"
			}
		} else if (match(params[len], "^bool")) {
			types = types "b"
		} else if (match(params[len], "^HSQUIRRELVM")) {
			types = "v"
		} else {
			types = types "i"
		}
	}

	# Check if we want to publish this function
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		next
	}
	api_selected = ""

	if ($1 == cls && funcname == "") {
		cls_param[1] = len;
		cls_param[2] = types;
	} else if (substr(funcname, 0, 1) == "_" && types != "v") {
	} else if (is_static) {
		static_method_size++
		static_methods[static_method_size, 0] = funcname
		static_methods[static_method_size, 1] = len
		static_methods[static_method_size, 2] = types
	} else {
		method_size++
		methods[method_size, 0] = funcname
		methods[method_size, 1] = len
		methods[method_size, 2] = types
	}
	next
}
