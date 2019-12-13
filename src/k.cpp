// #include <ccstdlib>
// #include <cstring>
// #include <stdio>
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

inline bool strContains(const char *str, const char *key) {	return strstr(str, key) != nullptr;}

inline bool strContains(string& s, const char* key, string& nxstr, size_t len=string::npos) {
	size_t f=s.find(key);
	return f==string::npos? (nxstr="",0) : (nxstr=s.substr(f+strlen(key), len),1);
}

// string& 
void k(string& *p){
	cout<<"\np0=="<< p[0]<<"=\n";
	cout<<"\np1 =="<< p[1]<<"=\n";
	cout<<"\np2 =="<< p[2]<<"=\n";
	
}

int main(){
	string p[]={
		"killh",
		"mkii",
		"oko"
	};
	k(p);

	// string o=p[0]+"OOO";
	// cout<<"\no =="<< o <<"\n";
	// cout<< (size_t)p <<"\n";
}