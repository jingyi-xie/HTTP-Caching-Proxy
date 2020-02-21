#include <iostream>
#include <fstream>
#include <assert.h>

#include "httpparser.hpp"
#include "../log.hpp"

using namespace std;
using namespace zq29;

vector<char> buildCharVec(const string& str) {
	return vector<char>(str.begin(), str.end());
}
string buildStrFromCharVec(const vector<char>& v) {
	return string(v.begin(), v.end());
}

void testNonNegHexStrToInt() {
	static const string TAG = "testNonNegHexStrToInt";
	bool failFlag = false;

	const vector<string> hexStrs = {
		"", "-1", "0", "1", "f", "F",
		"1f", "-1f", "ff",
		" ", "r", "rr", "1r",
		"0000"
	};
	const vector<int> ints = {
		-1, -1, 0, 1, 15, 15,
		31, -1, 255,
		-1, -1, -1, -1,
		0
	};
	assert(hexStrs.size() == ints.size());
	for(size_t i = 0; i < hexStrs.size(); i++) {
		const int res = zq29Inner::nonNegHexStrToInt(hexStrs[i]);
		if(res != ints[i]) {
			Log::testFail(TAG, Log::msg("case <", hexStrs[i], ">, got <", res, ">"));
			failFlag = true;
		}
	}
	if(!failFlag) { Log::testSuccess(TAG); }
}

class HTTPParserTest : public HTTPParser {
public:
	void parseMessageBody() override { }

	void testGetCRLFLine() {
		static const string TAG = "testGetCRLFLine";
		bool failFlag = false;
		vector<char> buffer;

		vector<string> illegalCases = {
			"", "1", "\n", "\r",
			"\n2", "3\n", "\n4\n",
			"\r5", "6\r", "\r7\r",
			"8\n\r", "9\n\r\n", "illegalCases\n",
			"illegalCases\r", "illegalCases\n\r"
		};

		for(const string& s : illegalCases) {
			buffer = buildCharVec(s);
			setBuffer(buffer);
			try {
				getCRLFLine();
				Log::testFail(TAG, "case <" + s + ">");
				failFlag = true;
			} 
			catch(HTTPBadMessageException e) {}
			catch(HTTPParserException e) {}
		}

		vector<string> legalCases = {
			"\r\n", "something\r\n",
			"\r\nsomething", "something\r\nanotherthing",
			"\r\n\r", "\r\n\n"
		};
		vector<string> results = {
			"", "something",
			"", "something",
			"", ""
		};
		vector<string> bufferStates = {
			"", "",
			"something", "anotherthing",
			"\r", "\n"
		};

		assert(legalCases.size() == bufferStates.size());
		assert(legalCases.size() == results.size());
		for(size_t i = 0; i < legalCases.size(); i++) {
			buffer = buildCharVec(legalCases[i]);
			setBuffer(buffer);
			try {
				if(getCRLFLine() != results[i]) {
					Log::testFail(TAG, Log::msg("case <" + legalCases[i] + ">, result ", i));
					failFlag = true;
				}
				if(buildStrFromCharVec(getBuffer()) != bufferStates[i]) {
					Log::testFail(TAG, Log::msg("case <" + legalCases[i] + ">, buffer state ", i));
					failFlag = true;	
				}
			} 
			catch(HTTPBadMessageException e) {
				Log::testFail(TAG, Log::msg("case <" + legalCases[i] + ">, exception ", e.what(), ", ", i));
				failFlag = true;
			}
			catch(HTTPParserException e) {
				Log::testFail(TAG, Log::msg("case <" + legalCases[i] + ">, exception ", e.what(), ", ", i));
				failFlag = true;
			}
		}		


		if(!failFlag) { Log::testSuccess(TAG); }
	}

	void testParseHeaderFields() {
		static const string TAG = "parseHeaderFields";
		bool failFlag = false;
		vector<char> buffer;

		const vector<string> illegalCases = {
			"", "1",
			" Cache-Control : cache-request-directive|cache-response-directive\r\n\r\n",
			"Cache-Control : cache-request-directive|cache-response-directive\r\n\r\n",
			"Cache-Control cache-request-directive|cache-response-directive\r\n\r\n",
			"Cache-Control: cache-request-directive|cache-response-directive\r\n"
		};

		for(size_t i = 0; i < illegalCases.size(); i++) {
			buffer = buildCharVec(illegalCases[i]);
			setBuffer(buffer);
			try {
				parseHeaderFields();
				failFlag = true;
				Log::testFail(TAG, Log::msg("ILLEGAL CASE <", illegalCases[i], ">"));
			} 
			catch(HTTPParserException e) {}
			catch(HTTPBadMessageException e) {}
		}


		const vector<string> legalCases = {
			"Cache-Control: \tcache-request-directive cache-response-directive \t\r\n\r\n",
			"Cache-Control:\r\n\r\n",
			"Cache-Control: \r\n\r\n",
			"Cache-Control: cache-request-directive|cache-response-directive\r\nCache-Control: \t2\r\n\r\n",
		};
		vector<set<pair<string, string>>> resultSets(legalCases.size());
		resultSets[0].insert(make_pair("Cache-Control", "cache-request-directive cache-response-directive"));
		resultSets[1].insert(make_pair("Cache-Control", ""));
		resultSets[2].insert(make_pair("Cache-Control", ""));
		resultSets[3].insert(make_pair("Cache-Control", "cache-request-directive|cache-response-directive"));
		resultSets[3].insert(make_pair("Cache-Control", "2"));
		
		for(size_t i = 0; i < legalCases.size(); i++) {
			buffer = buildCharVec(legalCases[i]);
			setBuffer(buffer);
			try {
				parseHeaderFields();
				if(headerFields != resultSets[i]) {
					failFlag = true;
					Log::testFail(TAG, Log::msg("CASE <", legalCases[i], ">, got:"));
					for(auto const& e : headerFields) {
						Log::testFail(TAG, Log::msg("\t<", e.first, "> <", e.second, ">"));
					}
				}
			} 
			catch(HTTPParserException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
			catch(HTTPBadMessageException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
		}

		if(!failFlag) { Log::testSuccess(TAG); }
	}

	void doTest() {
		testGetCRLFLine();
		testParseHeaderFields();
	}
};





class HTTPRequestParserTest : public HTTPRequestParser {
public:
	void testParseRequestLine() {
		static const string TAG = "testParseRequestLine";
		bool failFlag = false;
		vector<char> buffer;

		const vector<string> illegalCases = {
			"", "1", "GEX http://www.example.org/pub/WWW/TheProject.html HTTP/1.1\r\n",
			"GET http://www.example.org/pub/WWW/TheProject.html HTTP/121\r\n",
			"GET http://www.example.org/pub/WWW/TheProject.html 	HTTP/1.1\r\n",
			"GET 	http://www.example.org/pub/WWW/TheProject.html HTTP/1.1\r\n",
			" CONNECT www.example.com:80 HTTP/1.1\r\n",
			"CONNECT www.example.com:80 HTTP/1.1 \r\n"
		};

		for(size_t i = 0; i < illegalCases.size(); i++) {
			buffer = buildCharVec(illegalCases[i]);
			setBuffer(buffer);
			try {
				parseRequestLine();
				failFlag = true;
				Log::testFail(TAG, Log::msg("<", illegalCases[i], ">"));
			}
			catch(HTTPParserException e) {}
			catch(HTTPBadMessageException e) {}
		}


		const vector<string> legalCases = {
			"GET http://www.example.org/pub/WWW/TheProject.html HTTP/1.1\r\n",
			"CONNECT www.example.com:80 HTTP/1.1\r\n"
		};
		const vector<string> methods = {
			"GET",
			"CONNECT"
		};
		const vector<string> targets = {
			"http://www.example.org/pub/WWW/TheProject.html",
			"www.example.com:80"
		};
		const vector<string> versions = {
			"HTTP/1.1",
			"HTTP/1.1"
		};

		assert(legalCases.size() == methods.size());
		assert(legalCases.size() == targets.size());
		assert(legalCases.size() == versions.size());
		for(size_t i = 0; i < legalCases.size(); i++) {
			buffer = buildCharVec(legalCases[i]);
			setBuffer(buffer);
			try {
				parseRequestLine();
				if(requestLine.method != methods[i]) {
					failFlag = true;
					Log::testFail(TAG, "method");
				}
				if(requestLine.requestTarget != targets[i]) {
					failFlag = true;
					Log::testFail(TAG, "target");
				}
				if(requestLine.httpVersion != versions[i]) {
					failFlag = true;
					Log::testFail(TAG, "version");
				}
			} 
			catch(HTTPParserException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
			catch(HTTPBadMessageException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
		}

		if(!failFlag) { Log::testSuccess(TAG); }
	}

	void testValidCases() {
		for(int i = 0; i < 100; i++) {
			stringstream ss;
			ss << "./httpparser/testCases/validRequest" << i << ".txt";
			const string filename = ss.str();

			ifstream ifs(filename);
			if(!ifs) { continue; }
			const string ifsStr = string(
				istreambuf_iterator<char>(ifs), 
				istreambuf_iterator<char>()
			);
			ifs.close();

			Log::verbose(Log::msg(
				"file <", filename, "> ifs string is:\n", ifsStr
			));

			auto buffer = buildCharVec(ifsStr);
			setBuffer(buffer);
			HTTPRequest hr = build();

			Log::verbose(Log::msg(
				"parse result:\n", hr.toStr()
			));

		}
	}

	void doTest() {
		testParseRequestLine();
		Log::setVerbose(false);
		testValidCases();
		Log::setVerbose(true);
	}
};





class HTTPStatusParserTest : public HTTPStatusParser {
public:
	void testParseStatusLine() {
		static const string TAG = "testParseStatusLine";
		bool failFlag = false;
		vector<char> buffer;

		const vector<string> illegalCases = {
			"",
			"HTTP/1.1  200 OK\r\n",
			"HTTP/1.1 200  OK\r\n",
			" HTTP/1.1 200 OK\r\n",
			"HTTP /1.1 200 OK\r\n",
			"HTTP/1.1 2010 OK\r\n"
		};

		for(size_t i = 0; i < illegalCases.size(); i++) {
			buffer = buildCharVec(illegalCases[i]);
			setBuffer(buffer);
			try {
				parseStatusLine();
				failFlag = true;
				Log::testFail(TAG, Log::msg("<", illegalCases[i], ">"));
			}
			catch(HTTPParserException e) {}
			catch(HTTPBadMessageException e) {}
		}


		const vector<string> legalCases = {
			"HTTP/1.1 200 OK\r\n",
			"HTTP/1.1 404 Not Found\r\n"
		};
		const vector<string> versions = {
			"HTTP/1.1",
			"HTTP/1.1"
		};
		const vector<string> codes = {
			"200",
			"404"
		};
		const vector<string> reasons = {
			"OK",
			"Not Found"
		};

		assert(legalCases.size() == versions.size());
		assert(legalCases.size() == codes.size());
		assert(legalCases.size() == reasons.size());
		for(size_t i = 0; i < legalCases.size(); i++) {
			buffer = buildCharVec(legalCases[i]);
			setBuffer(buffer);
			try {
				parseStatusLine();
				if(statusLine.httpVersion != versions[i]) {
					failFlag = true;
					Log::testFail(TAG, "version");
				}
				if(statusLine.statusCode != codes[i]) {
					failFlag = true;
					Log::testFail(TAG, Log::msg("code case<", legalCases[i], ">"));
				}
				if(statusLine.reasonPhrase != reasons[i]) {
					failFlag = true;
					Log::testFail(TAG, "reason");
				}
			} 
			catch(HTTPParserException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
			catch(HTTPBadMessageException e) {
				failFlag = true;
				Log::testFail(TAG, Log::msg("exception: ", e.what()));
			}
		}

		if(!failFlag) { Log::testSuccess(TAG); }
	}

	void testValidCases() {
		for(int i = 0; i < 100; i++) {
			stringstream ss;
			ss << "./httpparser/testCases/validStatus" << i << ".txt";
			const string filename = ss.str();

			ifstream ifs(filename);
			if(!ifs) { continue; }
			const string ifsStr = string(
				istreambuf_iterator<char>(ifs), 
				istreambuf_iterator<char>()
			);
			ifs.close();

			Log::verbose(Log::msg(
				"file <", filename, "> ifs string is:\n", ifsStr
			));

			auto buffer = buildCharVec(ifsStr);
			setBuffer(buffer);
			HTTPStatus st = build();

			Log::verbose(Log::msg(
				"parse result:\n", st.toStr()
			));

		}
	}

	void doTest() {
		testParseStatusLine();
		testValidCases();
	}
};

int main() {
	testNonNegHexStrToInt();
	HTTPParserTest().doTest();
	HTTPRequestParserTest().doTest();
	HTTPStatusParserTest().doTest();
}