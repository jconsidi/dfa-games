#!/bin/sh

set -e

cd `dirname $0`

GAME="$1"
if [ -z "$GAME" ] ; then
    echo "USAGE:" `basename $0` "<GAME>"
    exit 1
fi

PLY_MAX="$2"
if [ -z "${PLY_MAX}" ] ; then
    PLY_MAX="1"
fi

make -j

find scratch/move_nodes -type l -exec rm {} \;

if [ -d "scratch/${GAME}" ] ; then
    find "scratch/${GAME}" -type l -exec rm {} \;
fi

if [ -f "config/${GAME}/tests.json" ] ; then
    ./test_perft_u "$GAME"
fi

./build_forward "$GAME" "${PLY_MAX}"
./build_backward "$GAME" "${PLY_MAX}"
./verify_backward_sound "$GAME" "${PLY_MAX}"
