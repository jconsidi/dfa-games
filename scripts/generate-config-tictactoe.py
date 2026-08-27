#!/usr/bin/env python3

import math
import sys

from gameconfig import GameConfig


def generate_size(n):
    n2 = n * n
    game_config = GameConfig(
        f"tictactoe_{n}", shape=[3] * n2, initial_position=[0] * n2
    )

    def calculate_layer(r, c):
        return r * n + c

    for side_to_move in range(2):
        # components
        loss_components = []

        def add_loss_component(name, coordinates):
            constraints = {
                calculate_layer(r, c): 2 - side_to_move for (r, c) in coordinates
            }
            game_config.add_component(name, "fixed", constraints)
            loss_components.append(name)

        for r in range(n):
            add_loss_component(
                f"lost,side_to_move={side_to_move},row={r}", [(r, c) for c in range(n)]
            )

        for c in range(n):
            add_loss_component(
                f"lost,side_to_move={side_to_move},col={c}", [(r, c) for r in range(n)]
            )

        add_loss_component(
            f"lost,side_to_move={side_to_move},diag0", [(r, r) for r in range(n)]
        )
        add_loss_component(
            f"lost,side_to_move={side_to_move},diag1",
            [(r, n - 1 - r) for r in range(n)],
        )

        game_config.add_component(
            f"lost,side_to_move={side_to_move}", "union", loss_components
        )

        game_config.add_component(
            f"not_lost,side_to_move={side_to_move}",
            "inverse",
            f"lost,side_to_move={side_to_move}",
        )

        # nodes

        game_config.add_move_node(side_to_move, "begin", changes=[])
        game_config.add_move_node(side_to_move, "not lost", changes=[])

        move_node_names = []
        for r in range(n):
            for c in range(n):
                node_name = f"move={r},{c}"
                layer = r * n + c

                game_config.add_move_node(
                    side_to_move,
                    node_name,
                    changes=[{"layer": layer, "before": 0, "after": side_to_move + 1}],
                )
                move_node_names.append(node_name)

        game_config.add_move_node(side_to_move, "end", [])

        # edges

        game_config.add_move_edge(
            side_to_move, "begin", "not lost", [f"not_lost,side_to_move={side_to_move}"]
        )
        for node_name in move_node_names:
            game_config.add_move_edge(side_to_move, "not lost", node_name, [])
            game_config.add_move_edge(side_to_move, node_name, "end", [])

        # tests - perft_u from initial position

        positions_expected = []
        for depth in range(1, 2 * n + 1):
            moves_0 = (depth + 1) // 2
            moves_1 = (depth + 0) // 2

            positions_0 = math.comb(n2, moves_0)
            positions_1 = math.comb(n2 - moves_0, moves_1)

            positions_0_won = 2 * n + 2 if depth == 2 * n else 0

            positions_expected.append((positions_0 - positions_0_won) * positions_1)

        game_config.add_test(
            {
                "type": "perft_u",
                "position": [0] * n2,
                "side_to_move": side_to_move,
                "expected": positions_expected,
            }
        )

        # tests - perft_u from lost position

        lost_position = []
        # first row by prev player
        lost_position.extend([2 - side_to_move] * n)
        # second row by next player except one
        lost_position.extend([1 + side_to_move] * (n - 1))
        # rest empty
        lost_position.extend([0] * (n * n - 2 * n + 1))

        game_config.add_test(
            {
                "type": "perft_u",
                "position": lost_position,
                "side_to_move": side_to_move,
                "expected": [0],
            }
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
