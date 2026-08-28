#!/bin/sh

set -e

cd `dirname $0`

usage() {
    echo "USAGE:" `basename $0` "<GAME>" "<PLY_MAX>"
}

GAME="$1"
if [ -z "$GAME" ] ; then
    usage
    exit 1
fi

PLY_MAX="$2"
if [ -z "${PLY_MAX}" ] ; then
    usage
    exit 1
fi

./build_backward "$GAME" "${PLY_MAX}"

echo PLY 000
./verify_lost_sound "$GAME" "backward,ply_max=000,side=0,losing"
./verify_lost_sound "$GAME" "backward,ply_max=000,side=1,losing"
./verify_won_sound "$GAME" "backward,ply_max=000,side=0,winning"
./verify_won_sound "$GAME" "backward,ply_max=000,side=1,winning"

for ply in $(seq "${PLY_MAX}")
do
    ply3=`printf "%03d" "$ply"`
    prev3=`printf "%03d" "$((ply-1))"`

    echo PLY "${ply3}"
    ./verify_losing_sound "$GAME" "backward,ply_max=${ply3},side=0,losing" "backward,ply_max=${prev3},side=1,winning"
    ./verify_losing_sound "$GAME" "backward,ply_max=${ply3},side=1,losing" "backward,ply_max=${prev3},side=0,winning"
    ./verify_winning_sound "$GAME" "backward,ply_max=${ply3},side=0,winning" "backward,ply_max=${prev3},side=1,losing"
    ./verify_winning_sound "$GAME" "backward,ply_max=${ply3},side=1,winning" "backward,ply_max=${prev3},side=0,losing"
done
