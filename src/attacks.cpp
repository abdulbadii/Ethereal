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

#include <cassert>
#include <cstdint>

#ifdef USE_PEXT
#include <cimmintrin>
#endif

#include "attacks.h"
// #include "bitboards.h"
#include "board.h"
#include "types.h"

uint64_t PawnAttacks[COLOUR_NB][SQUARE_NB];
uint64_t KnightAttacks[SQUARE_NB];
uint64_t BishopAttacks[0x1480];
uint64_t RookAttacks[0x19000];
uint64_t KingAttacks[SQUARE_NB];

Magic BishopTable[SQUARE_NB];
Magic RookTable[SQUARE_NB];

namespace {
inline int validCoordinate(int rank, int file) {
	return 0 <= rank && rank < RANK_NB
		&& 0 <= file && file < FILE_NB;
}
inline void setSquare(uint64_t& bb, int rank, int file) {
	if (validCoordinate(rank, file))
		bb |= 1ull << square(rank, file);
}

inline uint64_t sliderAttacks(int sq, uint64_t occupied, const int delta[4][2]) {

	int rank, file, dr, df;
	uint64_t result = 0ull;

	for (int i = 0; i < 4; ++i) {

		dr = delta[i][0], df = delta[i][1];

		for (rank = rankOf(sq) + dr, file = fileOf(sq) + df; validCoordinate(rank, file); rank += dr, file += df) {
				setBit(result, square(rank, file));
				if (testBit(occupied, square(rank, file)))
					break;
		}
	}

	return result;
}
inline void initSliderAttacks(int sq, Magic *table, uint64_t magic, const int delta[4][2]) {

	uint64_t edges = ((RANK_1 | RANK_8) & ~Ranks[rankOf(sq)])
						| ((FILE_A | FILE_H) & ~Files[fileOf(sq)]);

	uint64_t occupied = 0ull;

	// Init entry for the given square
	table[sq].magic = magic;
	table[sq].mask  = sliderAttacks(sq, 0, delta) & ~edges;
	table[sq].shift = 64 - popcount(table[sq].mask);

	// Track the offset as we use up the table
	if (sq != SQUARE_NB - 1)
		table[sq+1].offset = table[sq].offset + (1 << popcount(table[sq].mask));

	do { // Init attacks for all occupancy variations
		int index = sliderIndex(occupied, table[sq]);
		table[sq].offset[index] = sliderAttacks(sq, occupied, delta);
		occupied = (occupied - table[sq].mask) & table[sq].mask;
	} while (occupied);
}
}

void initAttacks() {

	const int PawnDelta[2][2]   = {{ 1,-1}, { 1, 1}};
	const int KnightDelta[8][2] = {{-2,-1}, {-2, 1}, {-1,-2}, {-1, 2},{ 1,-2}, { 1, 2}, { 2,-1}, { 2, 1}};
	const int KingDelta[8][2]   = {{-1,-1}, {-1, 0}, {-1, 1}, { 0,-1},{ 0, 1}, { 1,-1}, { 1, 0}, { 1, 1}};
	const int BishopDelta[4][2] = {{-1,-1}, {-1, 1}, { 1,-1}, { 1, 1}};
	const int RookDelta[4][2]   = {{-1, 0}, { 0,-1}, { 0, 1}, { 1, 0}};

	// First square has initial offset
	BishopTable[0].offset = BishopAttacks;
	RookTable[0].offset = RookAttacks;

	// Init attack tables for Pawns
	for (int sq = 0; sq < 64; ++sq) {
		for (int dir = 0; dir < 2; ++dir) {
				setSquare(PawnAttacks[WHITE][sq], rankOf(sq) + PawnDelta[dir][0], fileOf(sq) + PawnDelta[dir][1]);
				setSquare(PawnAttacks[BLACK][sq], rankOf(sq) - PawnDelta[dir][0], fileOf(sq) - PawnDelta[dir][1]);
		}
	}

	// Init attack tables for Knights & Kings
	for (int sq = 0; sq < 64; ++sq) {
		for (int dir = 0; dir < 8; ++dir) {
				setSquare(KnightAttacks[sq], rankOf(sq) + KnightDelta[dir][0], fileOf(sq) + KnightDelta[dir][1]);
				setSquare( KingAttacks[sq], rankOf(sq) +   KingDelta[dir][0], fileOf(sq) +   KingDelta[dir][1]);
		}
	}

	// Init attack tables for sliding pieces
	for (int sq = 0; sq < 64; ++sq) {
		initSliderAttacks(sq, BishopTable, BishopMagics[sq], BishopDelta);
		initSliderAttacks(sq,   RookTable,   RookMagics[sq],   RookDelta);
	}
}

int squareIsAttacked(Board& board, int colour, int sq) {

	uint64_t enemy    = board.colours[!colour];
	uint64_t occupied = board.colours[ colour] | enemy;

	uint64_t enemyPawns   = enemy &  board.pieces[PAWN  ];
	uint64_t enemyKnights = enemy &  board.pieces[KNIGHT];
	uint64_t enemyBishops = enemy & (board.pieces[BISHOP] | board.pieces[QUEEN]);
	uint64_t enemyRooks   = enemy & (board.pieces[ROOK  ] | board.pieces[QUEEN]);
	uint64_t enemyKings   = enemy &  board.pieces[KING  ];

	// Check for attacks to this square. While this function has the same
	// result as using attackersToSquare(board, colour, sq) != 0ull, this
	// has a better running time by avoiding some slider move lookups. The
	// speed gain is easily proven using the provided PERFT suit

	return (pawnAttacks(colour, sq) & enemyPawns)
		|| (knightAttacks(sq) & enemyKnights)
		|| (enemyBishops && (bishopAttacks(sq, occupied) & enemyBishops))
		|| (enemyRooks && (rookAttacks(sq, occupied) & enemyRooks))
		|| (kingAttacks(sq) & enemyKings);
}

uint64_t allAttackersToSquare(Board& board, uint64_t occupied, int sq) {

	// When performing a static exchange evaluation we need to find all
	// attacks to a given square, but we also are given an updated occupied
	// bitboard, which will likely not match the actual board, as pieces are
	// removed during the iterations in the static exchange evaluation

	return (pawnAttacks(WHITE, sq) & board.colours[BLACK] & board.pieces[PAWN])
			| (pawnAttacks(BLACK, sq) & board.colours[WHITE] & board.pieces[PAWN])
			| (knightAttacks(sq) & board.pieces[KNIGHT])
			| (bishopAttacks(sq, occupied) & (board.pieces[BISHOP] | board.pieces[QUEEN]))
			| (rookAttacks(sq, occupied) & (board.pieces[ROOK] | board.pieces[QUEEN]))
			| (kingAttacks(sq) & board.pieces[KING]);
}

uint64_t attackersToKingSquare(Board& board) {

	// Wrapper for allAttackersToSquare() for use in check detection
	int kingsq = getlsb(board.colours[board.turn] & board.pieces[KING]);
	uint64_t occupied = board.colours[WHITE] | board.colours[BLACK];
	return allAttackersToSquare(board, occupied, kingsq) & board.colours[!board.turn];
}
