#!/bin/sh

# $Id$

if ! [ -f ai/regression/regression.nut ]; then
	echo "Make sure you are in the root of OpenTTD before starting this script."
	exit 1
fi

cp ai/regression/regression.nut ai/regression/main.nut
cp ai/regression/regression_info.nut ai/regression/info.nut

if [ -f scripts/game_start.scr ]; then
	mv scripts/game_start.scr scripts/game_start.scr.regression
fi

params=""
gdb=""
if [ "$1" != "-r" ]; then
	params="-snull -mnull -vnull:ticks=30000"
fi
if [ "$1" = "-g" ]; then
	gdb="gdb --ex run --args "
fi
if [ -n "$gdb" ]; then
	$gdb ./openttd -x -c ai/regression/regression.cfg $params -g ai/regression/regression.sav
else
	./openttd -x -c ai/regression/regression.cfg $params -g ai/regression/regression.sav -d ai=2 2>&1 | awk '{ gsub("0x(\\(nil\\)|0+)(x0)?", "0x00000000", $0); gsub("^dbg: \\[ai\\]", "", $0); gsub("^ ", "ERROR: ", $0); gsub("ERROR: \\[1\\] ", "", $0); gsub("\\[P\\] ", "", $0); print $0; }' > tmp.regression
fi

if [ -z "$gdb" ]; then
	res="`diff -ub ai/regression/regression.txt tmp.regression`"
	if [ -z "$res" ]; then
		echo "Regression test passed!"
	else
		echo "Regression test failed! Difference:"
		echo "$res"
	fi
	echo ""
	echo "Regression test done"
fi

rm -f ai/regression/main.nut ai/regression/info.nut

if [ -f scripts/game_start.scr.regression ]; then
	mv scripts/game_start.scr.regression scripts/game_start.scr
fi

if [ "$1" != "-k" ]; then
	rm -f tmp.regression
fi
