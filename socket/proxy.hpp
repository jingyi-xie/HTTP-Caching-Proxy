#ifndef PROXY_H
#define PROXY_H

#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include "../httpparser/httpparser.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define BACKLOG 500

using namespace zq29;
using namespace std;
class Proxy {
private:
   int listen_fd;
   char port_num[NI_MAXSERV];
   size_t client_id;

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

   void handleRequest(size_t client_id, int client_fd, sockaddr_storage socket_addr) {
      const size_t bufferSize = 1024 * 64;
      char buffer[bufferSize];
      int len = recv(client_fd, buffer, bufferSize, 0);
      const string recvStr(buffer, len);
      HTTPRequestParser requestParser;
      HTTPRequest request;
      requestParser.setBuffer(vector<char>(buffer, buffer + len));
      request = requestParser.build();
      if(request.requestLine.method == "CONNECT") {
         auto af = HTTPRequestParser::parseAuthorityForm(request);
         int server_fd = connectServer(af.host.c_str(), af.port.c_str());
         if (server_fd == -1) {
            cerr << "Error: cannot connect to server" << endl;
            exit(EXIT_FAILURE);
         }
         if (send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0) == -1) {
            cerr << "CONNECT: sent 200 OK to client" << endl;
            exit(EXIT_FAILURE);
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
               exit(EXIT_FAILURE);
            }
            else if (msg_len == 0) {
               break;
            }
            if (send(other_fd, msg_buffer, msg_len, MSG_NOSIGNAL) == -1) {
               cerr << "CONNECT: send data" << endl;
               exit(EXIT_FAILURE);
            }
         }        
         close(client_fd);
         close(server_fd);
      }
      else {
         close(client_fd);
      }
   }


public:
   Proxy(const char * port) {
      snprintf(port_num, sizeof(port_num), "%s", port);
      client_id = 0;
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
         std::thread t(&Proxy::handleRequest, this, client_id++, client_fd, socket_addr);
         t.detach();
      }
      freeaddrinfo(host_info_list);
      close(listen_fd);
   }
};

#endif
