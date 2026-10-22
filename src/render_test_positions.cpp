// render_test_positions.cpp

// Writes one standalone HTML page showing the test positions configured
// for a game (config/GAME/positions-generated.json and
// positions-manual.json): each position as position_to_string prints it,
// and everything the test programs check about it. Nothing is checked
// here; make test does that. Positions and expected moves are built as
// DFAStrings, so an entry that does not fit the game's shape stops the
// page rather than being drawn.

#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "DFA.h"
#include "GameBase.h"
#include "game_utils.h"
#include "render_utils.h"
#include "test_utils.h"

static const char *page_template = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Test Positions</title>
<style>
:root {
  --bg: #fbfbfa;
  --surface: #ffffff;
  --text: #1d1d1b;
  --muted: #66665f;
  --border: #deddd8;
  --board-bg: #f3f2ee;
  --accent: #2f5d8a;
  --tag-bg: #e8eef5;
  --tag-text: #2f5d8a;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #161615;
    --surface: #1f1f1d;
    --text: #e9e8e4;
    --muted: #a3a29b;
    --border: #3a3935;
    --board-bg: #2a2a27;
    --accent: #8fb5dc;
    --tag-bg: #26323f;
    --tag-text: #a9c8e8;
  }
}
:root[data-theme="dark"] {
  --bg: #161615;
  --surface: #1f1f1d;
  --text: #e9e8e4;
  --muted: #a3a29b;
  --border: #3a3935;
  --board-bg: #2a2a27;
  --accent: #8fb5dc;
  --tag-bg: #26323f;
  --tag-text: #a9c8e8;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
}
main { max-width: 1100px; margin: 0 auto; padding: 24px 16px 48px; }
h1 { font-size: 1.6rem; margin: 0 0 4px; }
h2 { font-size: 1.05rem; margin: 0; }
h3 { font-size: 0.9rem; margin: 16px 0 6px; color: var(--muted); font-weight: 600; }
p { margin: 6px 0; }
.sub { color: var(--muted); margin-bottom: 20px; }
.legend { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 12px 16px; margin-bottom: 24px; }
.legend dt { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 0.85rem; font-weight: 600; }
.legend dd { margin: 0 0 8px 0; color: var(--muted); }
.card { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 16px; margin-bottom: 16px; }
.card-head { display: flex; flex-wrap: wrap; align-items: baseline; gap: 8px 12px; margin-bottom: 10px; }
.tag { display: inline-block; background: var(--tag-bg); color: var(--tag-text); border-radius: 4px; padding: 1px 7px; font-size: 0.78rem; }
.checked-by { color: var(--muted); font-size: 0.82rem; font-weight: 400; }
pre.board {
  display: inline-block;
  margin: 0;
  padding: 8px 10px;
  background: var(--board-bg);
  border: 1px solid var(--border);
  border-radius: 6px;
  font: 15px/1.25 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  letter-spacing: 0.15em;
  max-width: 100%;
  overflow-x: auto;
}
.moves { display: flex; flex-wrap: wrap; gap: 10px; }
.moves pre.board { font-size: 12px; }
.empty { color: var(--muted); font-style: italic; }
table { border-collapse: collapse; font-variant-numeric: tabular-nums; }
th, td { border-bottom: 1px solid var(--border); padding: 3px 14px 3px 0; text-align: right; }
th { color: var(--muted); font-weight: 600; }
code { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 0.88em; }
</style>
</head>
<body>
<main>
<h1 id="title"></h1>
<p class="sub" id="summary"></p>
<dl class="legend" id="legend"></dl>
<div id="cards"></div>
</main>
<script>
const DATA = __DATA__;

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

function board(text) {
  return el("pre", "board", text.replace(/\n+$/, ""));
}

function section(card, title, checkedBy) {
  const h = el("h3", null, title + " ");
  h.appendChild(el("span", "checked-by", "checked by " + checkedBy.join(", ")));
  card.appendChild(h);
}

document.title = DATA.game + " test positions";
document.getElementById("title").textContent = DATA.game + " test positions";
document.getElementById("summary").textContent = DATA.summary;

const legend = document.getElementById("legend");
for (const [program, meaning] of DATA.programs) {
  legend.appendChild(el("dt", null, program));
  legend.appendChild(el("dd", null, meaning));
}

const cards = document.getElementById("cards");
for (const p of DATA.positions) {
  const card = el("section", "card");

  const head = el("div", "card-head");
  head.appendChild(el("h2", null, "#" + p.index + (p.comment ? " · " + p.comment : "")));
  head.appendChild(el("span", "tag", p.file));
  head.appendChild(el("span", "tag", "side to move " + p.side_to_move));
  card.appendChild(head);

  card.appendChild(board(p.text));

  section(card, "Every position", p.always_checked_by);
  card.appendChild(el("p", null, "Move graph moves equal validate_moves; zero moves exactly when the game is over."));

  if (p.expected_moves) {
    const n = p.expected_moves.length;
    section(card, "Expected moves: " + n, p.moves_checked_by);
    if (n === 0) {
      card.appendChild(el("p", "empty", "no moves"));
    } else {
      const grid = el("div", "moves");
      for (const m of p.expected_moves) grid.appendChild(board(m));
      card.appendChild(grid);
    }
  }

  if (p.expected_result) {
    section(card, "Expected result", p.result_checked_by);
    card.appendChild(el("p", null, p.expected_result));
  }

  if (p.expected_perft_u) {
    section(card, "Expected perft_u (distinct positions after each depth)", p.perft_checked_by);
    const table = el("table");
    const hr = el("tr");
    hr.appendChild(el("th", null, "depth"));
    hr.appendChild(el("th", null, "positions"));
    table.appendChild(hr);
    p.expected_perft_u.forEach((count, i) => {
      const tr = el("tr");
      tr.appendChild(el("td", null, String(i + 1)));
      tr.appendChild(el("td", null, count.toLocaleString("en-US")));
      table.appendChild(tr);
    });
    card.appendChild(table);
  }

  cards.appendChild(card);
}
</script>
</body>
</html>
)HTML";

static std::string describe_result(const nlohmann::json& result)
{
  if(result.is_null())
    {
      return "not over (null)";
    }

  int value = result.get<int>();
  switch(value)
    {
    case -1:
      return "over: the side to move has lost (-1)";
    case 0:
      return "over: a draw (0)";
    case 1:
      return "over: the side to move has won (1)";
    }

  throw std::invalid_argument(std::format("expected_result {:d} is not -1, 0, 1 or null", value));
}

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: " << argv[0] << " OUTPUT.html GAME" << std::endl;
      return 1;
    }

  std::string output_path(argv[1]);
  std::string game_name(argv[2]);

  std::unique_ptr<GameBase> game(get_gamebase(game_name));
  const dfa_shape_t& shape = game->get_shape();

  nlohmann::json positions = nlohmann::json::array();
  std::vector<std::string> file_summaries;
  int index = 0;

  for(std::string config_filename : {"positions-generated.json", "positions-manual.json"})
    {
      // a game need not have both files; normalnim has only the manual one
      std::string config_path = std::format("config/{:s}/{:s}", game_name, config_filename);
      if(!std::filesystem::exists(config_path))
	{
	  continue;
	}

      size_t count = 0;

      for(const TestGroup& test_group : get_test_cases(config_filename, "positions", game_name))
	{
	  for(const nlohmann::json& test_case : test_group.test_cases)
	    {
	      ++count;

	      int side_to_move = test_case.at("side_to_move").get<int>();
	      if((side_to_move < 0) || (side_to_move >= game->get_sides()))
		{
		  throw std::invalid_argument(std::format("{:s} position {:d} has side_to_move {:d}, but the game has {:d} sides",
							  config_path, count, side_to_move, game->get_sides()));
		}

	      DFAString position(shape, test_case.at("position").get<std::vector<int>>());

	      nlohmann::json entry = {{"index", ++index},
				      {"file", config_filename},
				      {"comment", test_case.value("comment", "")},
				      {"side_to_move", side_to_move},
				      {"text", game->position_to_string(position)},
				      {"always_checked_by", {"test_move_graph", "test_validate"}}};

	      std::vector<std::string> expectation_checkers = {"test_validate", "Rust positions_manual"};

	      if(test_case.contains("expected_moves"))
		{
		  nlohmann::json moves = nlohmann::json::array();
		  for(const std::vector<int>& move_vector : test_case.at("expected_moves").get<std::vector<std::vector<int>>>())
		    {
		      DFAString move(shape, move_vector);
		      moves.push_back(game->position_to_string(move));
		    }
		  entry["expected_moves"] = moves;
		  entry["moves_checked_by"] = expectation_checkers;
		}

	      if(test_case.contains("expected_result"))
		{
		  entry["expected_result"] = describe_result(test_case.at("expected_result"));
		  entry["result_checked_by"] = expectation_checkers;
		}

	      if(test_case.contains("expected_perft_u"))
		{
		  entry["expected_perft_u"] = test_case.at("expected_perft_u").get<std::vector<size_t>>();
		  entry["perft_checked_by"] = {"test_perft_u", "Rust positions_manual"};
		}

	      positions.push_back(entry);
	    }
	}

      file_summaries.push_back(std::format("{:d} from {:s}", count, config_filename));
    }

  if(positions.size() == 0)
    {
      throw std::runtime_error("no test positions configured for " + game_name);
    }

  std::string summary = std::format("{:d} position{:s}: ", positions.size(), (positions.size() == 1) ? "" : "s");
  for(size_t i = 0; i < file_summaries.size(); ++i)
    {
      summary += (i ? ", " : "") + file_summaries[i];
    }
  summary += ". This page shows what the tests check; make test runs them.";

  nlohmann::json programs = nlohmann::json::array();
  programs.push_back({"test_validate", "Every position in both files: validate_moves returns no moves exactly when validate_outcome reports the game over. Where given, expected_moves must equal validate_moves as a set, and expected_result must equal validate_outcome."});
  programs.push_back({"test_move_graph", "Every position in both files: the moves the game's move graph generates equal validate_moves, with and without join splitting."});
  programs.push_back({"test_perft_u", "Positions with expected_perft_u: the number of distinct positions reached after each depth, generated with the move graph."});
  programs.push_back({"Rust positions_manual", "Every position in both files: expected_moves, expected_result and expected_perft_u, wherever given, against rules written independently from GAMES.md. A file naming a game without Rust rules fails the test rather than being skipped."});

  nlohmann::json data = {{"game", game_name},
			 {"summary", summary},
			 {"programs", programs},
			 {"positions", positions}};

  std::string page(page_template);
  replace_once(page, "__DATA__", script_safe(data.dump()));
  write_page(output_path, page);

  std::cout << summary << std::endl;
  std::cout << "wrote " << page.size() << " bytes to " << output_path << std::endl;

  return 0;
}
