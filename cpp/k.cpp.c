// #include <cassert>
// #include <climits>
// #include <cstdlib>
// #include <algorithm>
// #include <sstream>

#include <cstring>
// #include <cstdint>
// #include <cctype>
// #include <regex>
// #include <cstdlib>
// #include <cstdint>

#include <iostream>
using namespace std;

char* numberIn(string& s, char* n){
	const char* c=s.c_str();
	for(size_t i=0;i<=s.size();++i)
		if (isdigit(c[i])) {
			char* j=n;
			if (c[i-1]=='-') *j++='-';
			while (isdigit(*j++=c[i++]));
			*j=0;
			return n;
		}
	return NULL;
}
int numberIn(string& s){
	const char* c=s.c_str();
	char n[9];
	for(size_t i=0;i<=s.size();++i)
		if (isdigit(c[i])){
			char* j=n;
			if (c[i-1]=='-') *j++='-';
			while (isdigit(*j++=c[i++]));
			return atoi(n);
		}
	return 0;
}
struct l {
	int k;
	char m;
	int n;
};

int main(){
	// string::size_type sz;
	string st = "kol999setting";
	l o {};
	// char* s = new char[str.size()];
	// strcpy(s,str.c_str());
	cout<<"\no.k= "<<o.k<<"\n";
	cout<<"\no.m= "<<o.m<<"\n";
	cout<<"\no.n= "<<o.n<<"\n";


	// }

	// int i_dec = stoi (str_dec,&sz); 
	// int i_hex = stoi (str_hex,nullptr,16);
	// int i_bin = stoi (str_bin,nullptr,2);
	// int i_auto = stoi (str_auto,nullptr,0);

	// cout << str_dec << ": " << i_dec << " and [" << str_dec.substr(sz) << "]\n";
	// cout <<"\""<< str_hex << "\" : " << i_hex << '\n';
	// cout << str_bin << ": " << i_bin << '\n';
	// cout << str_auto << ": " << i_auto << '\n';
	// cout<<"string sz = "<<sz<<'\n';
// if ((t=numberIn(str)) != false)
		// cout<<"YES = "<<t<<"\n";
	// else
		// cout<<"No === "<<t<<"\n";
	// cout <<"info.values= "<<info.values[0]<<"\n" ;
	// cout <<"info.values= "<<info.values[1]<<"\n" ;
	// cout <<"info.values= "<<info.values[2]<<"\n" ;
	// cout <<"info.startTime= "<< info.startTime<<"\n";
	// smatch matches;
	//regex repat{};
	// if (regex_search(str_dec , matches, regex{R"(\d+)"}))
		// cout <<  "0 : " << matches[0] << '\n';
}