#!/usr/bin/env python3

import math
import sys

from gameconfig import GameConfig


def generate_size(width, height):
    ndim = width * height

    initial_position = []
    initial_position.extend([1] * (width * 2))
    initial_position.extend([0] * (width * (height - 4)))
    initial_position.extend([2] * (width * 2))

    game_config = GameConfig(
        f"breakthrough_{width}x{height}", shape=[3] * ndim, initial_position=initial_position
    )

    for side_to_move in range(2):
        # tests - perft_u from initial position

        positions_expected = []

        if height >= 5:
            positions_expected.append(width * 3 - 2)

        if height >= 6:
            positions_expected.append(positions_expected[-1] ** 2)

        game_config.add_position(
            {
                "comment": "initial position",
                "position": initial_position,
                "side_to_move": side_to_move,
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
    generate_size(6, 6)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
