# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

#
# Awk script to automatically generate the enums in script_window.hpp
#
# The file is scanned for @enum and @endenum tokens, and the content between them is replaced by an enum from a different file.
#
# Example:
#   // @enum enumname filename
#   ... content here is replaced ...
#   // @endenum
#
# The parameter "enumname" specifies the enumeration to extract. This can also be a regular expression.
# The parameter "filename" specifies the relative path to the file, where the enumeration is extracted from. This can also be a glob expression.
#
#

BEGIN {
	skiptillend = 0;
}

{ CR = (match($0, "\\r$") > 0 ? "\r" : "") }

/@enum/ {
	print;
	add_indent = gensub("[^	]*", "", "g");
	sub(".*@enum *", "");
	enum = $1;
	pattern = $2;

	files = "echo " pattern;
	files | getline filelist;
	close(files);

	split(filelist, filearray);
	count = asort(filearray);
	for (i = 1; i <= count; i++) {
		active = 0;
		active_comment = 0;
		comment = "";
		file = filearray[i];
		print add_indent "/* automatically generated from " file " */" CR
		while ((getline < file) > 0) {
			sub(rm_indent, "");

			# Remember possible doxygen comment before enum declaration
			if ((active == 0) && (match($0, "/\\*\\*") > 0)) {
				comment = add_indent $0;
				active_comment = 1;
			} else if (active_comment == 1) {
				comment = comment "\n" add_indent $0;
			}

			# Check for enum match
			if (match($0, "^	*enum *" enum " *\\{") > 0)  {
				rm_indent = $0;
				gsub("[^	]*", "", rm_indent);
				active = 1;
				if (active_comment > 0) print comment;
				active_comment = 0;
				comment = "";
			}

			# Forget doxygen comment, if no enum follows
			if (active_comment == 2 && $0 != "" CR) {
				active_comment = 0;
				comment = "";
			}
			if (active_comment == 1 && match($0, "\\*/") > 0) active_comment = 2;

			if (active != 0) {
				if (match($0, "^	*[A-Za-z0-9_]* *[,=]") > 0) {
					# Transform enum values
					sub(" *=[^,]*", "");
					sub(" *//", " //");

					match($0, "^(	*)([A-Za-z0-9_]+),(.*)", parts);

					if (parts[3] == "" CR) {
						printf "%s%s%-45s= ::%s\n", add_indent, parts[1], parts[2], (parts[2] "," CR)
					} else {
						printf "%s%s%-45s= ::%-45s%s\n", add_indent, parts[1], parts[2], (parts[2] ","), (parts[3]);
					}
				} else if ($0 == "" CR) {
					print "" CR;
				} else {
					print add_indent $0;
				}
			}

			if (match($0, "^	*\\};") > 0) {
				if (active != 0) print "" CR;
				active = 0;
			}
		}
		close(file);
	}

	skiptillend = 1;
	next;
}

/@endenum/ {
	print;
	skiptillend = 0;
	next;
}

{
	if (skiptillend == 0) print;
}
