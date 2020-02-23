#include <iostream>
#include "cache.hpp"
#include "httpproxycache.hpp"
using namespace zq29;
using namespace std;

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

int main() {
	testCacheBasic();
	//testCacheRemoveAll();
	testHTTPProxyCacheBasic();
	testTime();
}