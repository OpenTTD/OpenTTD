#!/bin/sh

# $Id$

# Arguments given? Show help text.
if [ "$#" != "0" ]; then
	cat <<EOF
Usage: ./findversion.sh
Finds the current revision and if the code is modified.

Output: <REV>\t<REV_NR>\t<MODIFIED>\t<CLEAN_REV>
REV
    a string describing what version of the code the current checkout is
    based on. The exact format of this string depends on the version
    control system in use, but it tries to identify the revision used as
    close as possible (using the svn revision number or hg/git hash).
    This also includes an indication of whether the checkout was
    modified and which branch was checked out. This value is not
    guaranteed to be sortable, but is mainly meant for identifying the
    revision and user display.

    If no revision identifier could be found, this is left empty.
REV_NR
    the revision number of the svn revision this checkout is based on.
    This can be used to determine which functionality is present in this
    checkout. For trunk svn checkouts and hg/git branches based upon it,
    this number should be accurate. For svn branch checkouts, this
    number is mostly meaningless, at least when comparing with the
    REV_NR from other branches or trunk.

    This number should be sortable. Within a given branch or trunk, a
    higher number means a newer version. However, when using git or hg,
    this number will not increase on new commits.

    If no revision number could be found, this is left empty.
MODIFIED
    Whether (the src directory of) this checkout is modified or not. A
    value of 0 means not modified, a value of 2 means it was modified.
    Modification is determined in relation to the commit identified by
    REV, so not in relation to the svn revision identified by REV_NR.

    A value of 1 means that the modified status is unknown, because this
    is not an svn/git/hg checkout for example.

CLEAN_REV
    the same as REV but without branch name

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
SRC_DIR=src

# Determine if we are using a modified version
# Assume the dir is not modified
MODIFIED="0"
if [ -d "$ROOT_DIR/.svn" ]; then
	# We are an svn checkout
	if [ -n "`svnversion \"$SRC_DIR\" | grep 'M'`" ]; then
		MODIFIED="2"
	fi
	# Find the revision like: rXXXXM-branch
	BRANCH=`LC_ALL=C svn info "$SRC_DIR" | "$AWK" '/^URL:.*branches/ { split($2, a, "/"); for(i in a) if (a[i]=="branches") { print a[i+1]; break } }'`
	TAG=`LC_ALL=C svn info "$SRC_DIR" | "$AWK" '/^URL:.*tags/ { split($2, a, "/"); for(i in a) if (a[i]=="tags") { print a[i+1]; break } }'`
	REV_NR=`LC_ALL=C svn info "$SRC_DIR" | "$AWK" '/^Last Changed Rev:/ { print $4 }'`
	if [ -n "$TAG" ]; then
		REV=$TAG
	else
		REV="r$REV_NR"
	fi
elif [ -d "$ROOT_DIR/.git" ]; then
	# We are a git checkout
	if [ -n "`git diff-index HEAD \"$SRC_DIR\"`" ]; then
		MODIFIED="2"
	fi
	HASH=`LC_ALL=C git rev-parse --verify HEAD 2>/dev/null`
	REV="g`echo $HASH | cut -c1-8`"
	BRANCH=`git branch|grep '[*]' | sed 's~\* ~~;s~^master$~~'`
	REV_NR=`LC_ALL=C git log --pretty=format:%s "$SRC_DIR" | grep "^(svn r[0-9]*)" | head -n 1 | sed "s~.*(svn r\([0-9]*\)).*~\1~"`
elif [ -d "$ROOT_DIR/.hg" ]; then
	# We are a hg checkout
	if [ -n "`hg status \"$SRC_DIR\" | grep -v '^?'`" ]; then
		MODIFIED="2"
	fi
	HASH=`LC_ALL=C hg parents 2>/dev/null | head -n 1 | cut -d: -f3`
	REV="h`echo $HASH | cut -c1-8`"
	BRANCH=`hg branch | sed 's~^default$~~'`
	REV_NR=`LC_ALL=C hg log -r $HASH:0 -k "svn" -l 1 --template "{desc}\n" "$SRC_DIR" | grep "^(svn r[0-9]*)" | head -n 1 | sed "s~.*(svn r\([0-9]*\)).*~\1~"`
else
	# We don't know
	MODIFIED="1"
	BRANCH=""
	REV=""
	REV_NR=""
fi

if [ "$MODIFIED" -eq "2" ]; then
	REV="${REV}M"
fi

CLEAN_REV=${REV}

if [ -n "$BRANCH" ]; then
	REV="${REV}-$BRANCH"
fi

echo "$REV	$REV_NR	$MODIFIED	$CLEAN_REV"
