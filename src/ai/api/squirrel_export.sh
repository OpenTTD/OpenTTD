#!/bin/bash

# $Id$

# This must be called from within the src/ai/api directory.

if [ -z "$1" ]; then
	for f in `ls *.hpp`; do
		case "${f}" in
			# these files should not be changed by this script
			"ai_controller.hpp" | "ai_object.hpp" | "ai_types.hpp" ) continue;
		esac
		awk -f squirrel_export.awk ${f} > ${f}.tmp
		if ! [ -f "${f}.sq" ] || [ -n "`diff -I '$Id' -b ${f}.tmp ${f}.sq 2> /dev/null || echo boo`" ]; then
			mv ${f}.tmp ${f}.sq
			echo "Updated: ${f}.sq"
			svn add ${f}.sq > /dev/null 2>&1
			svn propset svn:eol-style native ${f}.sq > /dev/null 2>&1
			svn propset svn:keywords Id ${f}.sq > /dev/null 2>&1
		else
			rm -f ${f}.tmp
		fi
	done
else
	awk -f squirrel_export.awk $1 > $1.tmp
	if ! [ -f "${f}.sq" ] || [ -n "`diff -I '$Id' -b $1.sq $1.tmp 2> /dev/null || echo boo`" ]; then
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
	f=`echo ${f} | sed "s/.hpp.sq$/.hpp/"`
	if [ ! -f ${f} ];then
		echo "Deleted: ${f}.sq"
		svn del --force ${f}.sq > /dev/null 2>&1
	fi
done

# Add stuff to ai_instance.cpp
f='../ai_instance.cpp'

functions=``

echo "
{ }
/.hpp.sq/ { next }
/squirrel_register_std/ { next }
/SQAIController_Register/ { print \$0; next }
/SQAI.*_Register/ { next }

/Note: this line a marker in squirrel_export.sh. Do not change!/ {
	print \$0
	gsub(\"^.*/\", \"\")
	split(\"`grep '^void SQAI.*_Register(Squirrel \*engine) {$' *.hpp.sq | sed 's/:.*$//' | sort | uniq | tr -d '\r' | tr '\n' ' '`\", files, \" \")

	for (i = 1; files[i] != \"\"; i++) {
		print \"#include \\\"api/\" files[i] \"\\\"\" \$0
	}

	next;
}

/\/\* Register all classes \*\// {
	print \$0
	gsub(\"^.*/\", \"\")
	print \"	squirrel_register_std(this->engine);\" \$0
	split(\"`grep '^void SQAI.*_Register(Squirrel \*engine) {$' *.hpp.sq | sed 's/^.*void //;s/Squirrel \*/this->/;s/ {/;/;s/_Register/0000Register/g;' | sort | sed 's/0000Register/_Register/g' | tr -d '\r' | tr '\n' ' '`\", regs, \" \")

	for (i = 1; regs[i] != \"\"; i++) {
		if (regs[i] == \"SQAIController_Register(this->engine);\") continue
		print \"	\" regs[i] \$0
	}

	next
}

{ print \$0; }
" > ${f}.awk

awk -f ${f}.awk ${f} > ${f}.tmp

if ! [ -f "${f}" ] || [ -n "`diff -I '$Id' -b ${f} ${f}.tmp 2> /dev/null || echo boo`" ]; then
	mv ${f}.tmp ${f}
	echo "Updated: ${f}"
else
	rm -f ${f}.tmp
fi
rm -f ${f}.awk
