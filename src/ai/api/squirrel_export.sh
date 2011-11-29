#!/bin/bash

# $Id$

# This file is part of OpenTTD.
# OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.

# Set neutral locale so sort behaves the same everywhere
LC_ALL=C
export LC_ALL

# We really need gawk for this!
AWK=gawk

${AWK} --version > /dev/null 2> /dev/null
if [ "$?" != "0" ]; then
	echo "This script needs gawk to run properly"
	exit 1
fi

# This must be called from within a src/???/api directory.
apilc=`pwd | sed "s@/api@@;s@.*/@@"`
apiuc=`echo ${apilc} | tr [a-z] [A-Z]`

if [ -z "$1" ]; then
	for f in `ls ../../script/api/*.hpp`; do
		bf=`basename ${f} | sed s/script/${apilc}/`

		# ScriptController has custom code, and should not be generated
		if [ "`basename ${f}`" = "script_controller.hpp" ]; then continue; fi

		${AWK} -v api=${apiuc} -f squirrel_export.awk ${f} > ${bf}.tmp

		if [ "`wc -l ${bf}.tmp | cut -d\  -f1`" = "0" ]; then
			if [ -f "${bf}.sq" ]; then
				echo "Deleted: ${bf}.sq"
				svn del --force ${bf}.sq > /dev/null 2>&1
			fi
			rm -f ${bf}.tmp
		elif ! [ -f "${bf}.sq" ] || [ -n "`diff -I '$Id' ${bf}.tmp ${bf}.sq 2> /dev/null || echo boo`" ]; then
			mv ${bf}.tmp ${bf}.sq
			echo "Updated: ${bf}.sq"
			svn add ${bf}.sq > /dev/null 2>&1
			svn propset svn:eol-style native ${bf}.sq > /dev/null 2>&1
			svn propset svn:keywords Id ${bf}.sq > /dev/null 2>&1
		else
			rm -f ${bf}.tmp
		fi
	done
else
	${AWK} -v api=${apiuc} -f squirrel_export.awk $1 > $1.tmp
	if [ `wc -l $1.tmp | cut -d\  -f1` -eq "0" ]; then
		if [ -f "$1.sq" ]; then
			echo "Deleted: $1.sq"
			svn del --force $1.sq > /dev/null 2>&1
		fi
		rm -f $1.tmp
	elif ! [ -f "${f}.sq" ] || [ -n "`diff -I '$Id' $1.sq $1.tmp 2> /dev/null || echo boo`" ]; then
		mv $1.tmp $1.sq
		echo "Updated: $1.sq"
		svn add $1.sq > /dev/null 2>&1
		svn propset svn:eol-style native $1.sq > /dev/null 2>&1
		svn propset svn:keywords Id $1.sq > /dev/null 2>&1
	else
		rm -f $1.tmp
	fi
fi

# Remove .hpp.sq if .hpp doesn't exist anymore
for f in `ls *.hpp.sq`; do
	f=`echo ${f} | sed "s/.hpp.sq$/.hpp/;s/${apilc}/script/"`
	if [ ! -f ../../script/api/${f} ];then
		echo "Deleted: ${f}.sq"
		svn del --force ${f}.sq > /dev/null 2>&1
	fi
done

# Add stuff to ${apilc}_instance.cpp
f="../${apilc}_instance.cpp"

functions=``

echo "
{ }
/.hpp.sq/ { next }
/squirrel_register_std/ { next }
/SQ${apiuc}Controller_Register/ { print \$0; next }
/SQ${apiuc}.*_Register/ { next }

/Note: this line is a marker in squirrel_export.sh. Do not change!/ {
	print \$0
	gsub(\"^.*/\", \"\")
	split(\"`grep '^void SQ'${apiuc}'.*_Register(Squirrel \*engine)$' *.hpp.sq | sed 's/:.*$//' | sort | uniq | tr -d '\r' | tr '\n' ' '`\", files, \" \")

	for (i = 1; files[i] != \"\"; i++) {
		print \"#include \\\"api/\" files[i] \"\\\"\" \$0
	}

	next;
}

/\/\* Register all classes \*\// {
	print \$0
	gsub(\"^.*/\", \"\")
	# List needs to be registered with squirrel before all List subclasses.
	print \"	SQ${apiuc}List_Register(this->engine);\" \$0
	split(\"`grep '^void SQ'${apiuc}'.*_Register(Squirrel \*engine)$' *.hpp.sq | grep -v 'SQ'${apiuc}'List_Register' | sed 's/^.*void //;s/Squirrel \*/this->/;s/$/;/;s/_Register/0000Register/g;' | sort | sed 's/0000Register/_Register/g' | tr -d '\r' | tr '\n' ' '`\", regs, \" \")

	for (i = 1; regs[i] != \"\"; i++) {
		if (regs[i] == \"SQ${apiuc}Controller_Register(this->engine);\") continue
		print \"	\" regs[i] \$0
	}

	next
}

{ print \$0; }
" > ${f}.awk

${AWK} -f ${f}.awk ${f} > ${f}.tmp

if ! [ -f "${f}" ] || [ -n "`diff -I '$Id' ${f} ${f}.tmp 2> /dev/null || echo boo`" ]; then
	mv ${f}.tmp ${f}
	echo "Updated: ${f}"
else
	rm -f ${f}.tmp
fi
rm -f ${f}.awk
