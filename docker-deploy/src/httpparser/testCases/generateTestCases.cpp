#include "socket.hpp" // from project 3 of ECE 650
#include "log.hpp"
#include <set>
#include <fstream>
using namespace zq29::zqSok;
using namespace zq29;
using namespace std;

int main() {
	const size_t bufferSize = 4096;
	const int listenSocket = startListening("1234", 5);

	set<string> requestStrSet;

	int fileIndex = 0;
	while(true) {
		sockaddr_storage socketAddr;
		socklen_t socketAddrLen = sizeof(socketAddr);
		Log::verbose("waiting accept...");
		const int socketFd = accept(listenSocket, (sockaddr*)&socketAddr, &socketAddrLen);
		Log::verbose("accepted!");

		Log::verbose("waiting recv...");
		char buffer[bufferSize];
		int len = recv(socketFd, buffer, bufferSize, 0);
		Log::verbose("received!");

		const string recvStr(buffer, len);
		//Log::verbose(Log::msg("Received:\n", recvStr, "\nEND"));
		close(socketFd);

		// simple de-duplicate
		if(requestStrSet.find(recvStr) == requestStrSet.end()) {
			requestStrSet.insert(recvStr);

			stringstream ss;
			ss << "validRequest" << fileIndex << ".txt";
			fileIndex++;
			ofstream ofs;
			ofs.open(ss.str());
			assert(ofs);
			ofs.write(recvStr.c_str(), recvStr.length());
			ofs.close();
			Log::success("write to file " + ss.str());
		} else {
			Log::verbose("duplicate detected!");
		}
	}
}