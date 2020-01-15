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

#include <cstdint>

#include "types.h"

enum {
	BOUND_NONE,
	BOUND_LOWER,
	BOUND_UPPER,
	BOUND_EXACT,
};

enum {
	TT_MASK_BOUND = 0x03,
	TT_MASK_AGE   = 0xFC,
	TT_BUCKET_NB  = 3,
};

enum {
	PKT_KEY_SIZE   = 16,
	PKT_SIZE       = 1 << PKT_KEY_SIZE,
	PKT_HASH_SHIFT = 64 - PKT_KEY_SIZE
};

struct TTEntry {
	int16_t eval, value;
	uint16_t move, hash16;
	int8_t depth;
	uint8_t generation;
};

struct TTBucket {
	TTEntry slots[TT_BUCKET_NB];
	uint16_t padding;
};

struct TTable {
	uint64_t hashMask;
	TTBucket *buckets;
	uint8_t generation;
};

struct PKEntry {
	uint64_t pkhash;
	uint64_t passed;
	int eval;
};

struct PKTable {
	PKEntry entries[PKT_SIZE];
	PKTable(){};
	PKTable(bool n): nul{n}{}
	bool nul=1;
};

extern TTable Table;
void initTT(uint64_t megabytes);
void updateTT();
void clearTT();
int hashfullTT();
int valueFromTT(int value, int height);
int valueToTT(int value, int height);
void storeTTEntry(uint64_t hash, uint16_t move, int value, int eval, int depth, int bound);
// PKEntry* getPKEntry(PKTable& pktable, uint64_t pkhash);
// void storePKEntry(const PKTable& pktable, uint64_t pkhash, uint64_t passed, int eval);

inline void storePKEntry(const PKTable& pktable, uint64_t pkhash, uint64_t passed, int eval) {
	PKEntry& pkentry = const_cast<PKTable&>(pktable).entries[pkhash >> PKT_HASH_SHIFT];
	pkentry.pkhash = pkhash;
	pkentry.passed = passed;
	pkentry.eval   = eval;
}

inline int getTTEntry(uint64_t hash, uint16_t& move, int& value, int& eval, int& depth, int& bound) {

	const uint16_t hash16 = hash >> 48;
	TTEntry *slots = Table.buckets[hash & Table.hashMask].slots;

	// Search for a matching hash signature
	for (int i = 0; i < TT_BUCKET_NB; ++i) {
		if (slots[i].hash16 == hash16) {

				// Update age but retain bound type
				slots[i].generation = Table.generation | (slots[i].generation & TT_MASK_BOUND);

				// Copy over the TTEntry and signal success
				move  = slots[i].move;
				value = slots[i].value;
				eval  = slots[i].eval;
				depth = slots[i].depth;
				bound = slots[i].generation & TT_MASK_BOUND;
				return 1;
		}
	}

	return 0;
}
