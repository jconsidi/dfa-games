#!/usr/bin/env python3

import math
import sys

from gameconfig import GameConfig


def generate_size(width, height):
    ndim = width * height

    initial_position = []
    for row in range(height):
        initial_position.extend((row + col) % 2 + 1 for col in range(width))

    game_config = GameConfig(
        f"clobber_{width}x{height}", shape=[3] * ndim, initial_position=initial_position
    )

    for side_to_move in range(2):
        # tests - perft_u from initial position

        positions_expected = []
        positions_expected.append((width - 1) * height + (height - 1) * width)

        game_config.add_position(
            {
                "comment": "initial position",
                "position": initial_position,
                "side_to_move": side_to_move,
                "expected_result": None,
                "expected_perft_u": positions_expected,
            }
        )

    game_config.save()


def main():
    generate_size(4, 4)
    generate_size(4, 5)
    generate_size(4, 6)
    generate_size(5, 4)
    generate_size(5, 5)
    generate_size(5, 6)
    generate_size(6, 5)
    generate_size(6, 6)
    generate_size(7, 7)
    generate_size(8, 8)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
