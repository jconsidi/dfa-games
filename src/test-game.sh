#!/bin/sh

set -e

cd `dirname $0`

GAME="$1"
if [ -z "$GAME" ] ; then
    echo "USAGE:" `basename $0` "<GAME>"
    exit 1
fi

make -j

find scratch/"${GAME}" scratch/move_nodes -type l -exec rm {} \;

./test_perft_u "$GAME"
