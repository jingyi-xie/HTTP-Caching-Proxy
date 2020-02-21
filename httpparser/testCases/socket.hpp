#ifndef ZQ29_SOCKET
#define ZQ29_SOCKET

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>

#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <memory>

#include "log.hpp"
#include "potato.hpp"

#define MAX_DATA_SIZE 65536

namespace zq29 {
namespace zq29Inner {
	using namespace std;

	struct ConnectInfo {
		int socketFd;
		string peerIp;
		string peerPort;

		ConnectInfo() : socketFd(-1), peerIp(""), peerPort("") {}
		void clear() {
			socketFd = -1;
			peerIp = "";
			peerPort = "";
		}
		bool isValid() {
			return (
				socketFd > 0 &&
				peerIp != "" &&
				peerPort != ""
			);
		}
		string toStr() {
			stringstream ss;
			ss << "ConnectInfo: fd " << socketFd << ", " << peerIp << ":" << peerPort;
			return ss.str();
		}
	};

	/*
	 * helpful socket functions written by zq29
	*/
	namespace zqSok {
		
		// TODO: this is only an IPv4 version
		string getMyPortBySocket(const int socketFd) {
			sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if(getsockname(socketFd, (sockaddr*)&sin, &len) < 0) {
				Log::errorThenThrow("In function getMyPortBySocket(): failed to get socket name");
			}
			stringstream ss;
			ss << ntohs(sin.sin_port);
			return ss.str();
		}

		// TODO: this is only an IPv4 version
		string getPeerPortBySocket(const int socketFd) {
			sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getpeername(socketFd, (sockaddr*)&sin, &len) < 0) {
			   Log::errorThenThrow("In function getPeerPortBySocket(): failed to get socket name");
			}
			stringstream ss;
			ss << ntohs(sin.sin_port);
			return ss.str();
		}

		// TODO: this is only an IPv4 version
		string getPeerIpBySocket(const int socketFd) {
			sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getpeername(socketFd, (sockaddr*) &sin, &len) < 0) {
			   Log::errorThenThrow("In function getPeerPortBySocket(): failed to get socket name");
			}
			char ipStr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &sin.sin_addr, ipStr, sizeof(ipStr));
			return string(ipStr);
		}

		string getIpByAddrinfo(const addrinfo* const addr) {
			if(addr->ai_family == AF_INET) { // ipv4
				sockaddr_in* si = (sockaddr_in*)(&(addr->ai_addr));
				in_addr ipAddr = si->sin_addr;
				char str[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
				return string(str);
			}
			
			assert(addr->ai_family == AF_INET6);
			sockaddr_in6* si = (sockaddr_in6*)(&(addr->ai_addr));
			in6_addr ipAddr = si->sin6_addr;
			char str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &ipAddr, str, INET6_ADDRSTRLEN);
			return string(str);
		}

		string getIpBySockaddrStorage(const sockaddr_storage* const addr) {
			if(((sockaddr*)(&addr))->sa_family == AF_INET) { // ipv4
				sockaddr_in* si = (sockaddr_in*)(&addr);
				in_addr ipAddr = si->sin_addr;
				char str[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
				return string(str);
			}
			
			sockaddr_in6* si = (sockaddr_in6*)(&addr);
			in6_addr ipAddr = si->sin6_addr;
			char str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &ipAddr, str, INET6_ADDRSTRLEN);
			return string(str);
		}

		string getPortBySockaddrStorage(const sockaddr_storage* const addr) {
			stringstream ss;
			if(((sockaddr*)(&addr))->sa_family == AF_INET) { // ipv4
				sockaddr_in* si = (sockaddr_in*)(&addr);
				ss << ntohs(si->sin_port);
			} else {
				sockaddr_in6* si = (sockaddr_in6*)(&addr);
				ss << ntohs(si->sin6_port);
			}
			return ss.str();
		}

		/*
		 * helper function, send all data through a connected socket
		 * raise exception when failed to send
		*/
		void sendAll(const int socketFd, const char* const buffer, const size_t len) {
			size_t nSentBytes = 0;
			while(nSentBytes < len) {
				size_t tempN = send(socketFd, buffer + nSentBytes, len - nSentBytes, 0);
				if(tempN < 0) {
					Log::errorThenThrow("In function __sendAll(): failed to send");
				}
				nSentBytes += tempN;
			}
		}
		void sendAll(const int socketFd, const string& msg) {
			sendAll(socketFd, (char*)(&msg[0]), msg.length());
		}

		/*
		 * helper function, receive all data through a connected socket
		 * receive according to PotatoProtocol
		 * return bytes received, 0 on close
		 * raise exception when failed to receive
		*/
		size_t recvAll(const int socketFd, char* const buffer, const size_t len) {
			size_t nRecvBytes = 0;
			while(nRecvBytes < len) {
				size_t tempN = recv(socketFd, buffer + nRecvBytes, len - nRecvBytes, 0);
				if(tempN < 0) {
					Log::errorThenThrow("In function __recvAll(): failed to recv");
				} else if(tempN == 0) {
					return 0;
				}
				nRecvBytes += tempN;
			}
			return nRecvBytes;
		}

		/*
		 * connect to some machine, a helper function
		 * returns socketFd, ip, port, raise exception on error
		*/
		ConnectInfo connect(const char* hostname, const char* port) {
			ConnectInfo res;
			res.peerPort = string(port);

			struct addrinfo hostInfo;
			memset(&hostInfo, 0, sizeof(hostInfo));
			hostInfo.ai_family = AF_INET; // TODO: here it uses only ipv4
			hostInfo.ai_socktype = SOCK_STREAM;

			struct addrinfo* hostInfoList;
			if(getaddrinfo(hostname, port, &hostInfo, &hostInfoList) != 0) {
				Log::errorThenThrow("In function __connect(): cannot get address info");
			}

			struct addrinfo* p;
			for(p = hostInfoList; p != NULL; p = p->ai_next) {
				int socketFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if(socketFd < 0) {
					continue;
				}
				
				if(connect(socketFd, p->ai_addr, p->ai_addrlen) < 0) {
					continue;
				}

				// here it means socket was created and successfully connected
				// set result
				res.socketFd = socketFd;
				res.peerIp = zqSok::getIpByAddrinfo(p);
				freeaddrinfo(hostInfoList);
				return res;
			}

			freeaddrinfo(hostInfoList);
			Log::errorThenThrow("In function __connect(): failed to connect");
			return res; // this doesn't matter now
		}

		/*
		 * receive a Message string according to my protocol
		 * return a string which will be used to build a Message object
		 * return "" on server closing connection
		 * raise exception when failed to receive
		*/
		string recvPotatoProtocolAll(const int socketFd) {
			static const size_t firstPartSize = 11;

			char buffer[MAX_DATA_SIZE];
			size_t nRecvBytes = 0;
			/*
			* receive this part first, which is 11 bytes
			* +--------------------+----+------------------+----+
			* | IDENTIFIER("zq29") | \s | Length(5 digits) | \s |
			* +--------------------+----+------------------+----+
			*/
			size_t tempN = zqSok::recvAll(socketFd, buffer, firstPartSize);
			if(tempN == 0) {
				return "";
			}
			if(string(buffer, 4) != Message::IDENTIFIER) {
				Log::errorThenThrow(Log::msg(
					"In function recvPotatoProtocolAll(): ",
					"failed to recognize the msg receiver, whose first ",
					firstPartSize, " bytes are <",
					string(buffer, firstPartSize), ">"
				));
			}
			assert(tempN == firstPartSize);
			nRecvBytes += tempN;

			string lenStr(buffer + 5, 5);
			stringstream ss;
			ss << lenStr;
			size_t length; // payload length
			ss >> length; // could be 0

			length += 16; // 15 bytes action and 1 byte \t
			tempN = zqSok::recvAll(socketFd, buffer + firstPartSize, length);
			if(tempN == 0) {
				return "";
			}
			assert(tempN == length);
			nRecvBytes += length;

			return string(buffer, nRecvBytes);
		}

		/*
		 * a helper function for debug (assert)
		 * must be a valid IPv4 address with port number
		*/
		bool isAddrValid(const string& hostIp, const string& port) {
			struct sockaddr_in sa;
			bool isIpValid = inet_pton(AF_INET, hostIp.c_str(), &(sa.sin_addr)) > 0;

			stringstream ss;
			ss << port;
			int p;
			ss >> p;
			bool isPortValid = !ss.fail() && p > 1024 && p < 65536;

			return isIpValid && isPortValid;
		}

		/*
		 * listen to a port, return socketFd
		*/
		int startListening(const string& port, size_t backlog) {
			int listenSocket = -1;

			struct addrinfo hostInfo;
			memset(&hostInfo, 0, sizeof(hostInfo));

			hostInfo.ai_family = AF_INET;
			hostInfo.ai_socktype = SOCK_STREAM;
			hostInfo.ai_flags = AI_PASSIVE;

			struct addrinfo *hostInfoList;
			if(getaddrinfo(nullptr, port.c_str(), &hostInfo, &hostInfoList) != 0) {
				Log::errorThenThrow("In function startListening(): Cannot get addr info");
			}

			struct addrinfo *p;
			for(p = hostInfoList; p != NULL; p = p->ai_next) {
				listenSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

				if(listenSocket < 0) {
					continue;
				}

				int yes = 1;
				setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

				if(bind(listenSocket, p->ai_addr, p->ai_addrlen) < 0) {
					close(listenSocket);
					continue;
				}

				Log::verbose("In function startListening(): listening to port " + zqSok::getMyPortBySocket(listenSocket));
				
				break;
			}

			if(p == nullptr) {
				Log::errorThenThrow("In function startListening(): Cannot bind socket");
			}

			freeaddrinfo(hostInfoList);

			if(listen(listenSocket, backlog) < 0) {
				Log::errorThenThrow("In function startListening(): Cannot listen on socket");
			}

			return listenSocket;
		}



	}
}
	namespace zqSok = zq29Inner::zqSok;
	using zq29Inner::ConnectInfo;
}

#endif