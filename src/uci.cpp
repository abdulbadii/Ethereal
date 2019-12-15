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

char WHITESPACE[]={" \n\r\t\f\v"};
pthread_mutex_t READYLOCK = PTHREAD_MUTEX_INITIALIZER;
#include <iostream>
using namespace std;
const string StartPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
string nextr(8191,0);

inline string& strs(string& s, const char* key){
	size_t f=s.find(key);	return s=f==string::npos? "": s.substr(f);}
inline string& strs(string& s, const char* key, size_t& u){
	size_t f=s.find(key);	return s=f==string::npos? "": (u=strlen(key),s.substr(f));}
inline string& strs(string& s, string& key, size_t& u){
	size_t f=s.find(key);	return s=f==string::npos? "": (u=key.size(), s.substr(f));}

inline bool equStart(string& s, const char* key, string& nxstr){
	uint16_t l=strlen(key);	return !s.compare(0,l,key)? nxstr=s.substr(l), 1: 0;}
inline bool equStart(string& s, const char* key){	return !s.compare(0,strlen(key),key); }
inline bool equStart(string& s, const char* key, size_t& l){	return !s.compare(0,l=strlen(key),key); }

inline bool strContains(string& s, const char* key, string& nxstr, size_t len) {
	size_t f=s.find(key), b;
	return f==string::npos? (nxstr="", 0): (nxstr=s.substr(b=f+strlen(key), len), s=s.substr(b+len), 1);
}
inline bool strContains(string& s, const char* key, string& nxstr) {
	size_t f=s.find(key);
	return f==string::npos? (nxstr="", 0): (nxstr=s.substr(f+strlen(key)), 1);
}
inline bool strContains(string& s, const char* key) {
	size_t f=s.find(key);
	return f==string::npos? 0: 1;
}
inline string& trTrail(string& s){
	size_t p=s.find_last_not_of(WHITESPACE);
	return s=p==string::npos? "": s.substr(0,p+1);}
inline string& trLead(string& s){
	size_t p=s.find_first_not_of(WHITESPACE);
	return s=p==string::npos ? "": s.substr(p);}

int strEquals(char *str1, const char *str2) {	return !strcmp(str1, str2);}
int strStartsWith(char *str, const char *key) {	return strstr(str, key) == str;}
int strContains(char *str, const char *key) {	return strstr(str, key) != nullptr;}

void *uciGo(void *cargo) {

	// Get our starting time as soon as possible
	double start = getRealTime();

	Limits limits;

	uint16_t bestMove, ponderMove;
	char moveStr[6];

	int depth = 0, infinite = 0;
	double wtime = 0, btime = 0, movetime = 0, winc = 0, binc = 0,
	mtg = -1;

	int multiPV     = ((UCIGoStruct*)cargo)->multiPV;
	char *str       = ((UCIGoStruct*)cargo)->str;
	Board board    = ((UCIGoStruct*)cargo)->board;
	Thread *threads = ((UCIGoStruct*)cargo)->threads;

	// Grab the ready lock, as we cannot be ready until we finish this search
	pthread_mutex_lock(&READYLOCK);

	// Reset global signals
	IS_PONDERING = 0;

	// Init the tokenizer with spaces
	char* p = strtok(str, " ");

	// Parse any time control and search method information that was sent
	for (p = strtok(NULL, " "); p != nullptr; p = strtok(NULL, " ")) {
		string ptr=p;
		if (ptr=="wtime") wtime = atoi(strtok(NULL, " "));
		if (ptr=="btime") btime = atoi(strtok(NULL, " "));
		if (ptr=="winc") winc = atoi(strtok(NULL, " "));
		if (ptr=="binc") binc = atoi(strtok(NULL, " "));
		if (ptr=="movestogo") mtg = atoi(strtok(NULL, " "));
		if (ptr=="depth") depth = atoi(strtok(NULL, " "));
		if (ptr=="movetime") movetime = atoi(strtok(NULL, " "));
		if (ptr=="infinite") infinite = 1;
		if (ptr=="ponder") IS_PONDERING = 1;
	}

	// Initialize limits for the search
	limits.limitedByNone  = infinite != 0;
	limits.limitedByTime  = movetime != 0;
	limits.limitedByDepth = depth    != 0;
	limits.limitedBySelf  = !depth && !movetime && !infinite;
	limits.timeLimit      = movetime;
	limits.depthLimit     = depth;

	// Pick the time values for the colour we are playing as
	limits.start = start;
	limits.time  = (board.turn == WHITE) ? wtime : btime;
	limits.inc   = (board.turn == WHITE) ?  winc :  binc;
	limits.mtg   = mtg;

	// Limit MultiPV to the number of legal moves
	limits.multiPV = MIN(multiPV, legalMoveCount(board));

	// Execute search, return best and ponder moves
	getBestMove(threads, board, &limits, bestMove, ponderMove);

	// UCI spec does not want reports until out of pondering
	while (IS_PONDERING);

	// Report best move ( we should always have one )
	moveToString(bestMove, moveStr, board.chess960);
	cout << "bestmove " << moveStr << " ";

	// Report ponder move ( if we have one )
	if (ponderMove != NONE_MOVE) {
		moveToString(ponderMove, moveStr, board.chess960);
		cout << "ponder " << moveStr;
	}

	// Make sure this all gets reported
	cout << "\n"; fflush(stdout);

	// Drop the ready lock, as we are prepared to handle a new search
	pthread_mutex_unlock(&READYLOCK);

	return nullptr;
}

void uciSetOption(string& str, Thread *threads, int& multiPV, int& chess960) {

	// Handle setting UCI options in Ethereal. Options include:
	//  Hash             : Size of the Transposition Table in Megabyes
	//  Threads          : Number of search threads to use
	//  MultiPV          : Number of search lines to report per iteration
	//  MoveOverhead     : Overhead on time allocation to avoid time losses
	//  SyzygyPath       : Path to Syzygy Tablebases
	//  SyzygyProbeDepth : Minimal Depth to probe the highest cardinality Tablebase
	//  UCI_Chess960     : Set when playing FRC, but not required in order to work

	if (equStart(str, "setoption name Hash value ", nextr)) {
		int megabytes = stoi(nextr);
		initTT(megabytes); cout << "info string set Hash to " << megabytes << "MB\n";
	}

	if (equStart(str, "setoption name Threads value ", nextr)) {
		int nthreads = stoi(nextr);
		free(threads); threads = createThreadPool(nthreads);
		cout << "info string set Threads to " << nthreads << "\n";
	}

	if (equStart(str, "setoption name MultiPV value ", nextr)) {
		multiPV = stoi(nextr);
		cout << "info string set MultiPV to " << multiPV << "\n";
	}

	if (equStart(str, "setoption name MoveOverhead value ", nextr)) {
		MoveOverhead = stoi(nextr);
		cout << "info string set MoveOverhead to " << MoveOverhead << "\n";
	}

	if (equStart(str, "setoption name SyzygyPath value ", nextr)) {
		char *ptr = &nextr[0];
		tb_init(ptr); cout << "info string set SyzygyPath to " << ptr << "\n";
	}

	if (equStart(str, "setoption name SyzygyProbeDepth value ", nextr)) {
		TB_PROBE_DEPTH = stoi(nextr);
		cout << "info string set SyzygyProbeDepth to " << TB_PROBE_DEPTH << "\n";
	}

	if (equStart(str, "setoption name UCI_Chess960 value ", nextr)) {
		if (equStart(nextr, "true"))
				cout << "info string set UCI_Chess960 to true\n", chess960 = 1;
		else if (equStart(nextr, "false"))
				cout << "info string set UCI_Chess960 to false\n", chess960 = 0;
	}
	fflush(stdout);
}

void uciPosition(string& str, Board& board, int chess960) {

	int size;
	uint16_t moves[MAX_MOVES];
	char *ptr, moveStr[6],testStr[6];
	Undo undo[1];

	// Position is defined by a FEN, X-FEN or Shredder-FEN
	if (strContains(str, "fen ", nextr))
		boardFromFEN(board, nextr, chess960);

	// Position is simply the usual starting position
	else if (strContains(str, "startpos"))
		boardFromFEN(board, StartPosition, chess960);

	char* s=&str[0];

	// Position command may include a list of moves
	ptr = strstr(s, "moves");
	if (ptr != nullptr)
		ptr += strlen("moves ");

	// Apply each move in the move list
	while (ptr != nullptr && *ptr != '\0') {

		// UCI sends moves in long algebraic notation
		for (int i = 0; i < 4; ++i) moveStr[i] = *ptr++;
		moveStr[4] = *ptr == '\0' || *ptr == ' ' ? '\0' : *ptr++;
		moveStr[5] = '\0';

		// Generate moves for this position
		size = 0; genAllLegalMoves(board, moves, size);

		// Find and apply the given move
		for (int i = 0; i < size; ++i) {
				moveToString(moves[i], testStr, board.chess960);
				if (strEquals(moveStr, testStr)) {
					applyMove(board, moves[i], undo);
					break;
				}
		}

		// Reset move history whenever we reset the fifty move rule. This way
		// we can track all positions that are candidates for repetitions, and
		// are still able to use a fixed size for the history array (512)
		if (board.halfMoveCounter == 0)
				board.numMoves = 0;

		// Skip over all white space
		while (*ptr == ' ') ptr++;
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
	int nps         = int(1000 / (1 + elapsed) * nodes);

	// If the score is MATE or MATED in X, convert to X
	int score   = bounded >=  MATE_IN_MAX ?  (MATE - bounded + 1) / 2
					: bounded <= MATED_IN_MAX ? -(bounded + MATE)     / 2 : bounded;

	// Two possible score types, mate and cp = centipawns
	const char *type  = bounded >=  MATE_IN_MAX ? "mate"
					: bounded <= MATED_IN_MAX ? "mate" : "cp";

	// Partial results from a windowed search have bounds
	const char *bound = bounded >=  beta ? " lowerbound "
					: bounded <= alpha ? " upperbound " : " ";

	cout << "info depth " << depth << " seldepth " << seldepth << " multipv " << multiPV << " score " << type << " " << score << bound << "time " << elapsed << " nodes " << nodes << " nps " << nps << " tbhits " << tbhits << " hashfull " << hashfull << " pv ";

	// Iterate over the PV and print each move
	for (int i = 0; i < threads->pv.length; ++i) {
		char moveStr[6];
		moveToString(threads->pv.line[i], moveStr, threads->board.chess960);
		cout << moveStr << " ";
	}

	// Send out a newline and flush
	puts(""); fflush(stdout);
}

void uciReportTBRoot(Board& board, uint16_t move, unsigned wdl, unsigned dtz) {

	char moveStr[6];

	// Convert result to a score. We place wins and losses just outside
	// the range of possible mate scores, and move further from them
	// as the depth to zero increases. Draws are of course, zero.
	int score = wdl == TB_LOSS ? -MATE + MAX_PLY + dtz + 1
				: wdl == TB_WIN  ?  MATE - MAX_PLY - dtz - 1 : 0;

	cout << "info depth " << MAX_PLY - 1 << " seldepth " << MAX_PLY - 1 << " multipv 1 score cp " << score << " time 0 nodes 0 tbhits 1 nps 0 hashfull " << 0 << " pv ";

	// Print out the given move
	moveToString(move, moveStr, board.chess960);
	puts(moveStr);
	fflush(stdout);
}

void uciReportCurrentMove(Board& board, uint16_t move, int currmove, int depth) {

	char moveStr[6];
	moveToString(move, moveStr, board.chess960);
	cout << "info depth " << depth << " currmove " << moveStr << " currmovenumber " << currmove << "\n";
	fflush(stdout);

}


int main(int argc, char* argv[]) {

	Board board;
	string str(8192,0);
	Thread *threads;
	pthread_t pthreadsgo;
	UCIGoStruct uciGoStruct;

	int chess960 = 0, multiPV  = 1;

	// Initialize core components of Ethereal
	initAttacks();
	initMasks();
	initEval();
	initSearch();
	initZobrist();
	initTT(16);
	threads = createThreadPool(1);
	boardFromFEN(board, StartPosition, chess960);
	
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
		if (str=="uci") {
			cout << "id name Ethereal  ETHEREAL_VERSION \n";
			cout << "id author Andrew Grant & Laldon\n";
			cout << "option name Hash type spin default 16 min 1 max 65536\n";
			cout << "option name Threads type spin default 1 min 1 max 2048\n";
			cout << "option name MultiPV type spin default 1 min 1 max 256\n";
			cout << "option name MoveOverhead type spin default 100 min 0 max 10000\n";
			cout << "option name SyzygyPath type string default <empty>\n";
			cout << "option name SyzygyProbeDepth type spin default 0 min 0 max 127\n";
			cout << "option name Ponder type check default false\n";
			cout << "option name UCI_Chess960 type check default false\n";
			cout << "uciok\n", fflush(stdout);
		}

		else if (str=="isready")	cout << "readyok\n", fflush(stdout);

		else if (str=="ucinewgame")
				resetThreadPool(threads), clearTT();

		else if (equStart(str, "setoption"))
				uciSetOption(str, threads, multiPV, chess960);

		else if (equStart(str, "position"))
				uciPosition(str, board, chess960);

		else if (equStart(str, "go")) {
				strncpy(uciGoStruct.str, &str[0], 512);
				uciGoStruct.multiPV = multiPV;
				uciGoStruct.board   = board;
				uciGoStruct.threads = threads;
				pthread_create(&pthreadsgo, nullptr, &uciGo, &uciGoStruct);
		}
		else if (str=="ponderhit")	IS_PONDERING = 0;

		else if (str=="stop") {
				ABORT_SIGNAL = 1, IS_PONDERING = 0;
				pthread_join(pthreadsgo, nullptr);
		}
		else if (str=="quit")	break;

		else if (equStart(str, "perft ", nextr))
				cout << "%\n" << perft(board, stoi(nextr)), fflush(stdout);
		else if (equStart(str, "print"))
				printBoard(board), fflush(stdout);
	}
	return 0;
}

