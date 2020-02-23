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
#include <ctime>
#include <algorithm>

#include "../log.hpp"
#include "cache.hpp"
#include "../httpparser/httpparser.hpp"

namespace zq29 {
namespace zq29Inner {

	using namespace std;
	namespace fs = std::filesystem;

	/*
	 * a class with several helpers functions
	 * which help us understand the semantics of an HTTP message
	*/
	class HTTPSemantics {
	public:
		/*
		 * check if the req & resp pair is cacheable according to
		 * https://tools.ietf.org/html/rfc7234#section-3
		 *
		 * exception: no throw
		*/
		static bool isStrictlyCacheable(const HTTPRequest& req, const HTTPStatus& sta);
		
		/*
		 * check if they're cacheable according to our project requirements
		*/
		static bool isCacheable(const HTTPRequest& req, const HTTPStatus& sta);
		
		/*
		 * conver a HTTP date string to seconds
		 * returns -1 on failure
		*/
		static int dateStrToSeconds(const string& s);

		/*
		 * calculating freshness lifetime according to
		 * https://tools.ietf.org/html/rfc7234#section-4.2.1
		 * 
		 * return a negative value on any error, no throw
		*/
		static int getFreshnessLifetime(const HTTPStatus& sta);

		/*
		 * calculating age according to 
		 * https://tools.ietf.org/html/rfc7234#section-4.2.3
		 *
		 * return a negative value on any error, no throw
		*/
		static int getAge(const HTTPStatus& sta, const time_t respTime);

		/*
		 * check if a response is fresh according to
		 * https://tools.ietf.org/html/rfc7234#section-4.2
		 *
		 * on any error, return false, no throw
		*/
		static bool isRespFresh(const HTTPStatus& sta, const time_t respTime);

	};

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
		string save(const HTTPRequest& req, const HTTPStatus& sta);

		/*
		 * implemented according to https://tools.ietf.org/html/rfc7234#section-4
		 * 
		 * given a request, determine if
		 * 1. we have a useable response in cache, action = 0, resp = the response
		 * 2. no same URI found, we do NOT hold a cache for it, action = 1
		 * 3. request method does not supported, action = 1
		 * 4. header fields has "Cache-Control: no-cache", action = 2
		 * 
		*/
		struct ConsRespResult {
			/*
			 * <action value>: things you supposed to do
			 * 0: good, reply to client with <resp>
			 * 1: cache miss, go talk to the server
			 * 2. cache "hit", but need re-validation
			 * 
			*/
			int action;
			HTTPStatus resp;
		};
		ConsRespResult constructResponses(const HTTPRequest& req);

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
		string getIdByFilename(const string& filename) const;
		string getReqName(const string& id) const;
		string getStaName(const string& id) const;

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

		/*
		 * get HTTP status string by HTTP request's first line
		 * return HTTPStatus() when no match or faild to parse match
		 * which is rare but possiily because of modification of files
		 *
		 * exception: no throw (but may throw when system lib fails which is rare)
		*/
		HTTPStatus getStaStrByReq(const HTTPRequest::RequestLine& requestLine) const;

	};
	bool HTTPProxyCache::initd = false;





	
	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPSemantics Implementation ///////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	bool HTTPSemantics::isStrictlyCacheable(const HTTPRequest& req, const HTTPStatus& sta) {
		// if req & sta objects are valid, then rule 1 & 2 are already satisfied
		if(req == HTTPRequest() || sta == HTTPStatus()) { return false; }

		// rule 3 & 4 & 5
		for(auto const& e : req.headerFields) {
			if(e.first == "Authorization" || 
				(e.first == "Cache-Control" && e.second == "no-store")
			) { 
				return false; 
			}
		}
		for(auto const& e : sta.headerFields) {
			if(e.first == "Cache-Control" && 
				(e.second == "no-store" || e.second == "private")
			) { 
				return false; 
			}
		}

		// rule 6 
		for(auto const& e : sta.headerFields) {
			// rule 6.1
			if(e.first == "Expires") { return true; }
			
			// rule 6.2
			else if(e.first == "Cache-Control" && 
				e.second.length() > 8 && e.second.substr(0, 7) == "max-age"
			) { return true; }
			
			// rule 6.3
			else if(e.first == "Cache-Control" && 
				e.second.length() > 9 && e.second.substr(0, 8) == "s-maxage"
			) { return true; }
			
			// rule 6.4 - 6.6
			// [BROKEN] not implemented, treated as un-cacheable
		}

		return false;
	}	

	bool HTTPSemantics::isCacheable(const HTTPRequest& req, const HTTPStatus& sta) {
		if(req.requestLine.method != "GET" || 
			sta.statusLine.statusCode != "200") {
			return false;
		}
		return isStrictlyCacheable(req, sta);
	}

	int HTTPSemantics::dateStrToSeconds(const string& s) {
		tm timeStru;
		if(strptime(
			s.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &timeStru
		) == nullptr) {	return -1; }
		return (int)mktime(&timeStru);
	}

	int HTTPSemantics::getFreshnessLifetime(const HTTPStatus& sta) {
		for(auto const& e : sta.headerFields) {
			// rule 1: its guaranteed to be shared before caching
			if(e.first == "Cache-Control" && 
				e.second.length() > 9 && 
				e.second.substr(0, 8) == "s-maxage"
			) { 
				stringstream ss;
				ss << e.second.substr(9, string::npos);
				int t = -1;
				ss >> t;
				return t;
			}

			// rule 2
			if(e.first == "Cache-Control" && 
				e.second.length() > 8 && 
				e.second.substr(0, 7) == "max-age"
			) { 
				stringstream ss;
				ss << e.second.substr(8, string::npos);
				int t = -1;
				ss >> t;
				return t;
			}

			// rule 3
			if(e.first == "Expires") {
				int expireTime = dateStrToSeconds(e.second);
				auto it = find_if(
					sta.headerFields.begin(), 
					sta.headerFields.end(), 
					[](const pair<string, string>& p) { 
						return p.first == "Date"; 
					});
				if(it == sta.headerFields.end()) { return -1; }
				int dateTime = dateStrToSeconds((*it).second);
				if(dateTime < 0 || expireTime < 0 || expireTime < dateTime) { return -1; }
				return expireTime - dateTime;
			}
		}
		// rule 4: heuristic freshness = 100s in this case
		static const int heuristicFreshness = 100;
		return heuristicFreshness;
	}

	int HTTPSemantics::getAge(const HTTPStatus& sta, const time_t respTime) {
		// [BROKEN] we only implement an estimate time, for better encapsulation
		const time_t now = time(0);

		auto it = find_if(
			sta.headerFields.begin(), 
			sta.headerFields.end(), 
			[](const pair<string, string>& p) { 
				return p.first == "Date"; 
			});
		if(it == sta.headerFields.end()) { // use respTime instead
			if(now < respTime) { assert(false); }
			return (int)(now - respTime);
		} else {
			const int intNow = (int)now;
			const int dateValue = dateStrToSeconds((*it).second);
			if(dateValue < 0 || intNow < dateValue) { return -1; }
			return intNow - dateValue;
		}

		return -1;
	}

	bool HTTPSemantics::isRespFresh(const HTTPStatus& sta, const time_t respTime) {
		const int lifetime = getFreshnessLifetime(sta);
		const int age = getAge(sta, respTime);
		if(lifetime < 0 || age < 0) { return false; }
		return (lifetime > age);
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

	string HTTPProxyCache::save(const HTTPRequest& req, const HTTPStatus& sta) {
		assert(idPool.size() > 0);
		const string id = *(idPool.begin());
		
		Cache::save(getReqName(id), req.toStr());
		Cache::save(getStaName(id), sta.toStr());

		// erase after successfully save
		idPool.erase(idPool.begin());
		if(idPool.size() == 0) {
			updateIdPool();
		}
		return id;
	}

	HTTPProxyCache::ConsRespResult HTTPProxyCache::constructResponses(const HTTPRequest& req) {
		ConsRespResult result;
		// rule 1: URI
		HTTPStatus resp = getStaStrByReq(req.requestLine);
		if(resp == HTTPStatus()) { // cache miss
			result.action = 1;
			return result;
		}

		// now cache "hit"

		// rule 2: method, in our case, "GET"
		if(req.requestLine.method != "GET") {
			Log::debug("you may want to check HTTPProxyCache::constructResponses rule 2 code");
			result.action = 1;
			return result;
		}

		// rule 3: [BROKEN] not supported

		// rule 4: [PARTIALLY BROKEN], no support for pragma since it is HTTP 1.0
		for(auto const& e : req.headerFields) {
			if(e.first == "Cache-Control" && e.second == "no-cache") {
				result.action = 2;
			}
		}

		// rule 5
		for(auto const& e : resp.headerFields) {
			if(e.first == "Cache-Control" && e.second == "no-cache") {
				result.action = 2;
			}
		}

		// TODO:
		return result;
	}

	HTTPProxyCache::HTTPProxyCache(const fs::path& p) :
		Cache(p)
	{
		restore();
		initd = true;
	}

	string HTTPProxyCache::getIdByFilename(const string& filename) const {
		const size_t sp = filename.find(DELIM);
		if(sp == string::npos) { return noid; }
		return filename.substr(sp + 1, string::npos);
	}

	string HTTPProxyCache::getReqName(const string& id) const {
		return REQ_ID_PREFIX + DELIM + id;
	}
	string HTTPProxyCache::getStaName(const string& id) const {
		return STA_ID_PREFIX + DELIM + id;
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

	HTTPStatus HTTPProxyCache::getStaStrByReq(const HTTPRequest::RequestLine& requestLine) const {
		if(!is_directory(wdir)) { return HTTPStatus(); }
		stringstream ss;
		ss << requestLine.method << " " 
			<< requestLine.requestTarget << " "
			<< requestLine.httpVersion;
		const string requestLineStr = ss.str();
		for(auto const& file : fs::directory_iterator(wdir)) {
			if(!is_regular_file(file.path())) { continue; }
			ifstream ifs(file.path());
			string str;
			getline(ifs, str);
			ifs.close();
			if(str == requestLineStr) {
				const string id = getIdByFilename(file.path().filename());
				if(id == noid) { return HTTPStatus(); }
				return buildStatusFromStr(getMsgById(getStaName(id)));
			}
		}
		return HTTPStatus();
	}


}
	
	using zq29Inner::HTTPProxyCache;
	using zq29Inner::HTTPSemantics;
}

#endif