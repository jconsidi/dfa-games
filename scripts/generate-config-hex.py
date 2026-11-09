#!/usr/bin/env python3

import sys

from gameconfig import GameConfig

# neighbours of (r, c) on the rhombus board, as (dr, dc) offsets.
NEIGHBOUR_OFFSETS = ((-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0))


def neighbours(n, r, c):
    for dr, dc in NEIGHBOUR_OFFSETS:
        rr = r + dr
        cc = c + dc
        if 0 <= rr < n and 0 <= cc < n:
            yield (rr, cc)


############################################################
# brute force reference, independent of the components ######
############################################################


def connected(n, position, side):
    """Flood fill: has side joined its two edges?

    Side 0 joins row 0 to row n - 1, side 1 joins column 0 to column n - 1.
    """

    stone = side + 1

    def edge_start(r, c):
        return (r if side == 0 else c) == 0

    def edge_end(r, c):
        return (r if side == 0 else c) == n - 1

    stack = [
        (r, c)
        for r in range(n)
        for c in range(n)
        if edge_start(r, c) and position[r * n + c] == stone
    ]
    seen = set(stack)
    while stack:
        r, c = stack.pop()
        if edge_end(r, c):
            return True
        for rr, cc in neighbours(n, r, c):
            if (rr, cc) not in seen and position[rr * n + cc] == stone:
                seen.add((rr, cc))
                stack.append((rr, cc))

    return False


def moves(n, position, side_to_move):
    if connected(n, position, 1 - side_to_move):
        return []

    output = []
    for layer in range(n * n):
        if position[layer] == 0:
            output.append(
                position[:layer] + (side_to_move + 1,) + position[layer + 1 :]
            )

    return output


def perft_u(n, position, side_to_move, depth_max):
    output = []
    positions = {position}
    for ply in range(depth_max):
        positions = {
            m for p in positions for m in moves(n, p, (side_to_move + ply) % 2)
        }
        output.append(len(positions))
        if not positions:
            break

    return output


############################################################
# config ###################################################
############################################################


def generate_size(n, perft_depth):
    n2 = n * n
    initial_position = [0] * n2

    game_config = GameConfig(
        f"hex_{n}", shape=[3] * n2, initial_position=initial_position
    )

    def calculate_layer(r, c):
        return r * n + c

    # components

    edges = [
        [calculate_layer(r, c), calculate_layer(rr, cc)]
        for r in range(n)
        for c in range(n)
        for (rr, cc) in neighbours(n, r, c)
        if calculate_layer(r, c) < calculate_layer(rr, cc)
    ]

    for side in range(2):
        # side 0 joins row 0 to row n - 1, side 1 joins column 0 to column
        # n - 1.
        def edge_layers(i):
            return [
                calculate_layer(i, j) if side == 0 else calculate_layer(j, i)
                for j in range(n)
            ]

        game_config.add_component(
            f"joined,side={side}",
            "connected",
            {
                "edges": edges,
                "classes": {"stone": [side + 1]},
                "labels": {"start": edge_layers(0), "end": edge_layers(n - 1)},
                "group_classes": ["stone"],
                "labels_required": ["start", "end"],
                "count_min": 1,
            },
        )
        game_config.add_component(
            f"not_joined,side={side}", "inverse", f"joined,side={side}"
        )

    for side_to_move in range(2):
        # nodes

        game_config.add_move_node(side_to_move, "begin", changes=[])
        game_config.add_move_node(side_to_move, "not lost", changes=[])

        move_node_names = []
        for r in range(n):
            for c in range(n):
                node_name = f"move={r},{c}"
                game_config.add_move_node(
                    side_to_move,
                    node_name,
                    changes=[
                        {
                            "layer": calculate_layer(r, c),
                            "before": 0,
                            "after": side_to_move + 1,
                        }
                    ],
                )
                move_node_names.append(node_name)

        game_config.add_move_node(side_to_move, "end", [])

        # edges

        game_config.add_move_edge(
            side_to_move,
            "begin",
            "not lost",
            [f"not_joined,side={1 - side_to_move}"],
        )
        for node_name in move_node_names:
            game_config.add_move_edge(side_to_move, "not lost", node_name, [])
            game_config.add_move_edge(side_to_move, node_name, "end", [])

        # tests - perft_u from initial position

        def add_position(position_data, perft_depth_max):
            position_data["expected_perft_u"] = perft_u(
                n,
                tuple(position_data["position"]),
                side_to_move,
                min(perft_depth, perft_depth_max),
            )
            game_config.add_position(position_data)

        add_position(
            {
                "comment": "initial position",
                "position": initial_position,
                "side_to_move": side_to_move,
                "expected_result": None,
            },
            perft_depth,
        )

        # tests - opponent joined their edges down the first column (side 0)
        # or along the first row (side 1), so the side to move has lost.

        opponent = 1 - side_to_move
        lost_position = [0] * n2
        for i in range(n):
            layer = calculate_layer(i, 0) if opponent == 0 else calculate_layer(0, i)
            lost_position[layer] = opponent + 1
        assert connected(n, lost_position, opponent)

        add_position(
            {
                "comment": "lost position",
                "position": lost_position,
                "side_to_move": side_to_move,
                "expected_result": -1,
                "expected_moves": [],
            },
            1,
        )

        # tests - opponent one stone short of the same chain. every empty
        # square is a legal move.

        if n >= 2:
            almost_position = list(lost_position)
            almost_position[
                calculate_layer(n - 1, 0) if opponent == 0 else calculate_layer(0, n - 1)
            ] = 0
            assert not connected(n, almost_position, opponent)

            add_position(
                {
                    "comment": "opponent one stone short",
                    "position": almost_position,
                    "side_to_move": side_to_move,
                    "expected_result": None,
                    "expected_moves": [
                        list(m)
                        for m in moves(n, tuple(almost_position), side_to_move)
                    ],
                },
                3,
            )

    game_config.save()


def main():
    # perft depth by size: the whole game where it is cheap, past the first
    # possible chain (ply 2n - 1) for hex_4, and only the opening beyond.
    generate_size(2, 4)
    generate_size(3, 9)
    generate_size(4, 8)
    generate_size(5, 4)
    generate_size(6, 3)
    generate_size(7, 2)
    generate_size(8, 2)

    return 0


############################################################
# startup handling #########################################
############################################################

if __name__ == "__main__":
    sys.exit(main())
