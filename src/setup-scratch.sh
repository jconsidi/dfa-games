#!/bin/bash

set -e

# Mirrors ScratchConfig's precedence and defaults (src/ScratchConfig.cpp):
# both variables default to "scratch", and setting only one is an error
# rather than a guess at what the other should be.
local_dir="${DFA_LOCAL_DIR:-}"
archive_dir="${DFA_ARCHIVE_DIR:-}"

if [ -n "$local_dir" ] && [ -z "$archive_dir" ]; then
    echo "DFA_LOCAL_DIR is set but DFA_ARCHIVE_DIR is not; set both or neither" >&2
    exit 1
fi
if [ -z "$local_dir" ] && [ -n "$archive_dir" ]; then
    echo "DFA_ARCHIVE_DIR is set but DFA_LOCAL_DIR is not; set both or neither" >&2
    exit 1
fi

local_dir="${local_dir:-scratch}"
archive_dir="${archive_dir:-scratch}"

# Ephemeral construction state and disposable caches: safe to wipe entirely.
mkdir -p "$local_dir"/build
mkdir -p "$local_dir"/binarydfa
mkdir -p "$local_dir"/change_cache
mkdir -p "$local_dir"/difference_cache
mkdir -p "$local_dir"/intersection_cache
mkdir -p "$local_dir"/inverse_cache
mkdir -p "$local_dir"/union_cache
mkdir -p "$local_dir"/move_nodes
mkdir -p "$local_dir"/sizes

# Durable, content-addressed output. Per-game directories are created lazily
# by Game.cpp/ConfigGame.cpp, so there is nothing else to pre-create here.
mkdir -p "$archive_dir"/dfas_by_hash
