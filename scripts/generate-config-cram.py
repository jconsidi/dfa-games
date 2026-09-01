#!/usr/bin/env python3

import math
import sys

from gameconfig import GameConfig


def generate_size(width, height):
    ndim = width * height

    initial_position = [0 for _ in range(ndim)]

    game_config = GameConfig(
        f"cram_{width}x{height}", shape=[2] * ndim, initial_position=initial_position
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

        # nodes

        def calculate_layer(r, c):
            return r * width + c
        
        game_config.add_move_node(side_to_move, "begin", changes=[])

        move_node_names = []
        def add_move_node(layer1, layer2):
            assert 0 <= layer1
            assert layer1 < layer2
            assert layer2 < width * height

            row1 = layer1 // width
            col1 = layer1 % width

            row2 = layer2 // width
            col2 = layer2 % width

            move_node_name = f"move_{row1},{col1}_{row2},{col2}"
            game_config.add_move_node(side_to_move,
                                      move_node_name,
                                      changes = [{"layer": layer1, "before": 0, "after": 1},
                                                 {"layer": layer2, "before": 0, "after": 1}])
            move_node_names.append(move_node_name)

        # horizontal moves
        for row in range(height):
            for col in range(width-1):
                layer = calculate_layer(row, col)
                add_move_node(layer, layer + 1)

        # vertical moves
        for row in range(height - 1):
            for col in range(width):
                layer = calculate_layer(row, col)
                add_move_node(layer, layer + width)
                                      
        game_config.add_move_node(side_to_move, "end", changes = [])

        # edges

        for move_node_name in move_node_names:
            game_config.add_move_edge(side_to_move, "begin", move_node_name, [])
            game_config.add_move_edge(side_to_move, move_node_name, "end", [])
        
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

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
