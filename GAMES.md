# Games

The rules of every game this project knows how to solve, and nothing else. A
game's section says what its name means, how it starts, what a legal move is,
and how it ends. Nothing here describes how any of it is represented or
computed.

Every section describes play from the starting position onward. A board that
could never be reached from the start is outside what these rules say, and
nothing here should be read as deciding what happens on one.

Most games are laid out on a rectangular board with a suffix indicating the size of the board.
For example, `breakthrough_4x6` indicates the game of Breakthrough on a 4x6 board.
Dimensions are always written `WIDTHxHEIGHT`.

## The normal play convention

Most of these games are scored by the **normal play convention**: a player who
has no legal move loses, and there are no draws. Nothing else ends the game —
no material count, no target square, no line to complete. Every position is a
win for one side or the other, decided entirely by who runs out of moves.

Games that end some other way have to say so, and each says what its result
rule is instead. Three here do: chess, othello and tic-tac-toe. All three can
be drawn, which is the clearest sign a game is not scored this way.

A game can be won by achieving something and still be scored by normal play, so long as achieving it ends the game.
Typically, this is implemented by adding a condition to all moves where the previous player may not have won.
In the case of Breakthrough, this means that moves are only allowed if the opponent has not moved a piece to the last row of the board.

## amazons

`amazons_WxH` — an amazons board `W` columns wide and `H` rows tall.

Each side has four amazons. Every amazon moves exactly as a chess queen does:
any distance along a row, column or diagonal, over empty squares only. After
moving, that same amazon shoots an arrow from the square it has just arrived
on, and the arrow travels the same way a queen moves — any distance in one of
the eight directions, over empty squares only. The square the arrow lands on
is burned for the rest of the game: nothing may move onto it or through it.

A move is therefore always both halves, the amazon's move and the arrow's
flight, and a move is legal only if both paths are clear. The square the amazon
started on is empty once it has left, so the arrow may be shot back into it.

Normal play: a player with no legal move loses. Because every move burns one
more square, the board fills steadily and the game always ends.

The standard board is 10x10, with each side's four amazons a knight's distance
in from the corners — a4, d1, g1 and j4 for the first player, mirrored to a7,
d10, g10 and j7 for the second. Smaller boards here use the same arrangement
drawn in toward the corners. That scaling is a convention of this project, not
part of the game.

## breakthrough

`breakthrough_WxH` — a board `W` columns wide and `H` rows tall. `H` is at
least 4, since each side fills two rows at the start and they must not overlap.

The first player's pieces fill the two rows at one end, the second player's
fill the two rows at the other, and each side advances toward the other.

A piece moves one square forward: straight ahead, or diagonally forward to
either side. Straight ahead is only legal onto an empty square. A diagonal
step is legal onto an empty square *or* onto a square holding an enemy piece,
which is captured and removed. A piece never captures straight ahead, never
moves backward or sideways, and never jumps.

A player wins by moving a piece onto the far row — the row the opponent
started on. That ends the game at once. A player also wins by leaving the
opponent with no legal move, which happens when the opponent has no pieces
left or every piece is blocked.

Normal play. The far-row win fits the convention because the game stops as
soon as a piece arrives there, leaving the beaten player nothing to do.

## chess

`chess+N` — standard chess. The number after the plus distinguishes internal
variants; the rules are the same in all of them.

Standard rules throughout: the usual starting position, the usual moves
including castling, en passant capture and pawn promotion, and the requirement
that a move must not leave one's own king attacked.

**Not normal play.** Being out of moves does not decide the game by itself. A
player with no legal move is checkmated and loses if their king is attacked,
and stalemated and draws if it is not. Chess is the one game here where the
same "no moves" condition covers both a loss and a draw, and the position has
to be examined to tell which.

Positions here carry no history, so the fifty-move rule and threefold
repetition are not part of the game as played in this project. Stalemate is
the only draw.

## clobber

`clobber_WxH` — a board `W` columns wide and `H` rows tall.

The board starts completely full, with the two players' pieces alternating like
the squares of a checkerboard.

There is exactly one kind of move: a player takes one of their own pieces and
moves it onto an orthogonally adjacent square — up, down, left or right — that
holds an enemy piece. The enemy piece is removed and the moving piece takes its
place, so the square moved from becomes empty. A piece may not move onto an
empty square, may not move onto a friendly piece, and may not move diagonally.
Every move therefore removes exactly one piece from the board.

Normal play: a player with no legal move loses. Pieces with no enemy neighbour
are stuck, so a player can be beaten while still holding most of their pieces.

## normalnim

`normalnim_NxM` — `N` heaps, each starting with `M` tokens.

A move removes any positive number of tokens from exactly one heap, up to and
including all of them. Both players always have the same moves available,
since there is nothing on the board belonging to either of them.

Normal play, which is what the name says: the player who cannot move — the one
facing all heaps empty — loses. This is the ordinary version of nim, as opposed
to misère nim, where the player forced to take the last token loses instead.

## othello

`othello_WxH` — an othello board `W` columns wide and `H` rows tall. Both are
even, so the four starting discs sit squarely in the middle.

The game starts with four discs on the four centre squares, two of each colour,
placed diagonally: the first player's two on one diagonal, the second player's
on the other.

A move places a new disc of the mover's colour on an empty square, and is legal
only if it brackets enemy discs. Look outward from the placed square along each
of the eight directions; if the squares immediately beyond it hold an unbroken
run of one or more enemy discs ending in a friendly disc, that whole run is
bracketed. A move must bracket at least one run, and every run it brackets, in
every direction, flips to the mover's colour.

If a player has no legal move, they pass and the opponent moves again. The game
ends when neither player can move, which is usually when the board is full but
can happen sooner.

**Not normal play.** The winner is whoever has more discs when the game ends,
and equal counts are a draw. Being unable to move is not itself a loss here —
it is a pass, and it can even be the winning side that runs out of moves.

## tictactoe

`tictactoe_N` — an `N x N` board, with `N` in a row needed to win. The familiar
game is `tictactoe_3`.

The board starts empty. A move marks any empty square with the mover's mark;
there is nothing else to a move, and a mark never moves or is removed.
A player wins immediately by completing a line: a full row, a full column, or
either of the two long diagonals, entirely of their own marks. The game ends
the moment a line appears.
In addition, the game ends when there are no longer any empty squares.
If the game ends and no player won, then the result is a tie.
