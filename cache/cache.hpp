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

#include "../log.hpp"

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
		void save(const string& id, const string& msg) const;

		/*
		 * return the 1st found id with the same content as msg
		 * if msg not existing in the cache, return noid
		 * this is an expensive call, but we do not care much about performance here
		 * optimization suggestion: keep a map in memory
		*/
		string getIdByMsg(const string& msg) const;

		string getMsgById(const string& id) const;

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

	void Cache::save(const string& id, const string& msg) const {
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

	string Cache::getIdByMsg(const string& msg) const {
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

	string Cache::getMsgById(const string& id) const {
		ifstream ifs;
		fs::path filename(wdir);
		filename += "/" + id;
		ifs.open(filename);
		if(!ifs) {
			throw CacheException(Log::msg("no cache entry with id <", id, ">"));
		}
		const string str = string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
		ifs.close();
		return str;
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


	
}
	
	using zq29Inner::Cache;
}

#endif