#!/usr/bin/env python3

import sys

from gameconfig import GameConfig


def get_queen_moves(width, height):
    # (from, to, between) for every queen move on an empty board, ordered
    # by from square then to square, with between listed from the from
    # end.

    output = []

    for square_from in range(width * height):
        x_from = square_from % width
        y_from = square_from // width

        for square_to in range(width * height):
            if square_to == square_from:
                continue

            x_to = square_to % width
            y_to = square_to // width

            if (
                x_from != x_to
                and y_from != y_to
                and abs(x_from - x_to) != abs(y_from - y_to)
            ):
                continue

            x_delta = (x_to > x_from) - (x_to < x_from)
            y_delta = (y_to > y_from) - (y_to < y_from)
            distance = max(abs(x_to - x_from), abs(y_to - y_from))

            between = [
                (x_from + i * x_delta) + width * (y_from + i * y_delta)
                for i in range(1, distance)
            ]

            output.append((square_from, square_to, between))

    return output


def get_initial_position(width, height):
    smaller_dim = min(width, height)
    if 4 <= smaller_dim <= 7:
        corner_offset = 1
    elif 8 <= smaller_dim <= 9:
        corner_offset = 2
    elif smaller_dim == 10:
        corner_offset = 3
    else:
        raise ValueError(f"no initial position for amazons_{width}x{height}")

    initial_position = [0] * (width * height)

    def set_square(x, y, c):
        initial_position[x + width * y] = c

    # top row - first player
    set_square(corner_offset, 0, 1)
    set_square(width - 1 - corner_offset, 0, 1)
    # middle row - first player
    set_square(0, corner_offset, 1)
    set_square(width - 1, corner_offset, 1)

    # middle row - second player
    set_square(0, height - 1 - corner_offset, 2)
    set_square(width - 1, height - 1 - corner_offset, 2)

    # bottom row - second player
    set_square(corner_offset, height - 1, 2)
    set_square(width - 1 - corner_offset, height - 1, 2)

    return initial_position


def count_moves(width, height, position, side_to_move):
    # counts moves by walking rays over the position itself, independently
    # of get_queen_moves, so the perft expectations check the move graph
    # against a second derivation.

    directions = [
        (dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (dx, dy) != (0, 0)
    ]

    def reachable(board, x, y):
        output = []
        for dx, dy in directions:
            x1, y1 = x + dx, y + dy
            while 0 <= x1 < width and 0 <= y1 < height and board[x1 + width * y1] == 0:
                output.append((x1, y1))
                x1, y1 = x1 + dx, y1 + dy
        return output

    friendly = 1 + side_to_move

    count = 0
    for x in range(width):
        for y in range(height):
            if position[x + width * y] != friendly:
                continue

            board = list(position)
            board[x + width * y] = 0
            for x1, y1 in reachable(board, x, y):
                board[x1 + width * y1] = friendly
                count += len(reachable(board, x1, y1))
                board[x1 + width * y1] = 0

    return count


def generate_size(width, height):
    ndim = width * height

    initial_position = get_initial_position(width, height)

    game_config = GameConfig(
        f"amazons_{width}x{height}", shape=[4] * ndim, initial_position=initial_position
    )

    # components - empty squares, the conditions for a queen moving or
    # shooting past them.

    def get_empty_name(layer):
        return f"empty,layer={layer:03d}"

    for layer in range(ndim):
        game_config.add_component(get_empty_name(layer), "fixed", {layer: 0})

    queen_moves = get_queen_moves(width, height)

    for side_to_move in range(2):
        # move graph - pick up a friendly queen, put it down on an empty
        # square a queen move away, then shoot an arrow a queen move from
        # there. the queen's from square is already empty when the arrow
        # passes over it.

        friendly = 1 + side_to_move

        def from_node(layer):
            return f"move from={layer}"

        def to_node(layer):
            return f"move to={layer}"

        def shot_node(layer):
            return f"shot at={layer}"

        game_config.add_move_node(side_to_move, "begin", changes=[])
        for layer in range(ndim):
            game_config.add_move_node(
                side_to_move,
                from_node(layer),
                changes=[{"layer": layer, "before": friendly, "after": 0}],
            )
        for layer in range(ndim):
            game_config.add_move_node(
                side_to_move,
                to_node(layer),
                changes=[{"layer": layer, "before": 0, "after": friendly}],
            )
        for layer in range(ndim):
            game_config.add_move_node(
                side_to_move,
                shot_node(layer),
                changes=[{"layer": layer, "before": 0, "after": 3}],
            )
        game_config.add_move_node(side_to_move, "end", changes=[])

        for layer in range(ndim):
            game_config.add_move_edge(
                side_to_move, "begin", from_node(layer), [], edge_name=from_node(layer)
            )

        for layer_from, layer_to, between in queen_moves:
            game_config.add_move_edge(
                side_to_move,
                from_node(layer_from),
                to_node(layer_to),
                [get_empty_name(layer) for layer in between],
                edge_name=f"{from_node(layer_from)}, {to_node(layer_to)}",
            )

        for layer_to, layer_at, between in queen_moves:
            game_config.add_move_edge(
                side_to_move,
                to_node(layer_to),
                shot_node(layer_at),
                [get_empty_name(layer) for layer in between],
                edge_name=f"{to_node(layer_to)}, {shot_node(layer_at)}",
            )

        for layer in range(ndim):
            game_config.add_move_edge(
                side_to_move, shot_node(layer), "end", [], edge_name=shot_node(layer)
            )

        # tests - perft_u from initial position

        game_config.add_position(
            {
                "comment": "initial position",
                "position": initial_position,
                "side_to_move": side_to_move,
                "expected_result": None,
                "expected_perft_u": [
                    count_moves(width, height, initial_position, side_to_move)
                ],
            }
        )

    game_config.save()


def main():
    generate_size(4, 4)
    generate_size(4, 5)
    generate_size(4, 6)
    generate_size(4, 7)
    generate_size(5, 4)
    generate_size(5, 5)
    generate_size(5, 6)
    generate_size(6, 4)
    generate_size(6, 5)
    generate_size(6, 6)
    generate_size(7, 7)
    generate_size(8, 8)
    generate_size(10, 10)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
