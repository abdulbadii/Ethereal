/*
	Ethereal is a UCI chess playing engine authored by Andrew Grant.
	<https://github.com/AndyGrant/Ethereal>     <andrew@grantnet.us>

	Ethereal is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Ethereal is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <iostream>
using namespace std;
#include "types.h"

extern const string PieceLabel[COLOUR_NB];

class Board {
public:
	uint64_t pieces[8], colours[3], history[512];
	uint64_t hash, pkhash, kingAttackers;
	uint64_t castleRooks, castleMasks[SQUARE_NB];
	int psqtmat, numMoves, chess960,
	turn, epSquare, halfMoveCounter, fullMoveCounter;
	uint8_t squares[SQUARE_NB];
	void operator()(){
	memset(this, 0, sizeof(*this));
	memset(&squares, EMPTY, sizeof(squares));
	return;}
};

struct Undo {
	uint64_t hash, pkhash, kingAttackers, castleRooks;
	int epSquare, halfMoveCounter, psqtmat, capturePiece;
};

void squareToString(int sq, char *str);
void boardFromFEN(Board& board,const string& fen, int chess960);
void boardToFEN(const Board& board, char *fen);
void printBoard(const Board& board);
int boardHasNonPawnMaterial(const Board& board, int turn);
int boardIsDrawn(const Board& board, int height);
int boardDrawnByFiftyMoveRule(const Board& board);
int boardDrawnByRepetition(const Board& board, int height);
int boardDrawnByInsufficientMaterial(const Board& board);

uint64_t perft(Board& board, int depth);
void runBenchmark(int argc, char **argv);
