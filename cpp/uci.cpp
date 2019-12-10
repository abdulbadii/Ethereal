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
//false|true//\b(?:alignas|alignof|and|and_eq|asm|bitand|bitor|bool|catch|char16_t|char32_t|class|compl|const_cast|constexpr|decltype|delete|dynamic_cast|explicit|friend|inline|mutable|namespace|new|noexcept|not|not_eq|nullptr|operator|or_eq|private|protected|public|reinterpret_cast|static_assert|static_cast|template|this|thread_local|throw|try|typeid|typename|using|virtual|wchar_t|xor|xor_eq)\b
//(#include)\s+<((?!stdio|pthread|windows|math|time|unistd|stdbool|fcntl|\w+/\w).+?)\.h>	\1 <c\2>

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

pthread_mutex_t READYLOCK = PTHREAD_MUTEX_INITIALIZER;
string StartPosition="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// inline string strs(string& s, const char* key){	size_t f;	return (f=s.find(key))==string::npos? "": s.substr(f);}
// inline string& strs(string& s, const char* key){
	// size_t f=s.find(key);
	// return s=f==string::npos? "": s.substr(f);}
// inline string& strs(string& s, const char* key, size_t& u){
	// size_t f=s.find(key);	return s=f==string::npos? "": (u=strlen(key),s.substr(f));}
// inline string& strs(string& s, string& key, size_t& u){
	// size_t f=s.find(key);	return s=f==string::npos? "": (u=key.size(), s.substr(f));}

inline bool equStart(string& s, const char* key){	return !s.compare(0,strlen(key),key); }
inline bool equStart(string& s, const char* key, size_t& l){	return !s.compare(0,l=strlen(key),key); }
inline bool equStart(string& s, const char* key, string& nxstr){
	uint16_t l=strlen(key);	return !s.compare(0,l,key)? nxstr=s.substr(l), 1: 0;}

inline bool strContains(string& s, const char* key, string& nxstr, size_t len) {
	size_t f=s.find(key), b;
	return f==string::npos? (nxstr="", 0): (nxstr=s.substr(b=f+strlen(key), len), s=s.substr(b+len), 1);
}
inline bool strContains(string& s, const char* key, string& nxstr) {
	size_t f=s.find(key);
	return f==string::npos? (nxstr="", 0): (nxstr=s.substr(f+strlen(key)), 1);
}
inline bool strContains(string& s, const char* key, size_t& u) {
	size_t f=s.find(key);
	return f==string::npos? u=0: (u=f+strlen(key),1);
}
inline bool strContains(string& s, const char* key) {
	size_t f=s.find(key);
	return f==string::npos? 0: 1;
}

inline string& noTrail(string& s){
	size_t p=s.find_last_not_of(WHITESPACE);
	return s=p==string::npos? "": s.substr(0,p+1);}
inline string noTrail(string s){
		size_t p=s.find_last_not_of(WHITESPACE);
		return p==string::npos? "": s.substr(0,p+1);}
inline string& noLead(string& s){
	size_t p=s.find_first_not_of(WHITESPACE);
	return s=p==string::npos ? "": s.substr(p);}


void *uciGo(void *cargo) {

	// Get our starting time as soon as possible
	double start = getRealTime();

	Limits limits;

	uint16_t bestMove, ponderMove;
	string moveStr(6,0);

	int depth = 0, infinite = 0;
	double wtime = 0, btime = 0, movetime = 0;
	double winc = 0, binc = 0, mtg = -1;

	int multiPV     = ((UCIGoStruct*)cargo)->multiPV;
	char* str = new char[BLOCK];
	strcpy(str,(((UCIGoStruct*)cargo)->str).c_str());
	Board *board    = ((UCIGoStruct*)cargo)->board;
	Thread *threads = ((UCIGoStruct*)cargo)->threads;

	// Grab the ready lock, as we cannot be ready until we finish this search
	pthread_mutex_lock(&READYLOCK);

	// Reset global signals
	IS_PONDERING = 0;

	// Init the tokenizer with spaces
	char* p = strtok(str, " ");

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
	delete str;

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
	cout << "bestmove " << moveStr << " ";

	// Report ponder move ( if we have one )
	if (ponderMove != NONE_MOVE) {
		moveToString(ponderMove, moveStr, board->chess960);
		cout << "ponder " << moveStr;
	}

	// Make sure this all gets reported
	 cout << "\n"; fflush(stdout);

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

	size_t v;
	if (equStart(str, "setoption name Hash value ", v)) {
		int megabytes = stoi(str.substr(v),nullptr);
		initTT(megabytes); cout << "info string set Hash to " << megabytes << "MB\n";
	}

	else if (equStart(str, "setoption name Threads value ", v)) {
		int nthreads = stoi(str.substr(v),nullptr);
		free(*threads); *threads = createThreadPool(nthreads);
		cout << "info string set Threads to " << nthreads << "\n";
	}

	else if (equStart(str, "setoption name MultiPV value ", v)) {
		*multiPV = stoi(str.substr(v),nullptr);
		cout << "info string set MultiPV to " << *multiPV << "\n";
	}

	else if (equStart(str, "setoption name MoveOverhead value ", v)) {
		MoveOverhead = stoi(str.substr(v),nullptr);
		cout << "info string set MoveOverhead to " << MoveOverhead << "\n";
	}

	else if (equStart(str, "setoption name SyzygyPath value ", v)) {
		const char *ptr = str.substr(v).c_str();
		tb_init(ptr); cout << "info string set SyzygyPath to " << ptr << "\n";
	}

	else if (equStart(str, "setoption name SyzygyProbeDepth value ", v)) {
		TB_PROBE_DEPTH = stoi(str.substr(v),nullptr);
		cout << "info string set SyzygyProbeDepth to " << TB_PROBE_DEPTH << "\n";
	}

	else if (equStart(str, "setoption name UCI_Chess960 value ", v)) {
		if (str.substr(v,4)=="true")
			 cout << "info string set UCI_Chess960 to true\n", *chess960 = 1;
		else if (str.substr(v,5)=="false")
			 cout << "info string set UCI_Chess960 to false\n", *chess960 = 0;
	}

	fflush(stdout);
}
void uciPosition(string& str, Board *board, int chess960) {
	
	string str8kb(16*BLOCK,0);
	uint16_t size, moves[MAX_MOVES];
	string moveStr(6,0),testStr(6,0);
	Undo undo[1];
	size_t i;
	
	// Position is defined by a FEN, X-FEN or Shredder-FEN
	if (strContains(str, "fen ", str8kb))
		boardFromFEN(board, str8kb, chess960);

	// Position is simply the usual starting position
	if (strContains(str, "startpos"))
		boardFromFEN(board, StartPosition, chess960);
	
	// Position command may include a list of moves
	if (strContains(str, "moves ", moveStr, 5))
	// Apply each move in the move list
	while(moveStr[0]) {

		// UCI sends moves in long algebraic notation
		if (moveStr[4] == ' ') moveStr[4] = 0;

		// Generate moves for this position
		size = 0; genAllLegalMoves(board, moves, size);

		// Find and apply the given move
		for (i=0; i < size; i++) {
				moveToString(moves[i], testStr, board->chess960);
				if (moveStr==testStr) {
					applyMove(board, moves[i], undo);
					break;
				}
		}
		// Reset move history whenever we reset the fifty move rule. This way
		// we can track all positions that are candidates for repetitions, and
		// are still able to use a fixed size for the history array (512)
		if (board->halfMoveCounter == 0)	board->numMoves = 0;

		// Skip over all white space
		i=0;
		while (str[++i] == ' ');
		str = str.substr(i);
		moveStr = str.substr(0,5);
	};
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

	cout << "info depth " << depth << " seldepth " << seldepth << " multipv " << multiPV << " score " << type << " " << score << bound << "time " << elapsed << " nodes " << nodes << " nps " << nps << " tbhits " << tbhits << " hashfull " << hashfull << " pv ";

	// Iterate over the PV and print each move
	for (int i = 0; i < threads->pv.length; i++) {
		string moveStr(6,0);
		moveToString(threads->pv.line[i], moveStr, threads->board.chess960);
		cout << moveStr << " ";
	}

	// Send out a newline and flush
	puts(""); fflush(stdout);
}

void uciReportTBRoot(Board *board, uint16_t move, unsigned wdl, unsigned dtz) {
	string moveStr(6,0);
	// Convert result to a score. We place wins and losses just outside
	// the range of possible mate scores, and move further from them
	// as the depth to zero increases. Draws are of course, zero.
	int score = wdl == TB_LOSS ? -MATE + MAX_PLY + dtz + 1
				: wdl == TB_WIN  ?  MATE - MAX_PLY - dtz - 1 : 0;

cout << "info depth " << MAX_PLY - 1 << " seldepth " << MAX_PLY - 1 << " multipv 1 score cp " << score << " time 0 nodes 0 tbhits 1 nps 0 hashfull " << 0 << " pv ";

	// Print out the given move
	moveToString(move, moveStr, board->chess960);
	cout << moveStr;
	fflush(stdout);
}

void uciReportCurrentMove(Board *board, uint16_t move, int currmove, int depth) {
	string moveStr(6,0);
	moveToString(move, moveStr, board->chess960);
	cout << "info depth " << depth << " currmove " << moveStr << " currmovenumber " << currmove << "\n";
	fflush(stdout);
}

int main(int argc, char* argv[]) {
	Board board;
	string str(16*BLOCK,0);
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
		size_t u;
		if (str=="uci") {
				 cout << "id name Ethereal " ETHEREAL_VERSION "\n";
				 cout << "id author Andrew Grant & Laldon\n";
				 cout << "option name Hash ; spin default 16 min 1 max 65536\n";
				 cout << "option name Threads type spin default 1 min 1 max 2048\n";
				 cout << "option name MultiPV type spin default 1 min 1 max 256\n";
				 cout << "option name MoveOverhead type spin default 100 min 0 max 10000\n";
				 cout << "option name SyzygyPath type string default <empty>\n";
				 cout << "option name SyzygyProbeDepth type spin default 0 min 0 max 127\n";
				 cout << "option name Ponder type check default false\n";
				 cout << "option name UCI_Chess960 type check default false\n";
				 cout << "uciok\n";
		}

		else if (str=="isready")
				 cout << "readyok\n";

		else if (str=="ucinewgame")
				resetThreadPool(threads), clearTT();

		else if (equStart(str, "setoption"))
				uciSetOption(str, &threads, &multiPV, &chess960);

		else if (equStart(str, "position"))
				uciPosition(str, &board, chess960);

		else if (equStart(str, "go")) {
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
		else if (str=="quit")	break;

		else if (equStart(str, "perft ", u))
				cout<< perft(&board, stoi(str.substr(u),nullptr));
		else if (equStart(str, "print"))
				printBoard(&board), fflush(stdout);
	}

	return 0;
}
