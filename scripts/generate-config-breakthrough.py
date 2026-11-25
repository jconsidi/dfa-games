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
            # no captures
            positions_expected.append(width * 3 - 2)
        elif height == 4:
            # all captures
            positions_expected.append(width * 2 - 2)

        if height >= 6:
            # no captures yet
            positions_expected.append(positions_expected[-1] ** 2)

        game_config.add_position(
            {
                "comment": "initial position",
                "position": initial_position,
                "side_to_move": side_to_move,
                "expected_perft_u": positions_expected,
            }
        )

    # components

    def calculate_layer(row, col):
        return row * width + col

    for side_to_move in range(2):
        # the side not to move has not reached the side to move's home row
        hostile_char = 2 - side_to_move
        home_row = 0 if side_to_move == 0 else height - 1
        column_names = []
        for col in range(width):
            layer = calculate_layer(home_row, col)
            column_name = f"not {hostile_char},layer={layer:03d}"
            game_config.add_component(
                column_name,
                "count",
                {
                    "c": hostile_char,
                    "count_min": 0,
                    "count_max": 0,
                    "layer_min": layer,
                    "layer_max": layer,
                },
            )
            column_names.append(column_name)
        game_config.add_component(
            f"not done,side_to_move={side_to_move}", "intersection", column_names
        )

    # move graphs

    for side_to_move in range(2):
        friendly_char = 1 + side_to_move
        hostile_char = 2 - side_to_move
        not_done = [f"not done,side_to_move={side_to_move}"]

        def from_node(row, col):
            return f"from {row},{col}"

        def push_node(row, col):
            return f"push {row},{col}"

        def capture_node(row, col):
            return f"capture {row},{col}"

        def add_node(name, changes):
            game_config.add_move_node(side_to_move, name, changes)

        def add_edge(from_name, to_name, conditions=(), edge_name=None):
            game_config.add_move_edge(
                side_to_move, from_name, to_name, conditions, edge_name=edge_name
            )

        # The nodes and edges are added in the order the C++ BreakthroughGame
        # added them, since the order sets the move graph's join tree and the
        # names of its cached DFAs.

        add_node("begin", [])
        add_node("begin+1", [])
        add_edge("begin", "begin+1", not_done, edge_name="begin not lost")

        for row in range(height):
            for col in range(width):
                layer = calculate_layer(row, col)
                add_node(
                    from_node(row, col),
                    [{"layer": layer, "before": friendly_char, "after": 0}],
                )
                add_edge("begin+1", from_node(row, col))

        for row in range(height):
            for col in range(width):
                layer = calculate_layer(row, col)
                add_node(
                    push_node(row, col),
                    [{"layer": layer, "before": 0, "after": friendly_char}],
                )
                add_node(
                    capture_node(row, col),
                    [{"layer": layer, "before": hostile_char, "after": friendly_char}],
                )

        add_node("end-1", [])
        add_node("end", [])
        add_edge("end-1", "end", not_done, edge_name="end not lost")

        # a piece moves one row forward, straight to an empty square or
        # diagonally to an empty or hostile square
        delta_row = 1 if side_to_move == 0 else -1
        for from_row in range(height):
            to_row = from_row + delta_row
            if not (0 <= to_row < height):
                continue

            for from_col in range(width):
                add_edge(from_node(from_row, from_col), push_node(to_row, from_col))
                for to_col in (from_col - 1, from_col + 1):
                    if 0 <= to_col < width:
                        add_edge(
                            from_node(from_row, from_col), capture_node(to_row, to_col)
                        )
                        add_edge(from_node(from_row, from_col), push_node(to_row, to_col))

        for row in range(height):
            for col in range(width):
                add_edge(push_node(row, col), "end-1")
                add_edge(capture_node(row, col), "end-1")

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
