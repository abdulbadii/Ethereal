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

#define VERSION_ID "11.78"

#if defined(USE_PEXT)
    #define ETHEREAL_VERSION VERSION_ID" (PEXT)"
#elif defined(USE_POPCNT)
    #define ETHEREAL_VERSION VERSION_ID" (POPCNT)"
#else
    #define ETHEREAL_VERSION VERSION_ID
#endif

class UCIGoStruct {
public:
	Board board;
	Thread *threads;
	int multiPV;
	string str{string(512,0)};
};

void uciReport(Thread *threads, int alpha, int beta, int value);
void uciReportCurrentMove(const Board& board, uint16_t move, int currmove, int depth);
void uciReportTBRoot(const Board& board, uint16_t move, unsigned wdl, unsigned dtz);