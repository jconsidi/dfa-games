#!/usr/bin/env python3
"""Confirm no solver is running against scratch on this host, so gc.pl can
safely run on it.

usage: check_no_solver.py [--scratch DIR]

Every DFA under construction has a staging directory scratch/build/<pid>, and
every BinaryDFA build one at scratch/binarydfa/<pid>, removed when that build
finishes. This checks each one's pid with kill(pid, 0), the same test gc.pl
uses before removing one.

A directory only exists while a build is in progress, so a process between
builds -- loading cached results, or a verifier that only reads -- leaves no
trace here.

Exits 0 when no staging directory has a live pid, 1 when one does, and 2 when
the check itself could not be done, so a failed check is never taken for a
clean one.
"""

import argparse
import os
import re
import sys

SRC_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "src"))


def fail(code, message):
    print(f"check_no_solver: {message}", file=sys.stderr)
    sys.exit(code)


def pid_is_alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        # exists, owned by someone else
        return True
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--scratch", default="scratch")
    args = parser.parse_args()

    scratch_dir = os.path.join(SRC_DIR, args.scratch)
    if not os.path.isdir(scratch_dir):
        fail(2, f"{scratch_dir} is not a directory")

    live = []
    checked = 0
    for staging in ("build", "binarydfa"):
        staging_dir = os.path.join(scratch_dir, staging)
        if not os.path.isdir(staging_dir):
            continue
        for entry in sorted(os.listdir(staging_dir)):
            match = re.match(r"^(\d+)(?:-\d+)?$", entry)
            if not match:
                continue
            checked += 1
            pid = int(match.group(1))
            if pid_is_alive(pid):
                live.append(f"{staging}/{entry} (pid {pid} is running)")

    print(f"checked {checked} staging directories under {scratch_dir}")
    if live:
        for item in live:
            print(f"RUNNING: {item}")
        sys.exit(1)

    print("no solver running")


if __name__ == "__main__":
    main()
