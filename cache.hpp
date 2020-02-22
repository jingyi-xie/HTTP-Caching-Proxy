#ifndef ZQ29_CACHE
#define ZQ29_CACHE

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
	 * a NON-thread-safe "cache" that implemented with std::filesystem
	 * 
	 * one cache object "manages" one directory, it write to/read from that dir
	*/
	const string CACHE_DIR_NAME = "/__cache__";
	class Cache {
	protected:
		/*
		 * working directory, every operation happens within it
		 * which means you should only pass relative filepath to every operations
		 * otherwise, you MAY see exceptions
		*/
		const fs::path wdir;
		void newWdirIfNone() const;

	public:
		static const string noid;

		string getWdir();

		class CacheException : public exception {
		private:
			const string msg;
		public:
			CacheException(const string& ms);
			const char* what() const throw() override;
		};
		/*
		 * by default, wdir is current_path()
		*/
		Cache(const fs::path& p = "");

		/*
		 * add to the cache, id cannot be "" (noid, empty string)
		 * if id already exists, override
		 * you should make id an valid filename, 
		 * or you will get exceptions, or worse, undefined behaviors
		*/
		void save(const string& id, const string& msg);

		/*
		 * return the 1st found id with the same content as msg
		 * if msg not existing in the cache, return noid
		 * this is an expensive call, but we do not care much about performance here
		 * optimization suggestion: keep a map in memory
		*/
		string getIdByMsg(const string& msg);

		/*
		 * remove cache content by id
		 * if id does not exist, do nothing
		*/
		void remove(const string& id);

		/*
		 * remove all the regular files within wdir
		 * you need to THINK TWICE before calling this
		*/
		void removeAll();

	};
	const string Cache::noid = "";


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
		 *
		*/

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
	//////////////////////////// Cache Implementation ///////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	void Cache::newWdirIfNone() const {
		if(!fs::is_directory(wdir.parent_path())) {
			throw CacheException("parent directory of wdir not exists");
		}
		if(!fs::is_directory(wdir)) {
			try {
				if(!fs::create_directory(wdir)) {
					throw CacheException("failed to create wdir");
				}
			} catch(const fs::filesystem_error& e) {
				throw CacheException(e.what());
			}
		}
	}

	string Cache::getWdir() { return wdir; }

	Cache::CacheException::CacheException(const string& m) : msg(m) {}
	const char* Cache::CacheException::what() const throw() { return msg.c_str(); }

	Cache::Cache(const fs::path& p) : 
		wdir(string(p == "" ? fs::current_path() : p) + CACHE_DIR_NAME)
	{
		if(!fs::is_directory(wdir.parent_path())) {
			throw CacheException(Log::msg(
				"failed to init Cache object, <", wdir.parent_path(),
				"> does not exist or is not a directory"
			));
		}
		newWdirIfNone();
	}

	void Cache::save(const string& id, const string& msg) {
		newWdirIfNone();
		ofstream ofs;
		fs::path newEntry(wdir);
		newEntry += "/" + id;
		ofs.open(newEntry);
		if(!ofs) {
			throw CacheException(Log::msg("on save, failed to open file <", id, ">"));
		}
		ofs << msg;
		ofs.close();
	}

	string Cache::getIdByMsg(const string& msg) {
		if(!is_directory(wdir)) { return noid; }
		for(auto const& file : fs::directory_iterator(wdir)) {
			if(!is_regular_file(file.path())) { continue; }
			ifstream ifs(file.path());
			const string str = string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
			if(str == msg) {
				return file.path().filename();
			}
		}
		return noid;
	}

	void Cache::remove(const string& id) {
		if(!is_directory(wdir)) { return; }
		for(auto const& file : fs::directory_iterator(wdir)) {
			if(!is_regular_file(file.path())) { continue; }
			if(id == file.path().filename()) {
				try {
					fs::remove(file.path());
				} catch(const fs::filesystem_error& e) {
					throw CacheException(e.what());
				}
				break;
			}
		}
	}

	void Cache::removeAll() {
		if(!is_directory(wdir)) { return; }
		for(auto const& file : fs::directory_iterator(wdir)) {
			if(!is_regular_file(file.path())) { continue; }
			try {
				fs::remove(file.path());
			} catch(const fs::filesystem_error& e) {
				throw CacheException(e.what());
			}
		}
	}


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
	
	using zq29Inner::Cache;
	using zq29Inner::HTTPProxyCache;
}

#endif