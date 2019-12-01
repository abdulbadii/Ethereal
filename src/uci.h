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
#include <iostream>

#include "types.h"

#define VERSION_ID "11.78"

#if defined(USE_PEXT)
    #define ETHEREAL_VERSION VERSION_ID" (PEXT)"
#elif defined(USE_POPCNT)
    #define ETHEREAL_VERSION VERSION_ID" (POPCNT)"
#else
    #define ETHEREAL_VERSION VERSION_ID
#endif

using std::string;
struct Limits {
    double start, time, inc, mtg, timeLimit;
    int limitedByNone, limitedByTime, limitedBySelf;
    int limitedByDepth, depthLimit, multiPV;
};

struct UCIGoStruct {
    int multiPV;
	 // char str[BLOCK];
    string str{string(BLOCK,0)};
    Board *board;
    Thread *threads;
};

void uciReport(Thread *threads, int alpha, int beta, int value);
void uciReportTBRoot(Board *board, uint16_t move, unsigned wdl, unsigned dtz);
void uciReportCurrentMove(Board *board, uint16_t move, int currmove, int depth);
