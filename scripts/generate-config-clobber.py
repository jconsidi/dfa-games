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

    # components - every move removes exactly one hostile stone and keeps
    # the friendly count, so after ply moves side 0 (character 1) has
    # moved ceil(ply / 2) times and side 1 (character 2) floor(ply / 2)
    # times, fixing both counts. later plies would need a negative count.

    initial_counts = [initial_position.count(c) for c in (1, 2)]

    ply = 1
    while True:
        counts = [
            initial_counts[0] - ply // 2,
            initial_counts[1] - (ply + 1) // 2,
        ]
        if min(counts) < 0:
            break

        count_names = []
        for c, count in zip((1, 2), counts):
            count_name = f"bound,ply={ply:03d},c={c}"
            game_config.add_component(
                count_name,
                "count",
                {
                    "c": c,
                    "count_min": count,
                    "count_max": count,
                    "layer_min": 0,
                    "layer_max": ndim - 1,
                },
            )
            count_names.append(count_name)

        game_config.add_component(f"bound,ply={ply:03d}", "intersection", count_names)
        ply += 1

    for side_to_move in range(2):
        # move graph - one node per (from, to) pair of orthogonally
        # adjacent squares. the friendly stone leaves from and replaces
        # the hostile stone on to.

        friendly = 1 + side_to_move
        hostile = 2 - side_to_move

        game_config.add_move_node(side_to_move, "begin", changes=[])

        move_node_names = []

        for x0 in range(width):
            for y0 in range(height):

                def add_move(x1, y1):
                    move_node_name = f"from {x0},{y0} to {x1},{y1}"
                    game_config.add_move_node(
                        side_to_move,
                        move_node_name,
                        changes=[
                            {"layer": x0 + width * y0, "before": friendly, "after": 0},
                            {
                                "layer": x1 + width * y1,
                                "before": hostile,
                                "after": friendly,
                            },
                        ],
                    )
                    move_node_names.append(move_node_name)

                if x0 > 0:
                    add_move(x0 - 1, y0)
                if x0 + 1 < width:
                    add_move(x0 + 1, y0)
                if y0 > 0:
                    add_move(x0, y0 - 1)
                if y0 + 1 < height:
                    add_move(x0, y0 + 1)

        game_config.add_move_node(side_to_move, "end", changes=[])

        for move_node_name in move_node_names:
            game_config.add_move_edge(side_to_move, "begin", move_node_name, [])
            game_config.add_move_edge(side_to_move, move_node_name, "end", [])

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
    generate_size(2, 2)
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
