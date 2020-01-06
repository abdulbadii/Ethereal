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

#include <cassert>
#include <cstring>
#include <iostream>
using namespace std;

enum { MG, EG };

enum { WHITE, BLACK };

enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

enum { MAX_PLY = 128, MAX_MOVES = 256, BLOCK=512 };

enum {
	WHITE_PAWN, BLACK_PAWN,
	WHITE_KNIGHT =  4, BLACK_KNIGHT =  5,
	WHITE_BISHOP =  8, BLACK_BISHOP =  9,
	WHITE_ROOK   = 12, BLACK_ROOK   = 13,
	WHITE_QUEEN  = 16, BLACK_QUEEN  = 17,
	WHITE_KING   = 20, BLACK_KING   = 21,
	EMPTY        = 26
};

enum {
	MATE = 32000,
	MATE_IN_MAX = MATE - MAX_PLY,
	MATED_IN_MAX = MAX_PLY - MATE,
	VALUE_NONE = 32001
};

enum {
	SQUARE_NB = 64, COLOUR_NB = 2,
	RANK_NB   =  8, FILE_NB   = 8,
	PHASE_NB  =  2, PIECE_NB  = 6,
	CONT_NB   =  2
};

const char WHITESPACE[]=" \n\r\t\f\v";
constexpr uint64_t allON = ~0ull;

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

// Forward definition of all structs

typedef struct Magic Magic;
typedef class Board Board;
typedef struct Undo Undo;
typedef struct EvalTrace EvalTrace;
typedef struct EvalInfo EvalInfo;
typedef struct MovePicker MovePicker;
typedef struct SearchInfo SearchInfo;
typedef struct PVariation PVariation;
typedef class Thread Thread;
typedef struct TTEntry TTEntry;
typedef struct TTBucket TTBucket;
typedef struct TTable TTable;
typedef struct PKEntry PKEntry;
typedef struct PKTable PKTable;
typedef struct Limits Limits;
typedef class UCIGoStruct UCIGoStruct;

// Renamings, currently for move ordering
typedef uint16_t KillerTable[MAX_PLY+1][2];
typedef uint16_t CounterMoveTable[COLOUR_NB][PIECE_NB][SQUARE_NB];
typedef int16_t HistoryTable[COLOUR_NB][SQUARE_NB][SQUARE_NB];
typedef int16_t ContinuationTable[CONT_NB][PIECE_NB][SQUARE_NB][PIECE_NB][SQUARE_NB];

namespace {
inline int pieceType(int piece) {
	assert(0 <= piece / 4 && piece / 4 <= PIECE_NB);
	assert(piece % 4 <= COLOUR_NB);
	return piece / 4;
}

inline int pieceColour(int piece) {
	assert(0 <= piece / 4 && piece / 4 <= PIECE_NB);
	assert(piece % 4 <= COLOUR_NB);
	return piece % 4;
}

inline int makePiece(int type, int colour) {
	assert(0 <= type && type < PIECE_NB);
	assert(0 <= colour && colour <= COLOUR_NB);
	return type * 4 + colour;
}
}

inline bool equStart(const string& s, const char* key, string& nx){
	uint16_t l=strlen(key);	return !s.compare(0,l,key)? nx=s.substr(l), 1: 0;}
inline bool equStart(const string& s, const char* key){	return !s.compare(0,strlen(key),key); }
inline bool equStart(const string& s, const char* key, size_t& l){	return !s.compare(0,l=strlen(key),key); }

inline string& parse(string& s, string& w, const char* del=WHITESPACE){
	size_t q,p=s.find_first_not_of(del);
	return w=p==string::npos? "":
	(w=s.substr(p),
	q=w.find_first_of(del),
	s=q==string::npos? "":w.substr(q),
	w.substr(0,q));
}
inline string& parse(string& s, string& w, const char* del,__attribute__((unused)) bool f){
	char l[32];	return parse(s, w, strcat(strcpy(l,del),WHITESPACE));}

inline bool strContains(const string& s, const char* key, string& nx) {
	size_t f=s.find(key), p;
	return f==string::npos? 0: 
	(nx=s.substr(f+strlen(key)),
	p=nx.find_first_not_of(WHITESPACE),
	nx=p==string::npos? "": nx.substr(p), 1);
}
inline bool strContains(const string& s, const char* key) {
	size_t f=s.find(key);	return f==string::npos? 0: 1;}

inline string& trTrail(string& s){
	size_t p=s.find_last_not_of(WHITESPACE);
	return s=p==string::npos? "": s.substr(0,p+1);}
inline string& trLead(string& s){
	size_t p=s.find_first_not_of(WHITESPACE);
	return s=p==string::npos ? "": s.substr(p);}
