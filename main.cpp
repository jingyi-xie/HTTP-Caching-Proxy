#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include "httpparser/httpparser.hpp"
#include "cache/httpproxycache.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define BACKLOG 500
#define RETRY 1000

using namespace zq29;
using namespace std;



class Proxy {
private:
	int listen_fd;
	char port_num[NI_MAXSERV];

	void sendAll(const int socketFd, const char* const buffer, const size_t len) {
		size_t nSentBytes = 0;
		while(nSentBytes < len) {
			size_t tempN = send(socketFd, buffer + nSentBytes, len - nSentBytes, 0);
			if(tempN < 0) {
            throw runtime_error("failed to sendAll");
				return;
			}
			nSentBytes += tempN;
		}
	}

	void sendAll(const int socketFd, const string& msg) {
      Log::debug(Log::msg("in sendAll: sending ", msg.length(), " bytes!!!!!!!!!!!!!!!"));
		sendAll(socketFd, (char*)(&msg[0]), msg.length());
	}

	int connectServer(const char* hostname, const char* port) {
		int server_fd;
		struct addrinfo hostInfo;
		struct addrinfo* hostInfoList;
		memset(&hostInfo, 0, sizeof(hostInfo));
		hostInfo.ai_family = AF_UNSPEC; 
		hostInfo.ai_socktype = SOCK_STREAM;
		hostInfo.ai_flags = AI_NUMERICSERV;
		if(getaddrinfo(hostname, port, &hostInfo, &hostInfoList) != 0) {
			return -1;
		}
		struct addrinfo* p;
		for(p = hostInfoList; p != NULL; p = p->ai_next) {
			server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if(server_fd < 0) {
				continue;
			}
			if(connect(server_fd, p->ai_addr, p->ai_addrlen) < 0) {
				continue;
			}
			// here it means socket was created and successfully connected
			// set result
			freeaddrinfo(hostInfoList);
			return server_fd;
		}
		freeaddrinfo(hostInfoList);
		return -1; // this doesn't matter now
	}

   int recvAppend(const int socketFd, vector<char>& buffer) {
      const size_t bufferSize = 1024 * 64;
      char charbuffer[bufferSize];
      int len = recv(socketFd, charbuffer, bufferSize, 0);
      if(len <= 0) { return len; }
      const vector<char> temp(charbuffer, charbuffer + len);
      buffer.insert(buffer.end(), temp.begin(), temp.end());
      return len;
   }

   HTTPRequest recvRequest(const int client_fd) {
      HTTPRequestParser reqParser1st;
      HTTPRequest req1st;
      vector<char> buffer1st;
      for(int _ = 0; _ < RETRY; _++) {
         if(recvAppend(client_fd, buffer1st) < 0) { break; }
         reqParser1st.setBuffer(buffer1st);
         try {
            req1st = reqParser1st.build();
            return req1st;
         } catch(const exception& e) {
            Log::debug(Log::msg("error in handleRequest(): ", e.what()));
         }
      }
      return HTTPRequest();
   }

   HTTPStatus recvStatus(const int server_fd) {
      HTTPStatusParser staParser;
      HTTPStatus sta;
      vector<char> buffer;
      for(int _ = 0; _ < RETRY; _++) {
         if(recvAppend(server_fd, buffer) < 0) { break; }
         staParser.setBuffer(buffer);
         try {
            sta = staParser.build();
            return sta;
         } catch(const exception& e) {
            Log::debug(Log::msg("error in handleRequest(): ", e.what()));
         }
      }
      return HTTPStatus();
   }

   void handleGET(const HTTPRequest& req, const int client_fd) {
      auto consRespResult = HTTPProxyCache::getInstance().constructResponse(req);

      if(consRespResult.action == 0) {
         Log::debug("in handleRequest(): Send back content from cache");
         const string respStr = consRespResult.resp.toStr();
         sendAll(client_fd, respStr);
      } else if(consRespResult.action == 1) {
         Log::debug("in handleRequest(): no valid content from cache");

         HTTPRequestParser::AbsoluteForm af = HTTPRequestParser::parseAbsoluteForm(req);
         const char* addr = af.authorityForm.host.c_str();
         const char* port = af.authorityForm.port == "" ? "80" : af.authorityForm.port.c_str();
         int server_fd = connectServer(addr, port);
         if (server_fd == -1) {
            Log::warning("failed to connect to server, ignore this request");
            return;
         }
         
         // contact server
         try {
            sendAll(server_fd, req.toStr());
         } catch(const exception& e) {
            close(server_fd);
            try {
               sendAll(client_fd, getHTTP502HTMLStr(e.what()));
            } catch(...) {}
            return;
         }

         // send response to client
         const HTTPStatus status = recvStatus(server_fd);
         if(status == HTTPStatus()) {
            try {
               sendAll(client_fd, getHTTP502HTMLStr("Received illegal response from server"));
            } catch(...) {
               close(server_fd);
            }
            return;
         }

         HTTPProxyCache::getInstance().save(req, status);
         sendAll(client_fd, status.toStr());

         // TODO
         /*
         while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
            send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
         }
         */
         //close(client_fd);
         //close(server_fd);
         close(server_fd);

      } else if(consRespResult.action == 2) {
         Log::debug("Begin re-validation!");
         HTTPRequestParser::AbsoluteForm af = HTTPRequestParser::parseAbsoluteForm(req);
         const char* addr = af.authorityForm.host.c_str();
         const char* port = af.authorityForm.port == "" ? "80" : af.authorityForm.port.c_str();
         int server_fd = connectServer(addr, port);
         if (server_fd == -1) {
            Log::warning("failed to connect to server, ignore this request");
            return;
         }
         // send re-validation to server
         try {
            sendAll(server_fd, consRespResult.validationReq.toStr());
         } catch(const exception& e) {
            close(server_fd);
            try {
               sendAll(client_fd, getHTTP502HTMLStr(e.what()));
            } catch(...) {
               close(client_fd);
            }
            return;
         }

         // get reuslt from server
         const HTTPStatus sta = recvStatus(server_fd);
         if(sta == HTTPStatus()) {
            close(server_fd);
            try {
               sendAll(client_fd, getHTTP502HTMLStr("while revalidating, we don't understand what server said"));
            } catch(...) {
               close(client_fd);
            }
            return;
         }

         // result is either 304 or 200
         if(sta.statusLine.statusCode == "200") {
            HTTPProxyCache::getInstance().save(req, sta);
            try {
               sendAll(client_fd, sta.toStr());
            } catch(...) {
               close(client_fd);
               return;
            }
         } else if(sta.statusLine.statusCode == "304") {
            try {
               sendAll(client_fd, consRespResult.resp.toStr());
            } catch(...) {
               close(client_fd);
               return;
            }   
         } else {
            try {
               sendAll(client_fd, getHTTP502HTMLStr("while revalidating, server returned neither 200 nor 304"));
            } catch(...) {
               close(client_fd);
               return;
            }
         }
      }
   }

   void handlePOST(const HTTPRequest& req, const int client_fd) {
      HTTPRequestParser::AbsoluteForm af = HTTPRequestParser::parseAbsoluteForm(req);
      const char* addr = af.authorityForm.host.c_str();
      const char* port = af.authorityForm.port == "" ? "80" : af.authorityForm.port.c_str();
      int server_fd = connectServer(addr, port);
      if (server_fd == -1) {
         Log::warning("failed to connect to server, ignore this request");
         return;
      }
      
      // contact server
      try {
         sendAll(server_fd, req.toStr());
      } catch(const exception& e) {
         close(server_fd);
         try {
            sendAll(client_fd, getHTTP502HTMLStr(e.what()));
         } catch(...) {}
         return;
      }

      // send response to client
      const HTTPStatus status = recvStatus(server_fd);
      if(status == HTTPStatus()) {
         try {
            sendAll(client_fd, getHTTP502HTMLStr("Received illegal response from server"));
         } catch(...) {
            close(server_fd);
         }
         return;
      }

      sendAll(client_fd, status.toStr());

      // TODO
      /*
      while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
         send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
      }
      */
      //close(client_fd);
      //close(server_fd);
      close(server_fd);
   }

   void handleConnect(const HTTPRequest& req, const int client_fd) {
      auto const af = HTTPRequestParser::parseAuthorityForm(req);
      const int server_fd = connectServer(af.host.c_str(), af.port.c_str());
      if (server_fd == -1) {
         Log::warning("in handleConnect(): failed to connect to server, ignore this request");
         return;
      }
      try {
         sendAll(client_fd, "HTTP/1.1 200 OK\r\n\r\n");
      } catch(const exception& e) {
         Log::warning("in handleConnect(): failed to return 200 to client");
         return;
      }

      const size_t bufferSize = 1024 * 64;
      char msg_buffer[bufferSize];
      while(true) {
         fd_set readfds;
         FD_ZERO(&readfds);
         FD_SET(server_fd, &readfds);
         FD_SET(client_fd, &readfds);
         select(max(server_fd, client_fd) + 1, &readfds, NULL, NULL, NULL);
         int ready_fd; 
         int other_fd;
         if (FD_ISSET(server_fd, &readfds)) {  // first priority (terminating signal)
            ready_fd = server_fd;
            other_fd = client_fd;
         }
         else if (FD_ISSET(client_fd, &readfds)) {
            ready_fd = client_fd;
            other_fd = server_fd;
         }
         int msg_len = recv(ready_fd, msg_buffer, sizeof(msg_buffer), 0);
         if (msg_len <= 0) {
            break;
         }
         if (send(other_fd, msg_buffer, msg_len, MSG_NOSIGNAL) == -1) {
            break;
         }
      }       
      close(client_fd);
      close(server_fd);
   }

   void handleRequest(const int client_fd) {
      // recv the 1st request
      HTTPRequest req1st = recvRequest(client_fd);
      if(req1st == HTTPRequest()) {
         close(client_fd);
         Log::debug("in handleRequest(): failed to get 1st request");
         return;
      }

      if(req1st.requestLine.method == "GET") {
         handleGET(req1st, client_fd);
      } else if(req1st.requestLine.method == "POST") {
         handlePOST(req1st, client_fd);
      } else if(req1st.requestLine.method == "CONNECT") {
         handleConnect(req1st, client_fd);
      } else {
         assert(false);
      }

      close(client_fd);

      /*
      if(request.requestLine.method == "POST") {
         
         else {
            Log::debug("in handleRequest(): no valid content from cache");
            HTTPRequestParser::AbsoluteForm af = HTTPRequestParser::parseAbsoluteForm(request);
            const char* addr = af.authorityForm.host.c_str();
            const char* port = af.authorityForm.port == "" ? "80" : af.authorityForm.port.c_str();
            int server_fd = connectServer(addr, port);
            if (send(server_fd, request.toStr().c_str(), request.toStr().length(), MSG_NOSIGNAL) == -1) {
               cerr << "Error: send request to server" << endl;
               return;
            }
            
            msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0);
            
            HTTPStatusParser statusParser;
            HTTPStatus status;
            statusParser.setBuffer(vector<char>(response_buffer, response_buffer + msg_len));   
            try {
               status = statusParser.build();
            } catch(...) {}
            HTTPProxyCache::getInstance().save(request, status);

            sendAll(client_fd, status.toStr());

            while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
               send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
            }
            //close(client_fd);
            close(server_fd);
         }

         //send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);

         //while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
         // send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
         //}
         close(client_fd);
         //close(server_fd);
      }
      else if(request.requestLine.method == "CONNECT") {
         auto af = HTTPRequestParser::parseAuthorityForm(request);
         int server_fd = connectServer(af.host.c_str(), af.port.c_str());
         if (server_fd == -1) {
            cerr << "Error: cannot connect to server" << endl;
            return;
         }
         if (send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0) == -1) {
            cerr << "CONNECT: sent 200 OK to client" << endl;
            return;
         }
         char msg_buffer[bufferSize];
         while(true) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            FD_SET(client_fd, &readfds);
            select(max(server_fd, client_fd) + 1, &readfds, NULL, NULL, NULL);
            int ready_fd; 
            int other_fd;
            if (FD_ISSET(server_fd, &readfds)) {  // first priority (terminating signal)
               ready_fd = server_fd;
               other_fd = client_fd;
            }
            else if (FD_ISSET(client_fd, &readfds)) {
               ready_fd = client_fd;
               other_fd = server_fd;
            }
            int msg_len = recv(ready_fd, msg_buffer, sizeof(msg_buffer), 0);
            if (msg_len == -1) {
               cerr << "CONNECT: receive data" << endl;
               break;
            }
            else if (msg_len == 0) {
               break;
            }
            if (send(other_fd, msg_buffer, msg_len, MSG_NOSIGNAL) == -1) {
               cerr << "CONNECT: send data" << endl;
               break;
            }
         }       
         close(client_fd);
         close(server_fd);
      }
      else {
         close(client_fd);
      }
      */
   }

   /*
	void handleRequest(size_t client_id, int client_fd, sockaddr_storage socket_addr) {
		const size_t bufferSize = 1024 * 64;
		char buffer[bufferSize];
		int len = recv(client_fd, buffer, bufferSize, 0);
		const string recvStr(buffer, len);
		HTTPRequestParser requestParser;
		HTTPRequest request;
		requestParser.setBuffer(vector<char>(buffer, buffer + len));
		try {
			request = requestParser.build();
		}
		catch(const HTTPParser::HTTPParserException& e) {
		  //	cerr << "While building request, HTTPParserException: " << e.what();
		} catch(const HTTPRequestParser::HTTP400Exception& e) {
			// may be "HEAD"
		}
		if (request.requestLine.method == "GET" || request.requestLine.method == "POST") {
			auto consRespResult = HTTPProxyCache::getInstance().constructResponse(request);
			char response_buffer[bufferSize];
			int msg_len = -1;

			if(consRespResult.action == 0 || consRespResult.action == 2) {
				Log::debug("in handleRequest(): Send back content from cache");
				const string respStr = consRespResult.resp.toStr();
				//Log::verbose(respStr);
				//strcpy(response_buffer, respStr.c_str());
				//msg_len = respStr.length();
				sendAll(client_fd, respStr);
			}
			else {
				Log::debug("in handleRequest(): no valid content from cache");
				HTTPRequestParser::AbsoluteForm af = HTTPRequestParser::parseAbsoluteForm(request);
				const char* addr = af.authorityForm.host.c_str();
				const char* port = af.authorityForm.port == "" ? "80" : af.authorityForm.port.c_str();
				int server_fd = connectServer(addr, port);
				if (send(server_fd, request.toStr().c_str(), request.toStr().length(), MSG_NOSIGNAL) == -1) {
					cerr << "Error: send request to server" << endl;
					return;
				}
				
				msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0);
				
				HTTPStatusParser statusParser;
				HTTPStatus status;
				statusParser.setBuffer(vector<char>(response_buffer, response_buffer + msg_len));	
				try {
					status = statusParser.build();
				} catch(...) {}
				HTTPProxyCache::getInstance().save(request, status);

				sendAll(client_fd, status.toStr());

				while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
					send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
				}
				//close(client_fd);
				close(server_fd);
			}

			//send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);

			//while ((msg_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0)) > 0) {
			//	send(client_fd, response_buffer, msg_len, MSG_NOSIGNAL);
			//}
			close(client_fd);
			//close(server_fd);
		}
		else if(request.requestLine.method == "CONNECT") {
			auto af = HTTPRequestParser::parseAuthorityForm(request);
			int server_fd = connectServer(af.host.c_str(), af.port.c_str());
			if (server_fd == -1) {
				cerr << "Error: cannot connect to server" << endl;
				return;
			}
			if (send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0) == -1) {
				cerr << "CONNECT: sent 200 OK to client" << endl;
				return;
			}
			char msg_buffer[bufferSize];
			while(true) {
				fd_set readfds;
				FD_ZERO(&readfds);
				FD_SET(server_fd, &readfds);
				FD_SET(client_fd, &readfds);
				select(max(server_fd, client_fd) + 1, &readfds, NULL, NULL, NULL);
				int ready_fd; 
				int other_fd;
				if (FD_ISSET(server_fd, &readfds)) {  // first priority (terminating signal)
					ready_fd = server_fd;
					other_fd = client_fd;
				}
				else if (FD_ISSET(client_fd, &readfds)) {
					ready_fd = client_fd;
					other_fd = server_fd;
				}
				int msg_len = recv(ready_fd, msg_buffer, sizeof(msg_buffer), 0);
				if (msg_len == -1) {
					cerr << "CONNECT: receive data" << endl;
					break;
				}
				else if (msg_len == 0) {
					break;
				}
				if (send(other_fd, msg_buffer, msg_len, MSG_NOSIGNAL) == -1) {
					cerr << "CONNECT: send data" << endl;
					break;
				}
			}		  
			close(client_fd);
			close(server_fd);
		}
		else {
			close(client_fd);
		}
	}
   */


public:
	Proxy(const char * port) {
		snprintf(port_num, sizeof(port_num), "%s", port);
	}
	void start() {
		int status;
		struct addrinfo host_info;
		struct addrinfo *host_info_list;

		memset(&host_info, 0, sizeof(host_info));

		host_info.ai_family = AF_UNSPEC;
		host_info.ai_socktype = SOCK_STREAM;
		host_info.ai_flags = AI_PASSIVE;

		status = getaddrinfo(NULL, port_num, &host_info, &host_info_list);
		if (status != 0) {
			cerr << "Error: cannot get address info for host" << endl;
			exit(EXIT_FAILURE);
		} // if

		listen_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
									host_info_list->ai_protocol);
		if (listen_fd == -1) {
			cerr << "Error: cannot create socket" << endl;
			exit(EXIT_FAILURE);
		} // if

		int yes = 1;
		status = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		status = bind(listen_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);

		if (status == -1) {
			cerr << "Error: cannot bind socket" << endl;
			exit(EXIT_FAILURE);
		} // if

		status = listen(listen_fd, BACKLOG);
		if (status == -1) {
			cerr << "Error: cannot listen on socket" << endl;
			exit(EXIT_FAILURE);
		} // if

		while(true) {
			struct sockaddr_storage socket_addr;
			socklen_t socket_addr_len = sizeof(socket_addr);
			int client_fd;
			client_fd = accept(listen_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
			if (client_fd == -1) {
				cerr << "Error: cannot accept connection on socket" << endl;
				exit(EXIT_FAILURE);
			} // if
			std::thread t(&Proxy::handleRequest, this, client_fd);
			t.detach();
		}
		freeaddrinfo(host_info_list);
		close(listen_fd);
	}
};


int main() {
	HTTPProxyCache::createInstance();
	Proxy p("12345");
	p.start();
}