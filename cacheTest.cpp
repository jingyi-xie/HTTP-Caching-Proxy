#include "cache.hpp"
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

int main() {
	testCacheBasic();
}