#!/bin/sh

set -e

# gc.pl now handles cache pruning itself, scoped to whichever directory it
# is given, so there is nothing left for this wrapper to do beyond passing
# the arguments through.
./gc.pl "$@"
