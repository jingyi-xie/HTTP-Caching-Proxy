#include <iostream>
#include <fstream>

#include "cache.hpp"
#include "httpproxycache.hpp"

using namespace zq29;
using namespace std;
namespace fs = std::filesystem;	

void testCacheBasic() {
	const string TAG = "testCacheBasic";
	bool failFlag = false;

	Cache cache1;

	try {
		Cache cache2("invalid path");
		failFlag = true;
		Log::testFail(TAG, "successfully init with arg 'invalid path'");
	} catch(const Cache::CacheException& e) {}

	try {
		cache1.save("1.txt", "123\n456\r\n789\n\n\n10");
	} catch(const Cache::CacheException& e) {
		failFlag = true;
		Log::testFail(TAG, e.what());
	}

	if(cache1.getIdByMsg("123\n456\r\n789\n\n\n10") != "1.txt") {
		failFlag = true;
		Log::testFail(TAG, "getIdByMsg, 1.txt");
	}
	if(cache1.getIdByMsg("123") != Cache::noid) {
		failFlag = true;
		Log::testFail(TAG, "getIdByMsg, noid");
	}

	cache1.remove("nothing should happen here");
	cache1.remove("1.txt");

	if(!failFlag) { Log::testSuccess(TAG); }
}

// this is dangerous
void testCacheRemoveAll() {
	const string TAG = "testCacheRemoveAll";
	bool failFlag = false;

	Cache c;
	c.save("1", "something");
	c.save("2", "ohhh");

	Log::warning(Log::msg(
		"You're about to delete all regular files in <",
		c.getWdir(), "> !"
	));
	cout << "input any char to start, or CTRL-C to stop:";
	char foo;
	cin >> foo;

	c.removeAll();

	cout << "check " << c.getWdir() << " to see if there are still files" << endl;

	if(!failFlag) { Log::testSuccess(TAG); }
}

void testHTTPProxyCacheBasic() {
	const string TAG = "testHTTPProxyCacheBasic";
	bool failFlag = false;

	// create and get
	try {
		HTTPProxyCache::getInstance();
		failFlag = true;
		Log::testFail(TAG, "unexpected success in get obj before create");
	} catch(const Cache::CacheException& e) {}

	HTTPProxyCache::createInstance();
	try {
		HTTPProxyCache::createInstance();
		failFlag = true;
		Log::testFail(TAG, "unexpected success in double create");
	} catch(const Cache::CacheException& e) {}

	HTTPProxyCache::getInstance();
	HTTPProxyCache::getInstance();



	if(!failFlag) { Log::testSuccess(TAG); }
}

void testTime() {
	const string TAG = "testTime";
	const string timeStr = "Sun, 23 Feb 2020 08:49:37 GMT";
	int time = HTTPSemantics::dateStrToSeconds(timeStr);
	if(time < 0) {
		Log::testFail(TAG, "HTTPSemantics::dateStrToSeconds failed");
	} else {
		//Log::verbose(Log::msg("HTTPSemantics::dateStrToSeconds got ", time));
		Log::testSuccess(TAG);
	}
}


void testGetStaByReq() {
	const string TAG = "testGetStaByReq";
	HTTPRequest::RequestLine line;
	line.method = "GET";
	line.requestTarget = "http://qianzuncheng.com/";
	line.httpVersion = "HTTP/1.1";
	auto res = HTTPProxyCache::getInstance().getStaByReq(line);
	//Log::verbose(Log::msg(TAG, ": <", res.id, ">"));
	ofstream ofs("test.txt");
	ofs << res.s.toStr();
	ofs.close();
}

void testFreshness() {
	ifstream ifs;
	ifs.open("__cache__/response_26");
	const string str1 = string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
	ifs.close();
	
	ifs.open("__cache__/request_26");
	const string str2 = string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
	ifs.close();

	HTTPStatusParser p;
	p.setBuffer(vector<char>(str1.begin(), str1.end()));
	auto resp = p.build();

	HTTPRequestParser p2;
	p2.setBuffer(vector<char>(str2.begin(), str2.end()));
	auto req = p2.build();

	for(auto const& e : resp.headerFields) {
		Log::verbose(Log::msg("<", e.first, ">, <", e.second, ">"));
	}

	HTTPSemantics::isCacheable(req, resp);
}

int main() {
	testCacheBasic();
	//testCacheRemoveAll();
	testHTTPProxyCacheBasic();
	testTime();
	testGetStaByReq();
	testFreshness();
}