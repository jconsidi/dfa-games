# gameconfig.py

import json
import pathlib


class GameConfig(object):
    def __init__(self, game, shape, initial_position):
        self.game = game

        self.game_data = {
            "game": game,
            "shape": list(shape),
            "initial_position": list(initial_position),
        }

        self.components_data = {"game": game, "components": {}}

        self.move_graph_data = []
        self.move_graph_edges = []
        self.move_graph_nodes = []
        for side_to_move in range(2):
            self.move_graph_data.append({"game": game, "nodes": [], "edges": []})
            self.move_graph_edges.append({})
            self.move_graph_nodes.append({})

    def add_component(self, component_name, component_type, component_inputs):
        assert component_name not in self.components_data
        self.components_data["components"][component_name] = {
            "type": component_type,
            "inputs": component_inputs,
        }

    def add_move_edge(
        self, side_to_move, from_node, to_node, conditions, edge_name=None
    ):
        if edge_name is None:
            edge_name = f"{from_node} to {to_node}"

        if edge_name in self.move_graph_edges[side_to_move]:
            raise RuntimeError(
                f"duplicate edge name {edge_name!r} (side_to_move={side_to_move})"
            )
        self.move_graph_edges[side_to_move][edge_name] = len(
            self.move_graph_data[side_to_move]["edges"]
        )

        if (
            self.move_graph_nodes[side_to_move][from_node]
            >= self.move_graph_nodes[side_to_move][to_node]
        ):
            raise RuntimeError(f"edge {edge_name!r} is incompatible with node order")

        conditions = list(conditions)
        self.move_graph_data[side_to_move]["edges"].append({
            "edge": edge_name,
            "from": from_node,
            "to": to_node,
            "conditions": conditions,
        })

    def add_move_node(self, side_to_move, node_name, changes):
        if node_name in self.move_graph_nodes[side_to_move]:
            raise RuntimeError(
                f"duplicate node name {node_name!r} (side_to_move={side_to_move})"
            )
        self.move_graph_nodes[side_to_move][node_name] = len(
            self.move_graph_nodes[side_to_move]
        )

        node_data = {"node": node_name, "changes": list(changes)}

        self.move_graph_data[side_to_move]["nodes"].append(node_data)

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
        save_config("components.json", self.components_data)
        for side_to_move in range(2):
            save_config(
                f"move_graph_{side_to_move}.json",
                self.move_graph_data[side_to_move],
                sort_keys=False,
            )
