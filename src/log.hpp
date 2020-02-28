/*
 * This file:
 * 1. helps me better print things while developing
 * 2. print what's required
*/
#ifndef ZQ29_LOG
#define ZQ29_LOG

#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <fstream>

namespace zq29 {
namespace zq29Inner {
	using namespace std;

	class Log {
	private:
		static bool __verbose;
		static bool __debug;
		static bool __warning;
		static bool __error;
		static bool __ifWriteToFile;
		static mutex printLock;

		static string logFilePath;

		static void _msg(stringstream& is) {}
		template<typename T, typename... Rest>
		static void _msg(stringstream& is, const T& arg, const Rest&... rest) {
			try {
				is << arg;
			} catch(...) { warning("in function msg: operator<< undefined, ignore!"); }
			_msg(is, rest...);
		}

		static void __printRedNoLock(const string& msg) {
			cout << "\033[0;31m" << msg << "\033[0m" << endl;
		}

		static void __printGreenNoLock(const string& msg) {
			cout << "\033[0;32m" << msg << "\033[0m" << endl;
		}

		static void __printYellowNoLock(const string& msg) {
			cout << "\033[1;33m" << msg << "\033[0m" << endl;
		}

		static void __printCyanNoLock(const string& msg) {
			cout << "\033[0;36m" << msg << "\033[0m" << endl;
		}

		static void __writeToFileIfEnabledNoLock(const string& msg) {
			if(__ifWriteToFile) {
				ofstream ofs(logFilePath, ofstream::app);
				if(ofs) {
					ofs << msg << endl;
				} else {
					__printYellowNoLock("Failed to write to log file " + logFilePath);
				}
				ofs.close();
			}
		}

	public:
		template<typename... T>
		static string msg(T... args) {
			stringstream ss;
			_msg(ss, args...);
			return ss.str();
		}

		static void testSuccess(const string& testName) {
			lock_guard<mutex> lck(printLock);
			__printGreenNoLock(Log::msg("Test <", testName, "> passed!"));
		}

		static void testFail(const string& testName, const string& where) {
			lock_guard<mutex> lck(printLock);
			__printRedNoLock(Log::msg("Test <", testName, "> failed at ", where));
		}

		static void setVerbose(bool b) { __verbose = b; }
		static void setDebug(bool b) { __debug = b; }
		static void setWarning(bool b) { __warning = b; }
		static void setError(bool b) { __error = b; }
		static void startWriteToFile() { __ifWriteToFile = true; }
		static void endWriteToFile() { __ifWriteToFile = false; }

		static string asctimeNow() {
			time_t rawtime;
			tm* timeinfo;
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			string result = string(asctime(timeinfo));

			// asctime is a stupid function
			// why should it return with a '\n' at last !??
			if(result[result.length() - 1] == '\n') { 
				result = result.substr(0, result.length() - 1);
			}
			return result;
		}

		static string asctimeFromTimeT(const time_t t) {
			time_t rawtime = t;
			tm* timeinfo;
			timeinfo = localtime(&rawtime);
			string result = string(asctime(timeinfo));

			// asctime is a stupid function
			// why should it return with a '\n' at last !??
			if(result[result.length() - 1] == '\n') { 
				result = result.substr(0, result.length() - 1);
			}
			return result;
		}

		static void proxy(const string& msg) {
			lock_guard<mutex> lck(printLock);
			__printYellowNoLock(msg);
			__writeToFileIfEnabledNoLock(msg);
		}

		static void verbose(const string& msg) {
			if(__verbose) {
				lock_guard<mutex> lck(printLock);
				cout << "***VERBOSE***: " << msg << endl;
			}
		}

		static void debug(const string& msg) {
			if(__debug) {
				lock_guard<mutex> lck(printLock);
				__printCyanNoLock(Log::msg("***DEBUG***: ", msg));
			}
		}

		static void warning(const string& msg) {
			if(__warning) {
				lock_guard<mutex> lck(printLock);
				__printYellowNoLock(Log::msg("(no-id): WARNING ", msg));
				__writeToFileIfEnabledNoLock(Log::msg("(no-id): WARNING ", msg));
			}
		}

		static void error(const string& msg) {
			if(__error) {
				lock_guard<mutex> lck(printLock);
				__printRedNoLock(Log::msg("(no-id): ERROR ", msg));
				__writeToFileIfEnabledNoLock(Log::msg("(no-id): ERROR ", msg));
			}
		}

		template<typename ExceptionType>
		static void errorThenThrow(const string& msg) {
			error(msg);
			throw ExceptionType(msg.c_str());
		}

	};

	string Log::logFilePath = "/var/log/erss/proxy.log";
	bool Log::__verbose = true;
	bool Log::__debug = true;
	bool Log::__warning = true;
	bool Log::__error = true;
	/* 
	 * do NOT change the line below
	 * call Log::startWriteToFile instead, which is
	 * sadly, not thread-safe
	*/
	bool Log::__ifWriteToFile = false;
	mutex Log::printLock;
}
	using zq29Inner::Log;
}

#endif