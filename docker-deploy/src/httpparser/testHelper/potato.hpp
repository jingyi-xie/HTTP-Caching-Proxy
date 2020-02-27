#ifndef ZQ29_POTATO
#define ZQ29_POTATO

#include <set>
#include <sstream>
#include <memory>

#include "log.hpp"

namespace zq29 {
namespace zq29Inner {
	using namespace std;

	namespace PotatoProtocol {

		/*
		 * a Message object is used to send string info through socket
		 * message format:
		 * +--------------------+----+------------------+----+------------------+----+----------------------------+
		 * | IDENTIFIER("zq29") | \s | Length(5 digits) | \s | Action(15 Bytes) | \s | Payload (with no \n in it) |
		 * +--------------------+----+------------------+----+------------------+----+----------------------------+
		 *
		 * - Length: length of the Payload
		 * - Action: see ACTION_SET below
		 * - Payload: detail for Message's children class, NO \n allowed
		*/

		/*
		 * message examples:
		 * 
		 * 1. "zq29 4     NewPlayerVerify 1111"
		 * 2. "zq29 7     GameOver        success"
		 * 3. "zq29 18    ConRight        127.0.0.1 9999 1 3"
		 * 4. "zq29 14    YourLeftIs      127.0.0.1 8888"
		*/
		class Message {
		protected:
			const set<string> ACTION_SET = {
				"NewPlayerVerify", 	// player send to master to verify id
				"GameOver",			// player send to master to inform exit
				"ConRight",			// master tell a player to connect to its right neighbor
				"PlayerReady",		// a player tells master that it's ready for the game
				"Potato" 			// a hot potato!
			};

			size_t length;
			string action;
			string payload;

		public:
			/*
			 * send at the beginning of every message
			 * to verify this message is sent by this program
			 * lower the chance of being attacked
			*/
			static const string IDENTIFIER;

			enum Type { NewPlayerVerify, GameOver, ConRight, PlayerReady, Potato };
			Type getType() {
				if(action == "NewPlayerVerify") { return NewPlayerVerify; }
				else if(action == "GameOver") { return GameOver; }
				else if(action == "ConRight") { return ConRight; }
				else if(action == "PlayerReady") { return PlayerReady; }
				else if(action == "Potato") { return Potato; }
				
				assert(false);
				return NewPlayerVerify;
			}
			/*
			 * build a Message object from raw string
			 * when fail, throw an exception
			*/
			virtual void build(const string& rawMsg) {
				if(rawMsg == "") {
					Log::errorThenThrow(Log::msg(
						"In function build(): detected unexpected socket disconnection!"
					));
				}

				stringstream ss;
				ss << rawMsg;

				// verify the message sender
				string tempId;
				ss >> tempId;
				if(!ss || tempId != IDENTIFIER) {
					Log::errorThenThrow(Log::msg(
						"In function build(): failed to identify the message <",  rawMsg, ">"
					));
				}

				// get length
				ss >> length;
				if(!ss) {
					Log::errorThenThrow("In function build(): failed to parse length");
				}

				ss >> action;
				if(!ss || ACTION_SET.find(action) == ACTION_SET.end()) {
					Log::errorThenThrow("In function build(): bad action <" + action + ">");
				}

				size_t nIgnore = 16 - action.length(); // 15 bytes action plus a \s
				ss.ignore(nIgnore); // ignore \s
				getline(ss, payload);

				if(payload.length() != length) {
					Log::errorThenThrow(Log::msg(
						"In function build(): failed to get payload, ",
						"length expected ", length,
						", got <", payload, "> with length ", payload.length()
					));
				}
			}

			/*
			 * return if the object is valid
			*/
			bool isValid() {
				return (
					ACTION_SET.find(action) != ACTION_SET.end() &&
					payload.length() == length
				);
			}

			/*
			 * when fail, throw an exception
			*/
			virtual string toStr() {
				if(!isValid()) {
					Log::errorThenThrow("In function toStr(): the object is not valid");
				}
				stringstream ss, helperss;
				ss << IDENTIFIER << "\t";

				ss << length;
				helperss << length;
				string lenStr;
				helperss >> lenStr;
				// 5 digits plus one \s
				for(size_t i = 0; i < 6 - lenStr.length(); i++) {
					ss << "\t";
				}

				ss << action;
				// 15 bytes plus one \s
				for(size_t i = 0; i < 16 - action.length(); i++) {
					ss << "\t";
				}

				ss << payload;

				return ss.str();
			}
		};

		const string Message::IDENTIFIER = "zq29";

		/*
		 * player send to master to verify id
		 * no payload in it
		*/
		class NewPlayerVerifyMessage : public Message {
		private:
			string listenPort;
		public:
			NewPlayerVerifyMessage() {}
			NewPlayerVerifyMessage(const string& port) {
				length = 0;
				action = "NewPlayerVerify";
				payload = port;
				listenPort = port;
			}

			string getPort() const { return listenPort; }

			void build(const string& rawMsg) override {
				Message::build(rawMsg);
				// now parse the payload
				stringstream ss;
				ss << payload;
				ss >> listenPort;
				if(listenPort.length() == 0) { // simple check
					Log::errorThenThrow("In function build(): failed to parse port in NewPlayerVerifyMessage");
				}
			}

			string toStr() override {
				if(listenPort == "") {
					Log::errorThenThrow("In function build(): this NewPlayerVerifyMessage obj is not valid");
				}
				action = "NewPlayerVerify";
				payload = listenPort;
				length = payload.length();
				return Message::toStr();
			}
		};

		/*
		 * player send to master to inform exit
		 * no payload in it
		*/
		class GameOverMessage : public Message {
		private:
			/*
			 * "success": game exit successfully
			 * "<whatever reason>": explains why
			*/
			string result;
		public:
			static const string SUCCESS;

			GameOverMessage() {}
			GameOverMessage(const string& result) {
				length = 0;
				action = "GameOver";
				payload = result;
				this->result = result;
			}

			string getResult() const { return result; }

			void build(const string& rawMsg) override {
				Message::build(rawMsg);
				// now parse the payload
				result = payload;
			}

			string toStr() override {
				if(result == "") {
					Log::errorThenThrow("In function build(): this GameOverMessage obj is not valid");
				}
				action = "GameOver";
				payload = result;
				length = payload.length();
				return Message::toStr();
			}
		};

		const string GameOverMessage::SUCCESS = "success";

		class ConRightMessage : public Message {
		private:
			string ip;
			string port;
			/*
			 * tell the player its id BTW
			*/
			string yourID;
			string totalPlayers;
		public:
			ConRightMessage() {}
			ConRightMessage(const string& ip, const string& port, const string& id, const string& n) {
				// no need to verify ip & port here in the message
				action = "ConRight";
				payload = ip + "\t" + port + "\t" + id;
				length = payload.length();
				this->ip = ip;
				this->port = port;
				this->yourID = id;
				this->totalPlayers = n;
			}
			
			string getIp() const { return ip; }
			string getPort() const { return port; }
			string getId() const { return yourID; }
			string getTotalPlayers() const { return totalPlayers; }

			void build(const string& rawMsg) override {
				Message::build(rawMsg);
				// now parse the payload
				stringstream ss;
				ss << payload;
				ss >> ip;
				if(!ss || ip.length() == 0) { // simple check
					Log::errorThenThrow("In function build(): failed to parse ip in ConRightMessage");
				}
				ss >> port;
				if(!ss || port.length() == 0) { // simple check
					Log::errorThenThrow("In function build(): failed to parse port in ConRightMessage");
				}
				ss >> yourID;
				if(!ss || yourID.length() == 0) { // simple check
					Log::errorThenThrow("In function build(): failed to parse id in ConRightMessage");
				}
				ss >> totalPlayers;
				if(totalPlayers.length() == 0) { // simple check
					Log::errorThenThrow("In function build(): failed to parse totalPlayers in ConRightMessage");
				}
			}

			string toStr() override {
				if(ip == "" || port == "") {
					Log::errorThenThrow("In function build(): this ConRightMessage obj is not valid");
				}
				action = "ConRight";
				payload = ip + "\t" + port + "\t" + yourID + "\t" + totalPlayers;
				length = payload.length();
				return Message::toStr();
			}
		};

		class PlayerReadyMessage : public Message {
		public:
			PlayerReadyMessage() {
				length = 0;
				action = "PlayerReady";
				payload = "";
			}
		};

		class PotatoMessage : public Message {
		private:
			size_t hops;
			string tracks;
		public:
			PotatoMessage() {}
			PotatoMessage(const size_t hops) : tracks("") {
				// no need to verify ip & port here in the message
				action = "Potato";
				stringstream ss;
				ss << hops << "\t" << tracks;
				payload = ss.str();
				length = payload.length();
				this->hops = hops;
			}

			size_t getHops() const { return hops; }
			string getTracks() const { return tracks; }
			string getPrettyTracks() const {
				string s = tracks;
				if(s[s.length() - 1] == '\t') {
					s = s.substr(0, s.length() - 1);
				}
				for(size_t i = 0; i < s.length(); i++) {
					if(s[i] == '\t') {
						s[i] = ',';
					}
				}
				return s;
			}
			void append(const string& id) {
				tracks = tracks + id + "\t";
			}
			void decreaseHop() {
				if(hops == 0) {
					Log::errorThenThrow("In functino decreaseHop(): a potato is not handled properly");
				}
				hops--;
			}

			void build(const string& rawMsg) override {
				Message::build(rawMsg);
				// now parse the payload
				stringstream ss;
				ss << payload;
				ss >> hops;
				if(!ss) { // simple check
					Log::errorThenThrow("In function build(): failed to parse hops in YourLeftIsessage");
				}
				ss.ignore();
				getline(ss, tracks);
			}

			string toStr() override {
				action = "Potato";
				stringstream ss;
				ss << hops << "\t" << tracks;
				payload = ss.str();
				length = payload.length();
				return Message::toStr();
			}
		};

		/*
		 * build a message from raw str
		 * interface to use all the above class
		 * may throw exceptions
		*/
		shared_ptr<Message> buildMsg(const string& rawStr) {
			Message msg;
			msg.build(rawStr);
			switch(msg.getType()) {
				case Message::NewPlayerVerify: {
					NewPlayerVerifyMessage msg;
					msg.build(rawStr);
					return make_shared<NewPlayerVerifyMessage>(move(msg));
				}
				case Message::GameOver:{
					GameOverMessage msg;
					msg.build(rawStr);
					return make_shared<GameOverMessage>(move(msg));
				}
				case Message::ConRight: {
					ConRightMessage msg;
					msg.build(rawStr);
					return make_shared<ConRightMessage>(move(msg));
				}
				case Message::PlayerReady: {
					PlayerReadyMessage msg;
					msg.build(rawStr);
					return make_shared<PlayerReadyMessage>(move(msg));
				}
				case Message::Potato: {
					PotatoMessage msg;
					msg.build(rawStr);
					return make_shared<PotatoMessage>(move(msg));
				}
				default:
					Log::errorThenThrow("In function buildMsg(): failed to recognize msg type");
					break;
			}
			Log::errorThenThrow("In function buildMsg(): failed to build msg");
			return nullptr;
		}
	}

}
	using namespace zq29Inner::PotatoProtocol;
}

#endif