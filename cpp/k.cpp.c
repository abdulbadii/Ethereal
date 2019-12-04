// #include <cstdlib>
// #include <cstring>
// #include <stdio.h>
#include <cstring>
#include <iostream>  
// #include <sstream>
using namespace std;

const char* WHITESPACE = " \n\r\t\f\v";


inline string& strsr(string& s, const char* key){
	uint16_t f;
	return s=(f=s.find(key))==string::npos? "": s.substr(f);
}
inline string& strsr(string& s, char* key, uint16_t& u){
	uint16_t f;
	return s=(f=s.find(key))==string::npos? "": (u=strlen(key),s.substr(f));
}
inline string& strsr(string& s, string& key, uint16_t& u){
	uint16_t f;
	return s=(f=s.find(key))==string::npos? "": (u=key.size(),s.substr(f));}

int main(){
	string str = "wtime btime winc binc movestogo depth movetime infinite ponder";
	string k(32,0);
	// char k[32];

	int depth=0, seldepth=0, multiPV=0;
	printf("info depth %d seldepth %d" "multipv %d", depth, seldepth, multiPV);


	// str=str.substr(u);
	// cout<<"\nu= "<<u<<"\nstr =="<<str<<"===\n";


}