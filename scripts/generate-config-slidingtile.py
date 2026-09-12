#!/usr/bin/env python3

import math
import sys

from gameconfig import GameConfig


def generate_size(width, height):
    assert width >= 2
    assert height >= 2
    ndim = width * height

    game_config = GameConfig(
        f"slidingtile_{width}x{height}", shape=[ndim] * ndim, initial_position=None, sides=1
    )

    won_position = [i + 1 for i in range(ndim - 1)]
    won_position.append(0)

    # won position as component

    game_config.add_component("won",
                              "fixed",
                              {i : c for (i, c) in enumerate(won_position)})

    # tests - perft_u from won position

    positions_expected = [2]
    positions_expected.append(3 + (1 if width > 2 else 0) + (1 if height > 2 else 0))

    game_config.add_position(
        {
            "comment": "won position",
            "position": won_position,
            "side_to_move": 0,
            "expected_perft_u": positions_expected,
        }
    )

    # nodes

    def calculate_layer(r, c):
        return r * width + c
        
    game_config.add_move_node(0, "begin", changes=[])

    move_node_names = []
    def add_move_nodes(layer1, layer2):
        assert 0 <= layer1 < width * height
        assert 0 <= layer2 < width * height

        row1 = layer1 // width
        col1 = layer1 % width

        row2 = layer2 // width
        col2 = layer2 % width

        for piece in range(1, ndim):
            move_node_name = f"move_{piece:02d}_{row1},{col1}_{row2},{col2}"
            game_config.add_move_node(0,
                                      move_node_name,
                                      changes = [{"layer": layer1, "before": piece, "after": 0},
                                                 {"layer": layer2, "before": 0, "after": piece}])
            move_node_names.append(move_node_name)

    for row in range(height):
        for col in range(width):
            layer = calculate_layer(row, col)

            if row > 0:
                add_move_nodes(layer, layer - width)
            if row + 1 < height:
                add_move_nodes(layer, layer + width)

            if col > 0:
                add_move_nodes(layer, layer - 1)
            if col + 1 < width:
                add_move_nodes(layer, layer + 1)
                
    game_config.add_move_node(0, "end", changes = [])

    # edges

    for move_node_name in move_node_names:
        game_config.add_move_edge(0, "begin", move_node_name, [])
        game_config.add_move_edge(0, move_node_name, "end", [])
        
    game_config.save()


def main():
    generate_size(2, 2)
    generate_size(3, 3)
    generate_size(4, 4)
    generate_size(5, 5)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
