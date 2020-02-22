#ifndef ZQ29_HTTPPROXYCACHE
#define ZQ29_HTTPPROXYCACHE

#include <filesystem> // C++ 17 required
#include <thread>
#include <mutex>
#include <stdexcept>
#include <fstream>
#include <streambuf>
#include <set>
#include <assert.h>

#include "log.hpp"

namespace zq29 {
namespace zq29Inner {

	using namespace std;
	namespace fs = std::filesystem;

	/*
	 * a thread-safe cache that manages HTTP messages
	 *
	 * this class follows the Singleton design pattern 
	 * ref: https://stackoverflow.com/questions/1008019/c-singleton-design-pattern
	 *
	 *
	*/
	class HTTPProxyCache : public Cache {
	public:
		static HTTPProxyCache& createInstance(const fs::path& p = "", bool __skipErrorCheck = false);
		static HTTPProxyCache& getInstance();
		HTTPProxyCache(const HTTPProxyCache& rhs) = delete;
		HTTPProxyCache& operator=(const HTTPProxyCache& rhs) = delete;

		/*
		 * save request and response
		 * return the id assigned to them
		*/
		string save()

	private:

		HTTPProxyCache(const fs::path& p);
		/*
		 * marks if HTTPProxyCache is useable
		*/
		static bool initd;

		/*
		 * filename format: <PREFIX DELIM ID>
		*/
		const string DELIM = "_";
		const string REQ_ID_PREFIX = "request" + DELIM;
		const string STA_ID_PREFIX = "response" + DELIM;
		string getIdByFilename(const string& filename);

		/*
		 * maintains a pool of available ids
		*/
		set<string> idPool;

		/*
		 * find more available ids
		 * NOT thread-safe
		 * and we may get ids that are already in use,
		 * which can be due to attacking or running out
		 * of id (not very likely), in these cases,
		 * we clear all the cache and start from the beginning
		*/
		void updateIdPool(const size_t expectedCount = 100);

		/*
		 * if the wdir already exists, restore from it
		 * this can only be called by constructor and 
		 * thus thread-safe
		*/
		void restore();

	};
	bool HTTPProxyCache::initd = false;

	



	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPProxyCache Implementation //////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	HTTPProxyCache& HTTPProxyCache::createInstance(const fs::path& p, bool __skipErrorCheck) {
		if(initd && !__skipErrorCheck) {
			throw CacheException("in HTTPProxyCache, createInstance called mutli times");
		}
		static HTTPProxyCache instance(p);
		return instance;
	}
	HTTPProxyCache& HTTPProxyCache::getInstance() {
		if(!initd) {
			throw CacheException("in HTTPProxyCache, should call createInstance before getInstance");
		}
		return createInstance("whatever", true);
	}

	HTTPProxyCache::HTTPProxyCache(const fs::path& p) :
		Cache(p)
	{
		restore();
		initd = true;
	}

	string HTTPProxyCache::getIdByFilename(const string& filename) {
		const size_t sp = filename.find(DELIM);
		if(sp == string::npos) { return noid; }
		return filename.substr(sp + 1, string::npos);
	}

	void HTTPProxyCache::updateIdPool(const size_t expectedCount) {
		if(!is_directory(wdir)) {
			throw CacheException("wdir not available");
		}
		size_t maxId = 0;
		for(auto const& file : fs::directory_iterator(wdir)) {
			if(!is_regular_file(file.path())) { continue; }
			stringstream ss;
			ss << getIdByFilename(file.path().filename());
			size_t temp;
			ss >> temp;
			maxId = max(maxId, temp);
		}

		for(size_t i = 1; i <= expectedCount; i++) {
			// overflow! attack or runnnig out of id
			if(maxId + i < maxId) {
				removeAll();
				idPool.clear();
				for(size_t j = 0; j < expectedCount; j++) {
					stringstream ss;
					ss << j;
					idPool.insert(ss.str());
				}
				return;
			}

			stringstream ss;
			ss << maxId + i;
			idPool.insert(ss.str());
		}
	}

	void HTTPProxyCache::restore() {
		assert(!initd);
		updateIdPool();
	}




}
	
	using zq29Inner::HTTPProxyCache;
}

#endif