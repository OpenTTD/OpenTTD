#!/bin/sh

# $Id$

if ! [ -f ai/regression/run.sh ]; then
	echo "Make sure you are in the root of OpenTTD before starting this script."
	exit 1
fi

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

if [ -d "ai/regression/tst_$1" ]; then
	tests="ai/regression/tst_$1"
elif [ -d "ai/regression/tst_$2" ]; then
	tests="ai/regression/tst_$2"
else
	tests=ai/regression/tst_*
fi

ret=0
for tst in $tests; do
	echo -n "Running $tst... "

	# Make sure that only one info.nut is present for each test run. Otherwise openttd gets confused.
	cp ai/regression/regression_info.nut $tst/info.nut

	sav=$tst/test.sav
	if ! [ -f $sav ]; then
		sav=ai/regression/empty.sav
	fi

	if [ -n "$gdb" ]; then
		$gdb ./openttd -x -c ai/regression/regression.cfg $params -g $sav
	else
		./openttd -x -c ai/regression/regression.cfg $params -g $sav -d script=2 -d misc=9 2>&1 | awk '{ gsub("0x(\\(nil\\)|0+)(x0)?", "0x00000000", $0); gsub("^dbg: \\[script\\]", "", $0); gsub("^ ", "ERROR: ", $0); gsub("ERROR: \\[1\\] ", "", $0); gsub("\\[P\\] ", "", $0); print $0; }' | grep -v '^dbg: \[.*\]' > tmp.regression
	fi

	if [ -z "$gdb" ]; then
		res="`diff -ub $tst/result.txt tmp.regression`"
		if [ -z "$res" ]; then
			echo "passed!"
		else
			echo "failed! Difference:"
			echo "$res"
			ret=1
		fi
	fi

	rm $tst/info.nut
done

if [ -f scripts/game_start.scr.regression ]; then
	mv scripts/game_start.scr.regression scripts/game_start.scr
fi

if [ "$1" != "-k" ]; then
	rm -f tmp.regression
fi

exit $ret
