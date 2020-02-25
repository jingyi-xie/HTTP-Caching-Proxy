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
	 * C++ did really stupid in time convert
	 * it uses different clocks for different time
	 * which makes it hard to convert
	 * this is a helper function to convert a
	 * time_point to a time_t
	*/
	template <typename TimePoint>
	time_t toTimeT(const TimePoint& tp) {
		auto temp = chrono::time_point_cast<chrono::system_clock::duration>(
				tp - TimePoint::clock::now()
				+ chrono::system_clock::now()
			);
		return chrono::system_clock::to_time_t(temp);
	}

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
		 * if request already exists, then update
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
			 * 2. cache "hit", but need re-validation, send <validtionReq> to server
			 * 
			*/
			int action;
			HTTPStatus resp;
			HTTPRequest validationReq;
		};
		ConsRespResult constructResponse(const HTTPRequest& req);

	public: // TODO: for testing

		HTTPProxyCache(const fs::path& p);
		/*
		 * marks if HTTPProxyCache is useable
		*/
		static bool initd;

		/*
		 * filename format: <PREFIX DELIM ID>
		*/
		const string DELIM = "_";
		const string REQ_ID_PREFIX = "request";
		const string STA_ID_PREFIX = "response";
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
		 *
		 * only when if id != noid, the result is valid! because of the exception thing
		*/
		struct GetStaResult {
			string id;
			HTTPStatus s;
			time_t respTime;
		};
		GetStaResult getStaByReq(const HTTPRequest::RequestLine& requestLine) const;

		HTTPRequest buildValidationRequest(const HTTPRequest& req, const HTTPStatus& sta) const;

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
		tm timeStru = { 0 };
		istringstream ss(s);
		const time_t rawTime = time(0);
		const int offset = timegm(localtime(&rawTime)) - rawTime;
		// Date: Tue, 25 Feb 2020 18:46:47 GMT
		if(!(ss >> get_time(&timeStru, "%a, %d %b %Y %H:%M:%S %Z"))) {
			return -1;
		}

		/*
		if(strptime(
			s.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &timeStru
		) == nullptr) {	return -1; }
		Log::debug(Log::msg(
			"sec:", timeStru.tm_sec,
			", min:", timeStru.tm_min,
			", hour:", timeStru.tm_hour,
			", mday:", timeStru.tm_mday,
			", mon:", timeStru.tm_mon,
			", year:", timeStru.tm_year,
			", wday:", timeStru.tm_wday,
			", yday:", timeStru.tm_yday,
			", isdst:", timeStru.tm_isdst
		));
		*/
		return (int)mktime(&timeStru) + offset ;
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
		// rule 4: heuristic freshness
		static const int heuristicFreshness = 3600 * 24; // one day
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
		Log::debug(Log::msg(
			"in isRespFresh(): freshness <", lifetime, ">, ",
			"age <", age, ">, isFresh <", 
			(lifetime > 0 && age > 0 && lifetime > age), ">"
		));
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
		
		auto result = getStaByReq(req.requestLine);
		// if already exists, update
		const string id = (result.id == noid ? *(idPool.begin()) : result.id);
		
		Cache::save(getReqName(id), req.toStr());
		Cache::save(getStaName(id), sta.toStr());
		Log::debug("Yeah! New pair in cache!");

		// erase after successfully save
		if(id != noid) { idPool.erase(idPool.begin()); }
		if(idPool.size() == 0) {
			updateIdPool();
		}
		return id;
	}

	HTTPProxyCache::ConsRespResult HTTPProxyCache::constructResponse(const HTTPRequest& req) {
		ConsRespResult result;
		const GetStaResult r = getStaByReq(req.requestLine);
		const HTTPStatus& resp = r.s;
		const time_t respTime = r.respTime;

		// rule 1: URI
		if(resp == HTTPStatus()) { // cache miss
			Log::debug("Cache miss");
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
				Log::debug("in constructResponses: request has 'no-cache'");
				result.action = 2;
				result.resp = resp;
				result.validationReq = buildValidationRequest(req, resp);
				return result;
			}
		}

		// rule 5
		for(auto const& e : resp.headerFields) {
			if(e.first == "Cache-Control" && e.second == "no-cache") {
				Log::debug("in constructResponses: response has 'no-cache'");
				result.action = 2;
				result.validationReq = buildValidationRequest(req, resp);
				return result;
			}
		}

		// rule 6.1
		if(HTTPSemantics::isRespFresh(resp, respTime)) {
			Log::debug("in constructResponses: response is fresh");
			result.action = 0;
			result.resp = resp;
			return result;
		} else if(false) { // rule 6.2
			// [BROKEN]: NEVER allowed to be served stale
		} else {
			Log::debug("in constructResponses: rule 6.2 go re-validation");
			result.action = 2;
			result.validationReq = buildValidationRequest(req, resp);
			return result;
		}
		
		Log::debug("in constructResponses: no rule matched, no response constructed");
		result.action = 1;
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

	HTTPProxyCache::GetStaResult HTTPProxyCache::getStaByReq(const HTTPRequest::RequestLine& requestLine) const {
		GetStaResult result;
		result.id = noid;
		result.s = HTTPStatus();
		result.respTime = 0;

		if(!is_directory(wdir)) { return result; }
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
			if(str[str.length() - 1] == '\r') {
				str = str.substr(0, str.length() - 1); // get rid of '\r'
			}
			if(str == requestLineStr) {
				const string id = getIdByFilename(file.path().filename());
				if(id == noid) { return result; }
				//fs::file_time_type t;
				try {
					const auto ftime = fs::last_write_time(file.path());
					result.respTime = toTimeT(ftime);
					result.s = buildStatusFromStr(getMsgById(getStaName(id)));
					result.id = id;
					return result;
				} catch(const fs::filesystem_error& e) {
					Log::warning(Log::msg("While fetching resp in cache: ", e.what()));
					return result;
				}
				
			}
		}
		return result;
	}

	HTTPRequest HTTPProxyCache::buildValidationRequest(const HTTPRequest& req, const HTTPStatus& sta) const {
		HTTPRequest result(req);
		auto const headerFields = result.headerFields;
		for(auto const& e : headerFields) {
			if(e.first == "ETag") {
				result.headerFields.insert(make_pair(
					"If-None-Match", e.second
				));
			}
			if(e.first == "Last-Modified") {
				result.headerFields.insert(make_pair(
					"If-Modified-Since", e.second
				));
			}
		}
		return result;
	}

}
	
	using zq29Inner::HTTPProxyCache;
	using zq29Inner::HTTPSemantics;
}

#endif