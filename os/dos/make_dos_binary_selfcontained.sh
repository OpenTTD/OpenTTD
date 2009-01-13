#!/bin/sh

# $Id$

cd `dirname $0`
cc -o exe2coff exe2coff.c || exit
cp $1 binary.exe || exit
./exe2coff binary.exe || exit
cat cwsdstub.exe binary > binary.exe || exit
mv binary.exe $1
rm binary exe2coff
