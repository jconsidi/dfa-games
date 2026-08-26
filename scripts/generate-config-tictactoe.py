#!/usr/bin/env python3

import json
import pathlib
import sys


class GameConfig(object):
    def __init__(self, game, shape, initial_position):
        self.game = game

        self.game_data = {
            "game": game,
            "shape": list(shape),
            "initial_position": list(initial_position),
        }

    def save(self):
        config_dir = pathlib.Path("config")
        game_dir = config_dir / self.game
        game_dir.mkdir(0o700, exist_ok=True)

        def save_config(config_filename, config_data, **kwargs):
            kwargs.setdefault("indent", 2)
            kwargs.setdefault("sort_keys", True)

            config_path = game_dir / config_filename
            with config_path.open("w") as config_fp:
                json.dump(config_data, config_fp, **kwargs)
                config_fp.write("\n")

        save_config("game.json", self.game_data, sort_keys=False)

def generate_size(n):
    n2 = n * n
    game_config = GameConfig(
        f"tictactoe_{n}", shape=[3] * n2, initial_position=[0] * n2
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
