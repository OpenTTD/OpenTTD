#!/bin/sh

ROOT_DIR=$(dirname "$0")/..

export DYLD_LIBRARY_PATH=${ROOT_DIR}/Frameworks

cd ${ROOT_DIR}/Resources
exec ./openttd "$@"
