#!/usr/bin/env python3

# build-site.py - rebuild the static site in docs/ (or OUTPUT_DIR).
#
# One page per configured game size for its test positions
# (src/render_test_positions) and one for its move graph
# (src/render_move_graph), plus an index of game families and a page per
# family linking its sizes. Build both binaries in src/ first. The site
# is built in a fresh directory and only replaces OUTPUT_DIR once every
# page has been written, so a failed run leaves the old site in place.

import html
import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = REPO / "src"
CONFIG = REPO / "config"

# marks a directory this script wrote, so it never replaces anything else
MARKER = ".dfa-games-site"

BINARIES = ["render_test_positions", "render_move_graph"]

STYLE = """
:root {
  --bg: #fbfbfa;
  --surface: #ffffff;
  --text: #1d1d1b;
  --muted: #66665f;
  --border: #deddd8;
  --accent: #2f5d8a;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #161615;
    --surface: #1f1f1d;
    --text: #e9e8e4;
    --muted: #a3a29b;
    --border: #3a3935;
    --accent: #8fb5dc;
  }
}
:root[data-theme="dark"] {
  --bg: #161615;
  --surface: #1f1f1d;
  --text: #e9e8e4;
  --muted: #a3a29b;
  --border: #3a3935;
  --accent: #8fb5dc;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
}
main { max-width: 760px; margin: 0 auto; padding: 24px 16px 48px; }
h1 { font-size: 1.6rem; margin: 0 0 4px; }
.sub { color: var(--muted); margin: 0 0 20px; }
a { color: var(--accent); }
nav { margin-bottom: 16px; font-size: 0.9rem; }
ul.families { list-style: none; padding: 0; margin: 0; }
ul.families li {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 12px 16px;
  margin-bottom: 10px;
}
ul.families a { font-weight: 600; font-size: 1.05rem; }
.count { color: var(--muted); margin-left: 8px; }
table {
  width: 100%;
  border-collapse: collapse;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 8px;
  font-variant-numeric: tabular-nums;
}
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--border); }
th { color: var(--muted); font-weight: 600; }
th.num, td.num { text-align: right; }
"""


def page(title, body):
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<style>{STYLE}</style>
</head>
<body>
<main>
{body}
</main>
</body>
</html>
"""


def size_key(size):
    # natural order: 4x5 before 4x10, tictactoe_3 before tictactoe_10
    return [int(part) for part in re.findall(r"\d+", size)] + [size]


def discover_games():
    families = {}
    for game_dir in sorted(CONFIG.iterdir()):
        if not game_dir.is_dir():
            continue
        game = game_dir.name
        if game.startswith("."):
            # hidden directories are tooling state, such as the .claude
            # directory Claude Code leaves behind, never game config
            continue
        if "_" not in game:
            raise RuntimeError(f"config/{game} has no _SIZE suffix to group it by")
        family, size = game.rsplit("_", 1)
        families.setdefault(family, []).append((size, game))

    if not families:
        raise RuntimeError(f"no game config found in {CONFIG}")

    for sizes in families.values():
        sizes.sort(key=lambda entry: size_key(entry[0]))
    return dict(sorted(families.items()))


def count_positions(game):
    total = 0
    for filename in ["positions-generated.json", "positions-manual.json"]:
        path = CONFIG / game / filename
        if path.exists():
            with path.open() as fp:
                total += len(json.load(fp)["positions"])
    return total


def run(binary, output_path, game):
    command = [str(SRC / binary), str(output_path), game]
    result = subprocess.run(command, cwd=SRC, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout[-4000:])
        sys.stderr.write(result.stderr[-4000:])
        raise RuntimeError(
            f"{binary} {game} failed with exit status {result.returncode}"
        )
    if not output_path.is_file():
        raise RuntimeError(f"{binary} {game} exited 0 but wrote no {output_path}")


def write_index(staging, families):
    items = []
    for family, sizes in families.items():
        noun = "size" if len(sizes) == 1 else "sizes"
        items.append(
            f'<li><a href="{family}/index.html">{html.escape(family)}</a>'
            f'<span class="count">{len(sizes)} {noun}</span></li>'
        )
    body = f"""<h1>DFA games</h1>
<p class="sub">Combinatorial games solved by representing sets of positions as layered DFAs.
The test positions and move graphs of every configured game, from
<a href="https://github.com/jconsidi/dfa-games">jconsidi/dfa-games</a>.</p>
<ul class="families">
{chr(10).join(items)}
</ul>"""
    (staging / "index.html").write_text(page("DFA games", body))


def write_family(staging, family, sizes):
    rows = []
    for size, game in sizes:
        rows.append(
            f"<tr><td>{html.escape(size)}</td>"
            f'<td class="num">{count_positions(game)}</td>'
            f'<td><a href="{game}/positions.html">test positions</a></td>'
            f'<td><a href="{game}/move-graph.html">move graph</a></td></tr>'
        )
    body = f"""<nav><a href="../index.html">DFA games</a></nav>
<h1>{html.escape(family)}</h1>
<p class="sub">{len(sizes)} configured {"size" if len(sizes) == 1 else "sizes"}.</p>
<table>
<tr><th>size</th><th class="num">positions</th><th></th><th></th></tr>
{chr(10).join(rows)}
</table>"""
    (staging / family / "index.html").write_text(page(f"{family} - DFA games", body))


def build(staging, families):
    (staging / MARKER).write_text("written by scripts/build-site.py\n")
    # served as is, without Jekyll
    (staging / ".nojekyll").write_text("")

    write_index(staging, families)
    pages = 1
    for family, sizes in families.items():
        (staging / family).mkdir()
        write_family(staging, family, sizes)
        pages += 1
        for size, game in sizes:
            game_dir = staging / family / game
            game_dir.mkdir()
            run("render_test_positions", game_dir / "positions.html", game)
            run("render_move_graph", game_dir / "move-graph.html", game)
            pages += 2
            print(f"built {family}/{game}", flush=True)
    return pages


def main():
    if len(sys.argv) > 2:
        sys.stderr.write("usage: build-site.py [OUTPUT_DIR]\n")
        return 1

    output = pathlib.Path(
        sys.argv[1] if len(sys.argv) == 2 else REPO / "docs"
    ).resolve()

    for binary in BINARIES:
        if not (SRC / binary).is_file():
            raise RuntimeError(
                f"src/{binary} is missing; build it first with make in src/"
            )

    if output.exists() and not (output / MARKER).is_file():
        raise RuntimeError(
            f"{output} exists but was not written by this script (no {MARKER}); refusing to replace it"
        )

    families = discover_games()

    output.parent.mkdir(parents=True, exist_ok=True)
    staging = pathlib.Path(
        tempfile.mkdtemp(prefix=f".{output.name}-building-", dir=output.parent)
    )
    try:
        pages = build(staging, families)
        if output.exists():
            retired = pathlib.Path(
                tempfile.mkdtemp(prefix=f".{output.name}-old-", dir=output.parent)
            )
            output.rename(retired / output.name)
            staging.rename(output)
            shutil.rmtree(retired)
        else:
            staging.rename(output)
    finally:
        if staging.exists():
            shutil.rmtree(staging)

    sizes = sum(len(s) for s in families.values())
    print(f"wrote {pages} pages to {output}: {len(families)} families, {sizes} sizes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
