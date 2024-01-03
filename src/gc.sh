#!/bin/sh

set -e

./gc.pl $*

cd scratch

# find -L ... -type l returns symlinks with missing targets
find -L *_cache -type l | xargs rm
