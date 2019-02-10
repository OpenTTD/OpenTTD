#!/bin/sh

# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.


# Arguments given? Show help text.
if [ "$#" != "0" ]; then
	cat <<EOF
Usage: ./findversion.sh
Finds the current revision and if the code is modified.

Output: <VERSION>\t<ISODATE>\t<MODIFIED>\t<HASH>
VERSION
    a string describing what version of the code the current checkout is
    based on.
    This also includes the commit date, an indication of whether the checkout
    was modified and which branch was checked out. This value is not
    guaranteed to be sortable, but is mainly meant for identifying the
    revision and user display.

    If no revision identifier could be found, this is left empty.
ISODATE
    the commit date of the revision this checkout is based on.
    The commit date may differ from the author date.
    This can be used to decide upon the age of the source.

    If no timestamp could be found, this is left empty.
MODIFIED
    Whether (the src directory of) this checkout is modified or not. A
    value of 0 means not modified, a value of 2 means it was modified.

    A value of 1 means that the modified status is unknown, because this
    is not an git checkout for example.

HASH
    the git revision hash

By setting the AWK environment variable, a caller can determine which
version of "awk" is used. If nothing is set, this script defaults to
"awk".
EOF
exit 1;
fi

# Allow awk to be provided by the caller.
if [ -z "$AWK" ]; then
	AWK=awk
fi

# Find out some dirs
cd `dirname "$0"`
ROOT_DIR=`pwd`

# Determine if we are using a modified version
# Assume the dir is not modified
MODIFIED="0"
if [ -d "$ROOT_DIR/.git" ]; then
	# We are a git checkout
	# Refresh the index to make sure file stat info is in sync, then look for modifications
	git update-index --refresh >/dev/null
	if [ -n "`git diff-index HEAD`" ]; then
		MODIFIED="2"
	fi
	HASH=`LC_ALL=C git rev-parse --verify HEAD 2>/dev/null`
	SHORTHASH=`echo ${HASH} | cut -c1-10`
	ISODATE=`LC_ALL=C git show -s --pretty='format:%ci' HEAD | "$AWK" '{ gsub("-", "", $1); print $1 }'`
	BRANCH="`git symbolic-ref -q HEAD 2>/dev/null | sed 's@.*/@@'`"
	TAG="`git name-rev --name-only --tags --no-undefined HEAD 2>/dev/null | sed 's@\^0$@@'`"

	if [ "$MODIFIED" -eq "0" ]; then
		hashprefix="-g"
	elif [ "$MODIFIED" -eq "2" ]; then
		hashprefix="-m"
	else
		hashprefix="-u"
	fi

	if [ -n "$TAG" ]; then
		VERSION="${TAG}"
		ISTAG="1"
	else
		VERSION="${ISODATE}-${BRANCH}${hashprefix}${SHORTHASH}"
		ISTAG="0"
	fi

elif [ -f "$ROOT_DIR/.ottdrev" ]; then
	# We are an exported source bundle
	cat $ROOT_DIR/.ottdrev
	exit
else
	# We don't know
	MODIFIED="1"
	HASH=""
	SHORTHASH=""
	BRANCH=""
	ISODATE=""
	TAG=""
	VERSION=""
	ISTAG="0"
fi

echo "$VERSION	$ISODATE	$MODIFIED	$HASH	$ISTAG"
