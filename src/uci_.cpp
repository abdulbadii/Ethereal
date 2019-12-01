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

#include <cinttypes>
#include <pthread.h>
#include <cstdint>
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "fathom/tbprobe.h"
#include "history.h"
#include "masks.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "texel.h"
#include "thread.h"
#include "time.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

extern int MoveOverhead;          // Defined by Time.c
extern unsigned TB_PROBE_DEPTH;   // Defined by Syzygy.c
extern volatile int ABORT_SIGNAL; // Defined by Search.c
extern volatile int IS_PONDERING; // Defined by Search.c

pthread_mutex_t READYLOCK = PTHREAD_MUTEX_INITIALIZER;
const char *StartPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

using namespace std;

inline string& noTrail(string& s){
	size_t p=s.find_last_not_of(WHITESPACE);
	return s=p==string::npos? "": s.substr(0,p+1);}
inline string noTrail(string s){
		size_t p=s.find_last_not_of(WHITESPACE);
		return p==string::npos? "": s.substr(0,p+1);}
inline string& noLead(string& s){
	size_t p=s.find_first_not_of(WHITESPACE);
	return s=p==string::npos ? "": s.substr(p);}
//inline string noLead(string s){
	// size_t p=s.find_first_not_of(WHITESPACE);
	// return p==string::npos ? "": s.substr(p);}
inline bool strStartsWith(string& s, const char* key, size_t& l){	return !s.compare(0,l=strlen(key),key); }
inline bool strStartsWith(string& s, const char* key){	return !s.compare(0,strlen(key),key); }

inline const char* strContains(string& s, const char* key, size_t& u) {
	const char* p=s.c_str();
	size_t f=s.find(key);
	u=f+strlen(key);
	return f==string::npos? nullptr: p+f;
}
inline const char* strContains(string& s, const char* key) {
	const char* p=s.c_str();	size_t f;	
	return (f=s.find(key))==string::npos? nullptr: p+f;
}

int main(int argc, char* argv[]) {
	Board board;
	string str(16*BLOCK,0);
	Thread *threads;
	pthread_t pthreadsgo;
	UCIGoStruct uciGoStruct;

	int chess960 = 0;
	int multiPV  = 1;

	// Initialize core components of Ethereal
	initAttacks(); initMasks(); initEval();
	initSearch(); initZobrist(); initTT(16);
	threads = createThreadPool(1);
	boardFromFEN(&board, StartPosition, chess960);

	// Allow the bench to be run from the command line
	if (argc > 1 && string(argv[1])=="bench") {
		runBenchmark(argc, argv);
		return 0;
	}

	// Allow the tuner to be run when compiled
	#ifdef TUNE
		runTexelTuning(threads);
		return 0;
	#endif

	while (getline(cin,str)) {
		size_t l;
		if (str=="uci") {
				printf("id name Ethereal " ETHEREAL_VERSION "\n");
				printf("id author Andrew Grant & Laldon\n");
				printf("option name Hash ; spin default 16 min 1 max 65536\n");
				printf("option name Threads type spin default 1 min 1 max 2048\n");
				printf("option name MultiPV type spin default 1 min 1 max 256\n");
				printf("option name MoveOverhead type spin default 100 min 0 max 10000\n");
				printf("option name SyzygyPath type string default <empty>\n");
				printf("option name SyzygyProbeDepth type spin default 0 min 0 max 127\n");
				printf("option name Ponder type check default false\n");
				printf("option name UCI_Chess960 type check default false\n");
				printf("uciok\n"), fflush(stdout);
		}

		else if (str=="isready")
				printf("readyok\n"), fflush(stdout);

		else if (str=="ucinewgame")
				resetThreadPool(threads), clearTT();

		else if (strStartsWith(str, "setoption"))
				uciSetOption(str, &threads, &multiPV, &chess960);

		else if (strStartsWith(str, "position"))
				uciPosition(str, &board, chess960);

		else if (strStartsWith(str, "go")) {
				uciGoStruct.str=str;
				uciGoStruct.multiPV = multiPV;
				uciGoStruct.board   = &board;
				uciGoStruct.threads = threads;
				pthread_create(&pthreadsgo, NULL, &uciGo, &uciGoStruct);
		}

		else if (str=="ponderhit")
				IS_PONDERING = 0;

		else if (str=="stop") {
				ABORT_SIGNAL = 1, IS_PONDERING = 0;
				pthread_join(pthreadsgo, NULL);
		}

		else if (str=="quit")
				break;

		else if (strStartsWith(str, "perft ", l))
				printf("%" PRIu64 "\n", perft(&board, stoi(str.substr(l,9),nullptr))), fflush(stdout);
		else if (strStartsWith(str, "print"))
				printBoard(&board), fflush(stdout);
	}

	return 0;
}

void *uciGo(void *cargo) {

	// Get our starting time as soon as possible
	double start = getRealTime();

	Limits limits;

	uint16_t bestMove, ponderMove;
	char moveStr[6];

	int depth = 0, infinite = 0;
	double wtime = 0, btime = 0, movetime = 0;
	double winc = 0, binc = 0, mtg = -1;

	int multiPV     = ((UCIGoStruct*)cargo)->multiPV;
	string str       = ((UCIGoStruct*)cargo)->str;
	Board *board    = ((UCIGoStruct*)cargo)->board;
	Thread *threads = ((UCIGoStruct*)cargo)->threads;

	// Grab the ready lock, as we cannot be ready until we finish this search
	pthread_mutex_lock(&READYLOCK);

	// Reset global signals
	IS_PONDERING = 0;

	// Init the tokenizer with spaces
	char* s = new char[BLOCK];
	strcpy(s,str.c_str());
	char* p = strtok(s, " ");

	// Parse any time control and search method information that was sent
	for (p = strtok(NULL, " "); p != NULL; p = strtok(NULL, " ")) {
		string ptr(p);
		if (ptr=="wtime") wtime = atoi(strtok(NULL, " "));
		else if (ptr=="btime") btime = atoi(strtok(NULL, " "));
		else if (ptr=="winc") winc = atoi(strtok(NULL, " "));
		else if (ptr=="binc") binc = atoi(strtok(NULL, " "));
		else if (ptr=="movestogo") mtg = atoi(strtok(NULL, " "));
		else if (ptr=="depth") depth = atoi(strtok(NULL, " "));
		else if (ptr=="movetime") movetime = atoi(strtok(NULL, " "));
		else if (ptr=="infinite") infinite = 1;
		else if (ptr=="ponder") IS_PONDERING = 1;
	}

	// Initialize limits for the search
	limits.limitedByNone  = infinite != 0;
	limits.limitedByTime  = movetime != 0;
	limits.limitedByDepth = depth    != 0;
	limits.limitedBySelf  = !depth && !movetime && !infinite;
	limits.timeLimit      = movetime;
	limits.depthLimit     = depth;

	// Pick the time values for the colour we are playing as
	limits.start = (board->turn == WHITE) ? start : start;
	limits.time  = (board->turn == WHITE) ? wtime : btime;
	limits.inc   = (board->turn == WHITE) ?  winc :  binc;
	limits.mtg   = (board->turn == WHITE) ?   mtg :   mtg;

	// Limit MultiPV to the number of legal moves
	limits.multiPV = MIN(multiPV, legalMoveCount(board));

	// Execute search, return best and ponder moves
	getBestMove(threads, board, &limits, &bestMove, &ponderMove);

	// UCI spec does not want reports until out of pondering
	while (IS_PONDERING);

	// Report best move ( we should always have one )
	moveToString(bestMove, moveStr, board->chess960);
	printf("bestmove %s ", moveStr);

	// Report ponder move ( if we have one )
	if (ponderMove != NONE_MOVE) {
		moveToString(ponderMove, moveStr, board->chess960);
		printf("ponder %s", moveStr);
	}

	// Make sure this all gets reported
	printf("\n"); fflush(stdout);

	// Drop the ready lock, as we are prepared to handle a new search
	pthread_mutex_unlock(&READYLOCK);

	return NULL;
}

void uciSetOption(string& str, Thread **threads, int *multiPV, int *chess960) {

	// Handle setting UCI options in Ethereal. Options include:
	//  Hash             : Size of the Transposition Table in Megabyes
	//  Threads          : Number of search threads to use
	//  MultiPV          : Number of search lines to report per iteration
	//  MoveOverhead     : Overhead on time allocation to avoid time losses
	//  SyzygyPath       : Path to Syzygy Tablebases
	//  SyzygyProbeDepth : Minimal Depth to probe the highest cardinality Tablebase
	//  UCI_Chess960     : Set when playing FRC, but not required in order to work

	size_t l;
	if (equStarts(str, "setoption name Hash value ", l)) {
		int megabytes = stoi(str.substr(l,9),nullptr);
		initTT(megabytes); printf("info string set Hash to %dMB\n", megabytes);
	}

	else if (equStarts(str, "setoption name Threads value ", l)) {
		int nthreads = stoi(str.substr(l,9),nullptr);
		free(*threads); *threads = createThreadPool(nthreads);
		printf("info string set Threads to %d\n", nthreads);
	}

	else if (equStarts(str, "setoption name MultiPV value ", l)) {
		*multiPV = stoi(str.substr(l,9),nullptr);
		printf("info string set MultiPV to %d\n", *multiPV);
	}

	else if (equStarts(str, "setoption name MoveOverhead value ", l)) {
		MoveOverhead = stoi(str.substr(l,9),nullptr);
		printf("info string set MoveOverhead to %d\n", MoveOverhead);
	}

	else if (equStarts(str, "setoption name SyzygyPath value ", l)) {
		const char *ptr = str.substr(l).c_str();
		tb_init(ptr); printf("info string set SyzygyPath to %s\n", ptr);
	}

	else if (equStarts(str, "setoption name SyzygyProbeDepth value ", l)) {
		TB_PROBE_DEPTH = stoi(str.substr(l,9),nullptr);
		printf("info string set SyzygyProbeDepth to %u\n", TB_PROBE_DEPTH);
	}

	else if (equStarts(str, "setoption name UCI_Chess960 value ", l)) {
		if (str.substr(l,4)=="true")
			printf("info string set UCI_Chess960 to true\n"), *chess960 = 1;
		else if (str.substr(l,5)=="false")
			printf("info string set UCI_Chess960 to false\n"), *chess960 = 0;
	}

	fflush(stdout);
}

void uciPosition(string& str, Board *board, int chess960) {

	int size;
	uint16_t moves[MAX_MOVES], u;
	char* p;
	string moveStr(6,0);
	char testStr[6];
	Undo undo[1];

	// Position is defined by a FEN, X-FEN or Shredder-FEN
	if ((p=strContains(str, "fen ", u)))	boardFromFEN(board, p+u, chess960);

	// Position is simply the usual starting position
	else if (strContains(str, "startpos"))
		boardFromFEN(board, StartPosition, chess960);


	// Position command may include a list of moves
	if (strsr(str,"moves ",u) != "")	str=str.substr(u);
	
	while (str!="") {
		// UCI sends moves in long algebraic notation
		moveStr=noTrail(str.substr(0,5))+'\0';
		str=str.substr(6);
		// Generate moves for this position
		size = 0;
		genAllLegalMoves(board, moves, &size);

		// Find and apply the given move
		for (int i = 0; i < size; i++) {
			moveToString(moves[i], testStr, board->chess960);
			string tests(testStr);
			if (moveStr==tests) {
				applyMove(board, moves[i], undo);
				break;
			}
		}
		// Reset move history whenever we reset the fifty move rule. This way
		// we can track all positions that are candidates for repetitions, and
		// are still able to use a fixed size for the history array (512)
		if (board->halfMoveCounter == 0)
			board->numMoves = 0;

		// Skip over all white space
		str=noLead(str);
		// while (*ptr == ' ') ptr++;
	}
}

void uciReport(Thread *threads, int alpha, int beta, int value) {

	// Gather all of the statistics that the UCI protocol would be
	// interested in. Also, bound the value passed by alpha and
	// beta, since Ethereal uses a mix of fail-hard and fail-soft

	int hashfull    = hashfullTT();
	int depth       = threads->depth;
	int seldepth    = threads->seldepth;
	int multiPV     = threads->multiPV + 1;
	int elapsed     = elapsedTime(threads->info);
	int bounded     = MAX(alpha, MIN(value, beta));
	uint64_t nodes  = nodesSearchedThreadPool(threads);
	uint64_t tbhits = tbhitsThreadPool(threads);
	int nps         = (int)(1000 * (nodes / (1 + elapsed)));

	// If the score is MATE or MATED in X, convert to X
	int score   = bounded >=  MATE_IN_MAX ?  (MATE - bounded + 1) / 2
					: bounded <= MATED_IN_MAX ? -(bounded + MATE)     / 2 : bounded;

	// Two possible score types, mate and cp = centipawns
	const char *type  = bounded >=  MATE_IN_MAX ? "mate"
					: bounded <= MATED_IN_MAX ? "mate" : "cp";

	// Partial results from a windowed search have bounds
	const char *bound = bounded >=  beta ? " lowerbound "
					: bounded <= alpha ? " upperbound " : " ";

	printf("info depth %d seldepth %d multipv %d score %s %d%stime %d "			"nodes %" PRIu64 " nps %d tbhits %" PRIu64 " hashfull %d pv ",			depth, seldepth, multiPV, type, score, bound, elapsed, nodes, nps, tbhits, hashfull);

	// Iterate over the PV and print each move
	for (int i = 0; i < threads->pv.length; i++) {
		char moveStr[6];
		moveToString(threads->pv.line[i], moveStr, threads->board.chess960);
		printf("%s ", moveStr);
	}

	// Send out a newline and flush
	puts(""); fflush(stdout);
}

void uciReportTBRoot(Board *board, uint16_t move, unsigned wdl, unsigned dtz) {

	char moveStr[6];

	// Convert result to a score. We place wins and losses just outside
	// the range of possible mate scores, and move further from them
	// as the depth to zero increases. Draws are of course, zero.
	int score = wdl == TB_LOSS ? -MATE + MAX_PLY + dtz + 1
				: wdl == TB_WIN  ?  MATE - MAX_PLY - dtz - 1 : 0;

	printf("info depth %d seldepth %d multipv 1 score cp %d time 0 "			"nodes 0 tbhits 1 nps 0 hashfull %d pv ",			MAX_PLY - 1, MAX_PLY - 1, score, 0);

	// Print out the given move
	moveToString(move, moveStr, board->chess960);
	puts(moveStr);
	fflush(stdout);
}

void uciReportCurrentMove(Board *board, uint16_t move, int currmove, int depth) {

	char moveStr[6];
	moveToString(move, moveStr, board->chess960);
	printf("info depth %d currmove %s currmovenumber %d\n", depth, moveStr, currmove);
	fflush(stdout);

}


// int strContains(char *str,const char *key) {	return strstr(str, key) != NULL;}
// inline int getInput(char *str) {

	// char *ptr;

	// if (fgets(str, 8192, stdin) == NULL)
		// return 0;

	// ptr = strchr(str, '\n');
	// if (ptr != NULL) *ptr = '\0';

	// ptr = strchr(str, '\r');
	// if (ptr != NULL) *ptr = '\0';

	// return 1;
// }
