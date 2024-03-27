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


BEGIN {
	cls = ""
	api_selected = ""
	cls_in_api = ""
	cls_level = 0
	skip_function_body = "false"
	skip_function_par = 0
	RS = "\r|\n"
	apis = tolower(api)
	if (apis == "gs") apis = "game"
}

{
	# replace Script with AI/GS, except for ScriptErrorType
	gsub(/Script/, api)
	gsub(/AIErrorType/, "ScriptErrorType")
	gsub(/GSErrorType/, "ScriptErrorType")
}

{
	if (skip_function_body == "true") {
		input = $0
		gsub(/[^{]/, "", input)
		skip_function_par += length(input)
		if (skip_function_par > 0) {
			input = $0
			gsub(/[^}]/, "", input)
			skip_function_par -= length(input)
			if (skip_function_par == 0) skip_function_body = "false"
		}
	}
	if (skip_function_body == "true") next
}

/@file/ {
	gsub(/script/, apis)
}

/^([	 ]*)\* @api/ {
	if (api == "Script") {
		api_selected = "true"
		next
	}

	# By default, classes are not selected
	if (cls_level == 0) api_selected = "false"

	gsub("^([	 ]*)", "", $0)
	gsub("* @api ", "", $0)

	if ($0 == "none") {
		api_selected = "false"
	} else if ($0 == "-all") {
		api_selected = "false"
	} else if (match($0, "-" apis)) {
		api_selected = "false"
	} else if (match($0, apis)) {
		api_selected = "true"
	}

	next
}

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

		if (cls_in_api == "true") {
			print comment_buffer
			print
			print "public:"
			comment_buffer = ""
		}
	} else if (cls_level == 1) {
		if (api_selected == "") api_selected = cls_in_api

		if (api_selected == "true") {
			print comment_buffer
			print
			print "public:"
			comment_buffer = ""
		}
		api_selected = ""
	} else {
		print "Classes nested too deep" > "/dev/stderr"
		exit 1
	}
	cls_level++
	next
}
/^(	*)public/    { if (cls_level == 1) comment_buffer = ""; public = "true";  next; }
/^(	*)protected/ { if (cls_level == 1) comment_buffer = ""; public = "false"; next; }
/^(	*)private/   { if (cls_level == 1) comment_buffer = ""; public = "false"; next; }

# Ignore special doxygen blocks
/^#ifndef DOXYGEN_API/          { doxygen_skip = "true"; next; }
/^#ifdef DOXYGEN_API/           { doxygen_skip = "next"; next; }
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

/^#/ {
	next
}

# Convert/unify type names
{
	gsub(/\<SQInteger\>/, "int")
	gsub(/\<SquirrelTable\>/, "table")
	gsub(/\<u?int[0-9]*(_t)?\>/, "int")
	gsub(/\<HSQOBJECT\>/, "object")
	gsub(/std::optional<std::string>/, "string")
	gsub(/(const )?std::string *[*&]?/, "string ")
}

# Store comments
/\/\*\*.*\*\//   { comment_buffer = $0; comment = "false"; next; }
/\/\*.*\*\//     { comment_buffer = ""; comment = "false"; next; }
/\/\*\*/         { comment_buffer = $0 "\n"; comment = "true";  next; }
/\/\*/           { comment_buffer = ""; comment = "false";  next; }
/\*\//           { comment_buffer = comment_buffer $0; comment = "false"; next; }
{
	if (comment == "true" && !match($0, /@api/))
	{
		if (match($0, /@game /) && api != "GS") next;
		if (match($0, /@ai /) && api != "AI") next;
		gsub("@game ", "", $0);
		gsub("@ai ", "", $0);
		comment_buffer = comment_buffer $0 "\n"; next;
	}
}


# We need to make specialized conversions for structs
/^(	*)struct/ {
	comments_so_far = comment_buffer
	comment_buffer = ""
	cls_level++

	# Check if we want to publish this struct
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		next
	}
	api_selected = ""

	if (public == "false") next

	print comments_so_far
	print $0

	next
}

# We need to make specialized conversions for enums
/^(	*)enum/ {
	comments_so_far = comment_buffer
	comment_buffer = ""
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

	print comments_so_far
	print $0

	next
}

# Maybe the end of the class, if so we can start with the Squirrel export pretty soon
/};/ {
	comment_buffer = ""
	cls_level--
	if (cls_level != 0) {
		if (in_enum == "true") print
		in_enum = "false"
		next
	}
	if (cls == "") {
		next
	}
	if (cls_in_api == "true") print
	next;
}

# Empty/white lines
/^([ 	]*)$/ {
	print $0

	next
}

# Skip non-public functions
{ if (public == "false") next }

# Add enums
{
	if (in_enum == "true") {
		print comment_buffer
		comment_buffer = ""
		gsub("=([^/]*),", ",", $0)
		print $0

		# Check if this a special error enum
		if (match(enums[enum_size], ".*::ErrorMessages") != 0) {
			# syntax:
			# enum ErrorMessages {
			#	ERR_SOME_ERROR,	// [STR_ITEM1, STR_ITEM2, ...]
			# }

			##TODO: don't print the STR_*
		}
		next
	}
}

# Add a const (non-enum) value
/^[ 	]*static const \w+ \w+ = [^;]+;/ {
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		comment_buffer = ""
		next
	}
	print comment_buffer
	print $0
	comment_buffer = ""
	next
}

# Add a method to the list
/^.*\(.*\).*$/ {
	if (cls_level != 1) next
	if (!match($0, ";")) {
		gsub(/ :$/, ";")
		skip_function_body = "true"
	}
	if (match($0, "~")) {
		if (api_selected != "") {
			print "Destructor for '"cls"' has @api. Tag ignored." > "/dev/stderr"
			api_selected = ""
		}
		next
	}

	# Check if we want to publish this function
	if (api_selected == "") api_selected = cls_in_api
	if (api_selected == "false") {
		api_selected = ""
		comment_buffer = ""
		next
	}
	api_selected = ""

	print comment_buffer
	print $0
	comment_buffer = ""

	next
}
