#!/usr/bin/env python3

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
            f"not_lost,side_to_move={side_to_move}", "inverse", f"lost,side_to_move={side_to_move}"
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
                    side_to_move, node_name, changes=[{"layer": layer, "before": 0, "after": side_to_move + 1}]
                )
                move_node_names.append(node_name)

        game_config.add_move_node(side_to_move, "end", [])

        # edges

        game_config.add_move_edge(side_to_move, "begin", "not lost", [f"not_lost,side_to_move={side_to_move}"])
        for node_name in move_node_names:
            game_config.add_move_edge(side_to_move, "not lost", node_name, [])
            game_config.add_move_edge(side_to_move, node_name, "end", [])

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
