/*
 * damsdecode
 *
 * A tool to convert DAMS source files binary format to plain text
 * and the opposite.
 *
 *  Copyright 2015 Pascal Séguy	<pascal.seguy@laposte.net>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  Build notes
 *  -----------
 *
 *  This program is written in C++ as a single file, it should
 *  compile on any platform. libboost is required.
 *
 *  Typical Linux command line to build it is:
 *
 *  	g++ -Wall damsdecode.cpp -o damsdecode
 *
 *
 *
 *	Purpose
 *	-------
 *
 *	I made this tool first to import DAMS' itself source files on my
 *	Linux host. Then I added the 'encode' feature to be able to export
 *	my edited sources back to the CPC for assembly by DAMS.
 *	By default, chained files (sources ending with a '*F,file'), are
 *	processed during both decoding/encoding, giving only one plain text
 *	file useful for editing, file is split back by the encoder.
 *
 *	NOTE: by default the encode procedure strips all comments, this
 *	allow to comment any source file without the target platform memory
 *	constraints, but has the drawback that sources cannot be re-imported
 *	(all comments would be lost).
 *
 *	Be careful on host editing to not use any non ASCII characters in
 *	symbols name, no check is done by damsdecode. Comments may contain
 *	any unicode characters since they are stripped by the encoder, unless
 *	you explicitly specify to let them go into the encoded file.
 *
 *
 *	Usage
 *	-----
 *
 *	Import your DAMS source file from your CPC to your host and
 *	decode it:
 *
 *		damsdecode <mydamssrc.src >mydamssrc.txt
 *
 *	Then edit mydamssrc.txt, do some changes and recode it:
 *
 *		damsdecode -e -o mydamssrc.src <mydamssrc.txt
 *
 *	Then export it back to your CPC for assembly.
 *
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cerrno>
#include <cstdlib>		// exit
#include <cstring>		// strerror
#include <cassert>
#include <boost/algorithm/string.hpp>	// to_upper

using namespace std;

//
// A list of DAMS mnemonics
// (implemented as a vector)
//
typedef vector<string> mnemotab_t;
mnemotab_t mnemotab;

//
// A map mnemonics
// and token value
//
typedef map<string, int> mnemomap_t;
mnemomap_t mnemomap;

//
// mnemonics data source (was in a file)
//
extern const char *mnemolist[];



void usage(int rc = 1)
{
	cerr << "damsdecode [option] <filein" << endl;
	cerr << "\t-e\t: encode to DAMS format" << endl;
	cerr << "\t-c\t: keep comments in encoded file (encode only)" << endl;
	cerr << "\t-F\t: Don't execute '*F,' directives (disable file chaining)" << endl;
	cerr << "\t-o ofile\t: set ouptut file name for encoder" << endl;
	cerr << "\t-S\t: on decode, skip the first 128 bytes" << endl;
	exit(rc);
}



int main(int argc, char *argv[])
{
int carg = 1;
bool encode = false;
bool skiphdr = false;
bool keepcomments = false;
bool nofollow = false;
const char *ofname = "damsencode.out";
ifstream inputfile;
ofstream outputfile;
string nextfile;

	//
	// Parse command line
	//
	while(carg < argc && argv[carg][0] == '-') {

		switch(argv[carg][1]) {

			case 'e':		// encode
				encode = true;
			break;

			case 'c':		// keep comments in encoded file
				keepcomments = true;
			break;

			case 'F':		//
				nofollow = true;
			break;

			case 'o':		// set output file name
				if(argv[carg][2]) {
					ofname = &argv[carg][2];
				}else if(carg + 1 < argc) {
					ofname = argv[++carg];
				}else{
					usage();
				}
			break;

			case 'S':		// skip
				skiphdr = true;
			break;

			case '?':
				usage(0);
			break;

			default:
			usage();
		}
		++carg;
	}

	//
	// Read DAMS mnemonics
	//
	for(int idx = 0; mnemolist[idx]; ++idx) {
		string mnemo = mnemolist[idx];
		mnemotab.push_back(mnemo);
		mnemomap[mnemo] = idx;
	}
	if(false) cerr << mnemotab.size() << " keywords loaded" << endl;	// debug

	int lnr = 0;	// line counter

	if(encode) {
		//
		// read a text file, convert it to DAMS format
		//
		ofstream cout;
		cout.open(ofname, ios_base::binary | ios_base::out);

		while(!cin.eof()) {
			stringbuf bline;	// line buffer
			if(cin.get(bline).eof()) break;	// read a line
			if(cin.fail()) cin.clear();	// empty line, reset error flag
			cin.get();	// skip the delimiting character
			++lnr;		// line counter

			// cerr << bline.str() << endl;

			//
			// Parse one line
			//
			{ istringstream cin(bline.str());	// hide std::cin
				char ch;
				string label;
				string mnemo;
				bool gotcomment = false;

				//
				// Fetch Label
				//
				while(cin.get(ch)) {
					if(ch == ';' || isspace(ch)) break;
					label += ch;
				}
				cout << label;	// output it

				if(cin) cin.putback(ch);	// unget the next character
				cin >> ws;	// eat whites

				//
				// Fetch mnemo
				//
				while(cin.get(ch)) {
					if(ch == ';' || isspace(ch)) break;
					mnemo += ch;
				}
				if(cin) cin.putback(ch);	// unget the next character
				cin >> ws;	// eat whites

				if(!mnemo.empty()) {
					boost::to_upper(mnemo);
					if(false) cerr << '[' << mnemomap[mnemo] << ']';	// debug

					//
					// find the DAMS token for this mnemonic
					//
					mnemomap_t::const_iterator iter;
					if((iter = mnemomap.find(mnemo)) == mnemomap.end()) {
						cerr << "line " << lnr << " invalid mnemo '" << mnemo << "'" << endl;
						exit(1);
					}
					cout.put((char)((*iter).second | 0X80));	// Output the DAMS' token

					//
					// fetch arguments.
					//
					if(mnemo == "DEFM") {
						//
						// I think that no comments are allowed in a DEFM,
						// should verify
						while(cin.get(ch)) cout.put(ch);	// pass-thru
						//cout << cin.rdbuf();	// THIS won't work on empty line
					}else{
						bool quoted = false;
						int nquoted = 0;
						while(cin.get(ch)) {
							if(quoted) {
								if(nquoted == 0) {	// if first char after a quote
									++nquoted;
									cout.put(ch);
								}else if(nquoted == 1) {	// second char
									quoted = false;			// auto stop quoting
									if(ch == '"') {
										cout.put(ch);
									}else{
										cin.putback(ch);	// miss the terminating '"', it's allowed
									}
								}
							}else{	// not quoted
								if(ch == '"') {
									quoted = true;
									nquoted = 0;
									cout.put(ch);
								}else if(ch == ';') {	// a comment ?
									cin.putback(ch);
									break;	// end of args loop
								}else{
									if(!isspace(ch)) {
										// If we are in a DEFM, spaces are significants
										cout.put(ch);
									}
								}
							}
						}
					}
				}

				if(cin && cin.get(ch)) {
					if(ch == ';') {
						if(keepcomments) {
							cout.put(0xff);
							gotcomment = true;
							if(true) {
								while(cin.get(ch)) cout.put(ch);	// pass-thru
							}else{
								cout << cin.rdbuf();	// This destroy cout on empty comments!!
							}
						}else{
							if(label.empty() && mnemo.empty()) {
								// output the comment token only on empty lines
								cout.put(0xff);
								gotcomment = true;
							}
						}
					}else{
						cin.putback(ch);
						cerr << "line " << lnr << ": unexpected extra characters: '" << cin.rdbuf() << "'" << endl;
						exit(1);
					}
				}
				if(label.empty() && mnemo.empty() && !gotcomment) {	// empty line ?
					cout.put(0xff);
					gotcomment = true;
				}
				cout.put(0x0D);	// DAMS eol

				if(label.substr(0,3) == "*F," && !nofollow) {
					nextfile = label.substr(3);
					cout.put(0);	// DAMS end of file
					cout.close();
					if(cout.fail()) {
						cerr << "file write error: " << strerror(errno) << endl;
						exit(1);
					}
					cout.open(nextfile.c_str(), ios_base::binary | ios_base::out);
					cerr << "Next file: " << nextfile << endl;
					nextfile.clear();
				}
			}
		}
		cout.put(0);	// DAMS end of file
		cout.close();
		if(cout.fail()) {
			cerr << "file write error: " << strerror(errno) << endl;
			exit(1);
		}

	}else{
		//
		// decode: read a DAMS source file,
		// output it in text
		//

		while(cin) {	// while any file

			//
			// I grabbed DAMS sources files from a 3" floppy
			// in 1999,  I can't remember why the files
			// have a 128 bytes binary header that must
			// be ignored.
			if(skiphdr) cin.seekg(0x80);

			while(cin.peek() != 0) {
				istream &cinfile(cin);	// make an alias of cin
				//stringbuf bline;
				//if(!cin.get(bline, '\r')) break;	// read a line
				//cin.get();	// skip the delimiting character
				char buf[4000];
				if(!cin.getline(buf, sizeof(buf), '\r')) {	// read a line
					cerr << "getline failed" << endl;
					exit(1);
				}
				++lnr;
				{ istringstream cin(buf /*bline.str()*/);		// cin is a shortcut on curent line
					char ch;
					enum { in_label, in_operand, in_comment } step = in_label;
					int col = 0;
					string label;

					while(cin.get(ch)) {
						if(ch == 0) {
							cerr << "line " << lnr << ": unexpected NULL character" << endl;
							if(skiphdr) {
								// There is some garbage at end of my DAMS original files.
								// Treat this as a end of file
								cinfile.setstate(ios_base::eofbit);
								break;
							}
							exit(1);

						}
						if(ch > 0) {
							if(step == in_label) {
								label += ch;
							}
							if(step == in_operand && col == 0) {
								cout << '\t';
							}
							cout << ch;
							++col;
						}else{	// ch < 0
							if(step == in_comment) {
								cout << "(0x" << hex << setw(2) << setfill('0') << (unsigned char)ch << ')'; // debug

							}else{
								if(ch == -1) {		// comment
									switch(step) {
									case in_label:
										if(col > 0) {
											if(label.length() < 8) cout << "\t";
											cout << "\t\t\t";
										}
									break;
									case in_operand:
										if(col == 0) {
											cout << "\t\t\t";
										}else if(col < 8) {
											cout << "\t\t";
										}else if(col < 16) {
											cout << "\t";
										}else {
											cout << ' ';
										}
									break;
									case in_comment: abort();
									}
									cout << "; ";
									step = in_comment;
								}else{
									unsigned index = (unsigned char)ch & 0x7F;
									if(index >= (unsigned int)mnemotab.size()) {
										cerr << "unexpected token " << index << endl;
										exit(1);
									}
									if(label.length() < 8) {
										cout << "\t" ;
									}else{
										cout << ' ' ;
									}
									cout << mnemotab[index];
									step = in_operand;
								}
								col = 0;
							}
						}
					}
					cout << endl;

					if(label.substr(0, 3) == "*F," && !nofollow) {
						nextfile = label.substr(3);
					}
				}
			}

			if(nextfile.empty()) {
				cin.setstate(ios_base::failbit);
			}else{
				inputfile.close();
				inputfile.open(nextfile/* + ".BIN")*/.c_str(), ios_base::in | ios_base::binary);
				if(!inputfile) {
					cerr << "can't open " << nextfile << endl;
					exit(1);
				}
				cin.rdbuf(inputfile.rdbuf());
				cerr << "Next file: " << nextfile << endl;
				nextfile.clear();
			}
		}
	}
	return(0);
}


const char *mnemolist[] = {
"LD",
"INC",
"DEC",
"ADD",
"ADC",
"SUB",
"SBC",
"AND",
"XOR",
"OR",
"CP",
"PUSH",
"POP",
"BIT",
"RES",
"SET",
"RLC",
"RRC",
"RL",
"RR",
"SLA",
"SRA",
"SRL",
"IN",
"OUT",
"RST",
"DJNZ",
"EX",
"IM",
"JR",
"CALL",
"RET",
"JP",
"NOP",
"RLCA",
"RRCA",
"RLA",
"RRA",
"DAA",
"CPL",
"SCF",
"CCF",
"HALT",
"EXX",
"DI",
"EI",
"NEG",
"RETN",
"RETI",
"RRD",
"RLD",
"LDI",
"CPI",
"INI",
"OUTI",
"LDD",
"CPD",
"IND",
"OUTD",
"LDIR",
"CPIR",
"INIR",
"OTIR",
"LDDR",
"CPDR",
"INDR",
"OTDR",
"DEFB",
"DEFW",
"DEFM",
"DEFS",
"EQU",
"ORG",
"ENT",
"IF",
"ELSE",
"END",
0
};
