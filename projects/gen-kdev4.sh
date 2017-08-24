#!/bin/sh

# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

# echo without interpretation of backslash escapes and without
# adding newline at the end - just the string as it is
rawprint()
{
	printf '%s' "$@"
}

encode_dword()
{
	printf '\x%02x' \
		`expr $1 / 16777216 % 256` \
		`expr $1 / 65536    % 256` \
		`expr $1 / 256      % 256` \
		`expr $1            % 256`
}

encode_string()
{
	# turn string into UTF-16 and hexdump it
	hex_utf16=`rawprint "$1" | iconv -t UTF-16BE | od -t x1 -A n | tr -d -c '[:xdigit:]'`;

	encode_dword `rawprint "$hex_utf16" | wc -m | xargs -I {} expr {} / 2` # length = num hex digits / 2
	rawprint "$hex_utf16" | sed 's/../\\x&/g' # put '\x' prefix before every pair of digits
}

encode_single_define()
{
	encode_string `rawprint "$1" | grep -o '^[^=]*'` # everything before '='
	rawprint '\x00\x00\x00\n\x00'
	encode_string `rawprint "$1" | sed 's/^[^=]*=\?//'` # everything after '='
}

# $1 - newline-separated list of defines
encode_defines()
{
	# add some fixed defines and discard empty lines from the tail
	defines=`printf 'va_list\nva_args\n%s' "$1"`

	# count lines (no newline at the end so add one)
	encode_dword `printf '%s\n' "$defines" | wc -l`

	while [ -n "$defines" ]; do
		encode_single_define `rawprint "$defines" | head -n 1`
		defines=`rawprint "$defines" | tail -n +2`
	done
}

encode_includes()
{
	encode_dword 3 # number of custom includes
	encode_string "$1/src/stdafx.h"
	encode_string "$1/objs/lang"
	encode_string "$1/objs/setting"
}

# escape with a backslash (\) characters special to the sed replace string: \ &
# also escape our custom filed separator that we will be using in sed: @
escape_sed_special()
{
	sed -e 's/[\&@]/\\&/g'
}



PROJECT_DIR=`pwd`
DIR_NAME=`pwd | xargs -0 basename`

USAGE_TEXT='Usage:

	projects/gen-kdev4 [PROJECT_NAME|-h|--help]

PROJECT_NAME is the name of the project that will be displayed in KDevelop.
Before executing, cd into OpenTTD folder and run ./configure script.

-h, --help
	print help and exit'

case "$# $1" in
	'1 -h' | '1 --help') printf 'Generate OpenTTD project files for KDevelop 4+\n\n%s\n' "$USAGE_TEXT"; exit 0;;
	1*)                  PROJECT_NAME="$1";;
	0*)                  PROJECT_NAME="$DIR_NAME";;
	*)                   printf 'Wrong arguments given. %s\n' "$USAGE_TEXT" >&2; exit 1;;
esac

CFLAGS=`grep '^using CFLAGS\.\.\.' config.log 2>/dev/null`
if [ -z "$CFLAGS" ]; then
	echo "OpenTTD config.log not found" >&2
	echo "cd into OpenTTD first and run 'configure'" >&2
	exit 1
fi
DEFINES=`eval "printf '%s\n' $CFLAGS" | grep '^\-D' | cut -c3-`

PROJECT_NAME_SED=s@!!PROJECT_NAME!!@`rawprint "$PROJECT_NAME" | escape_sed_special`@g
PROJECT_DIR_SED=s@!!PROJECT_DIR!!@`rawprint "$PROJECT_DIR" | escape_sed_special`@g
CUSTOM_DEFINES_SED=s@!!CUSTOM_DEFINES!!@`encode_defines "$DEFINES" | escape_sed_special`@g
CUSTOM_INCLUDES_SED=s@!!CUSTOM_INCLUDES!!@`encode_includes "$PROJECT_DIR" | escape_sed_special`@g

mkdir -p .kdev4

sed -e "$PROJECT_NAME_SED" \
	>"$PROJECT_DIR/$DIR_NAME.kdev4" \
	<< "EOF"
[Project]
Manager=KDevCustomMakeManager
Name=!!PROJECT_NAME!!
EOF

sed -e "$PROJECT_DIR_SED" -e "$CUSTOM_DEFINES_SED" -e "$CUSTOM_INCLUDES_SED" \
	>"$PROJECT_DIR/.kdev4/$DIR_NAME.kdev4" \
	<< "EOF"
[CustomDefinesAndIncludes][ProjectPath0]
Defines=!!CUSTOM_DEFINES!!
Includes=!!CUSTOM_INCLUDES!!
Path=.

[Defines And Includes][Compiler]
Name=GCC
Path=gcc
Type=GCC

[Filters]
size=10

[Filters][0]
inclusive=0
pattern=.*
targets=3

[Filters][1]
inclusive=0
pattern=.svn
targets=2

[Filters][2]
inclusive=0
pattern=.hg
targets=2

[Filters][3]
inclusive=0
pattern=.git
targets=2

[Filters][4]
inclusive=0
pattern=*.rej
targets=1

[Filters][5]
inclusive=0
pattern=*.orig
targets=1

[Filters][6]
inclusive=0
pattern=*~
targets=1

[Filters][7]
inclusive=0
pattern=.*.kate-swp
targets=1

[Filters][8]
inclusive=0
pattern=.*.swp
targets=1

[Filters][9]
inclusive=0
pattern=/objs
targets=2

[Launch]
Launch Configurations=Launch Configuration 0

[Launch][Launch Configuration 0]
Configured Launch Modes=execute
Configured Launchers=nativeAppLauncher
Name=Launch OpenTTD\s
Type=Native Application

[Launch][Launch Configuration 0][Data]
Arguments=-d 1
Dependencies=@Variant(\x00\x00\x00\t\x00\x00\x00\x00\x00)
Dependency Action=Nothing
EnvironmentGroup=
Executable=file://!!PROJECT_DIR!!/bin/openttd
External Terminal=konsole --noclose --workdir %workdir -e %exe
Project Target=
Use External Terminal=false
Working Directory=file://!!PROJECT_DIR!!/bin
isExecutable=true
EOF
