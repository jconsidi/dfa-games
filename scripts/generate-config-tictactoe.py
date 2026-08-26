#!/usr/bin/env python3

import sys

from gameconfig import GameConfig


def generate_size(n):
    n2 = n * n
    game_config = GameConfig(
        f"tictactoe_{n}", shape=[3] * n2, initial_position=[0] * n2
    )

    game_config.save()


def main():
    for size in range(2, 5):
        generate_size(size)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
