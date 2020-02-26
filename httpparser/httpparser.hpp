#ifndef ZQ29_HTTPPARSER
#define ZQ29_HTTPPARSER

#include <set>
#include <tuple>
#include <vector>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include "../log.hpp"

/*

Search BROKEN to find places where this program breaks the HTTP standard

*/

/*

Below are references for this program's implementation;
every behavior in every function can be found in below comments

To get start with HTTP, see https://tools.ietf.org/html/rfc7230#section-2

3. Message Format (https://tools.ietf.org/html/rfc7230#section-3)
HTTP-message = start-line
				*( header-field CRLF )
				CRLF
				[ message-body ]


	* About CR LF: https://stackoverflow.com/questions/5757290/http-header-line-break-style
	May ignore proceding CR, but this program won't (use strictly CR LF)

	* Whitespaces between start-line and header-field should cause the message be rejected

The normal procedure for parsing an HTTP message is to read the
start-line into a structure, read each header field into a hash table
by field name until the empty line, and then use the parsed data to
determine if a message body is expected.  If a message body has been
indicated, then it is read as a stream until an amount of octets
equal to the message body length is read or the connection is closed.

3.1 Start Line
start-line = request-line / status-line

3.1.1 Request Line
request-line = method SP request-target SP HTTP-version CRLF

[BROKEN] every error that cannot be recognized will result in 
returning an HTTP 400 error; some errors mentioned in the standard
are treated as unrecognized errors

* method (section-4)
	One element in set { "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE" }
	This program only supports { "GET", "POST", "CONNECT" }

* request-target (section-5.3)
	request-target = absolute-form / authority-form / ...

	absolute-form = absolute-URI
	absolute-URI  = scheme ":" hier-part [ "?" query ] (https://tools.ietf.org/html/rfc3986#section-4.3)
	hier-part     = "//" authority path-abempty (rfc 3986 page 49)
					 / path-absolute
					 / path-rootless
					 / path-empty
	authority-form = authority (rfc 7230 5.3.3)

	

	When making a request to a proxy, other than a CONNECT or server-wide
	OPTIONS request (as detailed below), a client MUST send the target
	URI in absolute-form as the request-target

	e.g. GET http://www.example.org/pub/WWW/TheProject.html HTTP/1.1

	The authority-form of request-target is only used for CONNECT requests

	e.g. CONNECT www.example.com:80 HTTP/1.1

	According to 3.1.1, this program responses 400 (Bad Request) error
	(with the request-target properly encoded, [BROKEN], not implemented here), 
	when there are spaces in one of the three compoents of the request line

3.1.2 Status Line
status-line = HTTP-version SP status-code SP reason-phrase CRLF

* HTTP-version = "HTTP/" DIGIT "." DIGIT (section-2.6)
* status-code = 3DIGIT
* reason-phrase  = *( HTAB / SP / VCHAR / obs-text )

3.2 Header Fields
header-field = field-name ":" OWS field-value OWS
OWS = *( SP / HTAB )

[BROKEN 3.2.4] multi-line field-value won't be supported by this program
(https://stackoverflow.com/questions/31237198/is-it-possible-to-include-multiple-crlfs-in-a-http-header-field),
and will cause an HTTP 400 (bad request) error



3.3.  Message Body (https://tools.ietf.org/html/rfc7230#section-3.3)

3.3.3.  Message Body Length

   The length of a message body is determined by one of the following
   (in order of precedence):

   1.  ... and any response with a 1xx
       (Informational), 204 (No Content), or 304 (Not Modified) status
       code is always terminated by the first empty line after the
       header fields, regardless of the header fields present in the
       message, and thus cannot contain a message body.

   2.  Any 2xx (Successful) response to a CONNECT request implies that
       the connection will become a tunnel immediately after the empty
       line that concludes the header fields.  A client MUST ignore any
       Content-Length or Transfer-Encoding header fields received in
       such a message.

   3.  If a Transfer-Encoding header field is present and the chunked
       transfer coding (Section 4.1) is the final encoding, the message
       body length is determined by reading and decoding the chunked
       data until the transfer coding indicates the data is complete.

       If a Transfer-Encoding header field is present in a response and
       the chunked transfer coding is not the final encoding, the
       message body length is determined by reading the connection until
       it is closed by the server.  If a Transfer-Encoding header field
       is present in a request and the chunked transfer coding is not
       the final encoding, the message body length cannot be determined
       reliably; the server MUST respond with the 400 (Bad Request)
       status code and then close the connection.

       If a message is received with both a Transfer-Encoding and a
       Content-Length header field, the Transfer-Encoding overrides the
       Content-Length.  Such a message might indicate an attempt to
       perform request smuggling (Section 9.5) or response splitting
       (Section 9.4) and ought to be handled as an error.  A sender MUST
       remove the received Content-Length field prior to forwarding such
       a message downstream.

   4.  If a message is received without Transfer-Encoding and with
       either multiple Content-Length header fields having differing
       field-values or a single Content-Length header field having an
       invalid value, then the message framing is invalid and the
       recipient MUST treat it as an unrecoverable error.  If this is a
       request message, the server MUST respond with a 400 (Bad Request)
       status code and then close the connection.  If this is a response
       message received by a proxy, the proxy MUST close the connection
       to the server, discard the received response, and send a 502 (Bad
       Gateway) response to the client.  If this is a response message
       received by a user agent, the user agent MUST close the
       connection to the server and discard the received response.

   5.  If a valid Content-Length header field is present without
       Transfer-Encoding, its decimal value defines the expected message
       body length in octets.  If the sender closes the connection or
       the recipient times out before the indicated number of octets are
       received, the recipient MUST consider the message to be
       incomplete and close the connection.

   6.  If this is a request message and none of the above are true, then
       the message body length is zero (no message body is present).

   7.  Otherwise, this is a response message without a declared message
       body length, so the message body length is determined by the
       number of octets received prior to the server closing the
       connection.

4.1 Chunked Transfer Coding
     chunked-body   = *chunk
                      last-chunk
                      trailer-part
                      CRLF

     chunk          = chunk-size [ chunk-ext ] CRLF
                      chunk-data CRLF
     chunk-size     = 1*HEXDIG
     last-chunk     = 1*("0") [ chunk-ext ] CRLF

     chunk-data     = 1*OCTET ; a sequence of chunk-size octets
4.1.3. Decoding Chunked
   A process for decoding the chunked transfer coding can be represented
   in pseudo-code as:

     length := 0
     read chunk-size, chunk-ext (if any), and CRLF
     while (chunk-size > 0) {
        read chunk-data and CRLF
        append chunk-data to decoded-body
        length := length + chunk-size
        read chunk-size, chunk-ext (if any), and CRLF
     }
     read trailer field
     while (trailer field is not empty) {
        if (trailer field is allowed to be sent in a trailer) {
            append trailer field to existing header fields
        }
        read trailer-field
     }
     Content-Length := length
     Remove "chunked" from Transfer-Encoding
     Remove Trailer from existing header fields
*/

/*

About CONNECT
See CONNECT at https://tools.ietf.org/html/rfc7231#section-4.3.6

request-target = authority-form (rfc 7230 5.3)
authority-form = authority (rfc 7230 5.3.3)
authority = [ userinfo "@" ] host [ ":" port ] (https://tools.ietf.org/html/rfc3986#section-3.2)

*/


/*
 * Error Handling
 *
 * [RETRY LATER]: this indicates the content in a buffer seems to be an incomplete HTTP message
 * 		so please retry parsing later after receiving more data to the buffer
 *
 * [ERROR]: this indicates the content has random message or HTTP message that does NOT obey standards
 *
 * 1. HTTPParserException: [RETRY LATER]
 *
 * 2. HTTPBadMessageException: [ERROR]
 *
 * 2. HTTP400Exception : public HTTPBadMessageException: [ERROR]
 *
 * 3. HTTPBadStatusException : public HTTPBadMessageException : [ERROR]
 *
 * 4. StatusNotCompleteException: [RETRY LATER]
*/

namespace zq29 {
namespace zq29Inner {

	/*
	 * utility functions
	*/
	namespace utils {
		bool isDigit(char c);

		/*
		 * as the name suggests, parse a non-negative
		 * hex number string to an integer
		 * returns -1 on any error
		*/
		int nonNegHexStrToInt(const string& s);
		/*
		 * reverse of the above funnction
		*/
		//string sizeToHexStr(const size_t s);
	}


	class HTTPMessage {
	public:
		/*
		 * some header fields may have the same filed name
		 * but different value, so use set<pair<>>
		*/
		// WHY I DID NOT USE MAP HERE???
		set<pair<string, string>> headerFields;

		string messageBody;
	
		HTTPMessage();
		HTTPMessage(const set<pair<string, string>>& h, const string& m);

		virtual string toStr() const = 0;
	};



	class HTTPRequest : public HTTPMessage {
	public:
		struct RequestLine {
			string method;
			string requestTarget;
			string httpVersion;
			bool operator==(const RequestLine& rhs) const;
			string toStr() const;
		};

		HTTPRequest();
		HTTPRequest(const RequestLine& r, 
			const set<pair<string, string>>& h, const string& m);

		virtual string toStr() const override;
		bool operator==(const HTTPRequest& rhs) const;

		RequestLine requestLine;
	};



	class HTTPStatus : public HTTPMessage {
	public:
		struct StatusLine {
			string httpVersion;
			string statusCode;
			string reasonPhrase;
			bool operator==(const StatusLine& rhs) const;
			string toStr() const;
		};

		HTTPStatus();
		HTTPStatus(const StatusLine& s, 
			const set<pair<string, string>>& h, const string& m);

		virtual string toStr() const override;
		string headerToStr() const;
		bool operator==(const HTTPStatus& rhs) const;

		StatusLine statusLine;
	};



	/*
	 * Parses string to build an HTTP message object 
	 * You should NOT directly use this class,
	 * instead, HTTPRequestParser and HTTPStatusParser should be used
	 *
	 * Input: a reference of vector<char> buffer
	 *		the buffer may have any content
	 * 		- if it begins with a string that cannot be parsed
	 * 		  into an HTTP message, an exception will be thrown,
	 * 		  what's left in the buffer is undefined
	 * 		- if it contains an HTTP message string + anything else,
	 * 		  HTTP message string will be extracted and what's left
	 * 		  is still left in the buffer
	 *
	 * Output: an HTTP message object; the buffer will be modified
	*/
	class HTTPParser {
	protected:
		vector<char> buffer;
		/*
		 * some header fields may have the same filed name
		 * but different value, so use set<pair<>>
		*/
		set<pair<string, string>> headerFields;
		// return headerFields.end() if not found, throw HTTPBadMessageException when multi iters are found
		set<pair<string, string>>::iterator getHeaderFieldByName(const string& name);
		set<pair<string, string>>::iterator getHeaderFieldByName(const string& name) const;
		// count how many header fields are there with "name" as a key
		size_t countHeaderFieldByName(const string& name) const;
		// erase all header fields with key "name", or do nothing if none exists
		void eraseHeaerFieldByName(const string& name);

		string messageBody;

		void __checkLeadingSpaces(stringstream& ss);
		void __checkSkipOneSP(stringstream& ss);
		void __checkEndl(stringstream& ss);

		/*
		 * extract (return and erase from the buffer) a line that
		 * ends with "CR LF" from the buffer
		 * returns a string WITHOUT "CR LF" at the end
		 * if the buffer contains no "CR LF", throw an HTTPParserException
		 * if a single CR is found at then end of the buffer, throw an HTTPParserException
		 * if single CR or LF is found else where, throw an HTTP400Exception
		*/
		string getCRLFLine();

		/*
		 * extract header fields from buffer,
		 * set headerFields
		 * will parse the end of header (CR LF) as well
		*/
		void parseHeaderFields();

		/*
		 * a helper function to parse message body
		 * when the final encoding of header field
		 * "Transfer-Encoding" is "chunked"
		*/
		void parseChunkedMessageBody();
		/*
		 * this function first determins the length of the message body
		 * and then extract and fill the message body with it, with NO check
		 * so it MUST be carefully called after parseHeaderFields
		 *
		 * its implementation depends on HTTP message type
		*/
		virtual void parseMessageBody() = 0;

	public:
		class HTTPBadMessageException : public exception {
		private:
			const string msg;
		public:
			HTTPBadMessageException(const string& msg = "");
			const char* what() const throw() override;
		};

		class HTTPParserException : public exception {
		private:
			const string msg;
		public:
			HTTPParserException(const string& msg = "");
			const char* what() const throw() override;
		};

		/*
		 * before set the buffer, it will also triggers to clear the object
		*/
		virtual void setBuffer(const vector<char>& bufferContent);
		vector<char> getBuffer() const;

		virtual void clear();
	};



	/*
	 * For HTTP request
	*/
	class HTTPRequestParser : public HTTPParser {
	protected: // for testing purpose
		
		HTTPRequest::RequestLine requestLine;

		void parseRequestLine();

		void parseMessageBody() override;

	public:
		class HTTP400Exception : public HTTPBadMessageException {
		public:
			HTTP400Exception(const string& msg);
		};

		virtual void setBuffer(const vector<char>& bufferContent) override;
		virtual void clear() override;
		/*
		 * return an HTTPRequest object that is
		 * built from the buffer, which means,
		 * setBuffer should be called before this method
		 * 
		 * you can call getBuffer to get what's left in the buffer
		 *
		 * on failure, throw exceptions
		*/
		HTTPRequest build();

		/*
		 * static methods help parse member into more fields
		 * e.g. parse requestLine.requestTarget in to authority-form,
		 * 		which is [ userinfo "@" ] host [ ":" port ]
		*/

		struct AuthorityForm {
			string host;
			string port;
		};
		static AuthorityForm parseAuthorityForm(const string& str, bool isConnect);
		static AuthorityForm parseAuthorityForm(const HTTPRequest& req);
		struct AbsoluteForm {
			AuthorityForm authorityForm;
			string path;
		};
		static AbsoluteForm parseAbsoluteForm(const HTTPRequest& req);
	};



	/*
	 * For HTTP status
	 *
	 * !!!*** IMPORTANT ***!!!
	 * 
	 * The following cases will throw StatusNotCompleteException
	 * To handle this exception, read until connection is closed,
	 * put everything in buffer AND CALL METHOD setStatusComplete(true)
	 * THEN redo the parsing, you'll be fine
	 *
	 * 1. Transfer-Encoding (if exists) does NOT have 'chunked'
	 * 2. rule 7 in sections 3.3.3, which means all rules from 1-6 are not satisfied
	*/
	class HTTPStatusParser : public HTTPParser {
	protected: // for testing purpose
		
		HTTPStatus::StatusLine statusLine;

		// see section 3.3.3 rule 2
		bool isRespToCONNECT;

		// see srction 3.3.3 rule 3
		bool isStatusComplete;
		

		void parseStatusLine();

		void parseMessageBody() override;

	public:
		HTTPStatusParser();

		void setRespToCONNECT(bool b);
		void setStatusComplete(bool b);

		class HTTPBadStatusException : public HTTPBadMessageException {
		public:
			HTTPBadStatusException(const string& msg);
		};

		class StatusNotCompleteException : public exception {
		private:
			const string msg;
		public:
			StatusNotCompleteException(const string& msg = "");
			const char* what() const throw() override;
		};

		virtual void setBuffer(const vector<char>& bufferContent) override;
		virtual void clear() override;

		/*
		 * return an HTTPStatus object that is
		 * built from the buffer, which means,
		 * setBuffer should be called before this method
		 * 
		 * you can call getBuffer to get what's left in the buffer
		 *
		 * on failure, throw exceptions
		*/
		HTTPStatus build();
	};


	/*
	 * shortcut functions
	 * 
	 * sc stands for shortcut
	*/
	namespace sc {
		/*
		 * build HTML string from a HTTP 400 Bad Request error
		*/
		string getHTTP400HTMLStr(const string& error);

		string getHTTP502HTMLStr(const string& error);

		/*
		 * hack the status HTML string, add "<h1>zq29 HTTP Cache Proxy</h1>"
		 * if no <body> tag, do nothing
		*/
		string hackStatusHTML(string html);

		/*
		 * works as its name suggests
		 * any exceptions thown will be catched and treated
		 * as failure to build the object, in which case
		 * an HTTPStatus() is returned
		 *
		 * exception guarantee: no throw
		*/
		HTTPStatus buildStatusFromStr(const string& str);
	}







	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// Utils Implementation ///////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	bool utils::isDigit(char c) {
		return (c >= '0' && c <= '9');
	}

	int utils::nonNegHexStrToInt(const string& s) {
		int i = -1;   
		stringstream ss;
		ss << std::hex << s;
		ss >> i;
		string foo;
		if(!ss || ss >> foo || i < 0) { return -1; }
		return i;
	}

	/*
	string utils::sizeToHexStr(const size_t s) {
	}
	*/


	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPMessage Implementation /////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	HTTPMessage::HTTPMessage() {}
	HTTPMessage::HTTPMessage(const set<pair<string, string>>& h, const string& m) : 
		headerFields(h), messageBody(m) {}



	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPRequest Implementation /////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	string HTTPRequest::RequestLine::toStr() const {
		stringstream ss;
		ss << method << " "
			<< requestTarget << " "
			<< httpVersion;
		return ss.str();
	}

	bool HTTPRequest::RequestLine::operator==(const HTTPRequest::RequestLine& rhs) const {
		return (method == rhs.method &&
			requestTarget == rhs.requestTarget &&
			httpVersion == rhs.httpVersion
			);
	}

	HTTPRequest::HTTPRequest() {}
	HTTPRequest::HTTPRequest(const RequestLine& r, 
		const set<pair<string, string>>& h, const string& m) :
		HTTPMessage(h, m), requestLine(r) {}

	string HTTPRequest::toStr() const {
		stringstream ss;
		ss << requestLine.method << " "
			<< requestLine.requestTarget << " "
			<< requestLine.httpVersion << "\r\n";
		for(auto const& e : headerFields) {
			ss << e.first << ": "
				<< e.second << "\r\n";
		}
		ss << "\r\n";
		ss << messageBody;
		return ss.str();
	}

	bool HTTPRequest::operator==(const HTTPRequest& rhs) const {
		return (headerFields == rhs.headerFields &&
			messageBody == rhs.messageBody &&
			requestLine == rhs.requestLine);
	}


	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPStatus Implementation //////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	bool HTTPStatus::StatusLine::operator==(const HTTPStatus::StatusLine& rhs) const {
		return (httpVersion == rhs.httpVersion &&
			statusCode == rhs.statusCode &&
			reasonPhrase == rhs.reasonPhrase);
	}
	string HTTPStatus::StatusLine::toStr() const {
		stringstream ss;
		ss << httpVersion << " "
			<< statusCode << " "
			<< reasonPhrase;
		return ss.str();
	}

	HTTPStatus::HTTPStatus() {}
	HTTPStatus::HTTPStatus(const StatusLine& s, 
		const set<pair<string, string>>& h, const string& m) :
		HTTPMessage(h, m), statusLine(s) {}

	string HTTPStatus::toStr() const {
		stringstream ss;
		ss << statusLine.httpVersion << " "
			<< statusLine.statusCode << " "
			<< statusLine.reasonPhrase << "\r\n";
		for(auto const& e : headerFields) {
			ss << e.first << ": "
				<< e.second << "\r\n";
		}
		ss << "\r\n";
		ss << messageBody;
		return ss.str();
	}

	string HTTPStatus::headerToStr() const {
		stringstream ss;
		ss << statusLine.httpVersion << " "
			<< statusLine.statusCode << " "
			<< statusLine.reasonPhrase << "\r\n";
		for(auto const& e : headerFields) {
			ss << e.first << ": "
				<< e.second << "\r\n";
		}
		ss << "\r\n";
		return ss.str();
	}

	bool HTTPStatus::operator==(const HTTPStatus& rhs) const {
		return (headerFields == rhs.headerFields &&
			messageBody == rhs.messageBody &&
			statusLine == rhs.statusLine);
	}






	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPParser Implementation //////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	// return headerFields.end() if not found
	set<pair<string, string>>::iterator HTTPParser::getHeaderFieldByName(const string& name) {
		auto res = headerFields.end();
		for(auto e = headerFields.begin(); e != headerFields.end(); e++) {
			if((*e).first == name) {
				if(res != headerFields.end()) {
					throw HTTPBadMessageException(Log::msg(
						"multiple header fields with name <", name, ">",
						"were found while calling getHeaderFieldByName"
					));
				}
				res = e;
			}
		}
		return res;
	}
	set<pair<string, string>>::iterator HTTPParser::getHeaderFieldByName(const string& name) const {
		auto res = headerFields.end();
		for(auto e = headerFields.begin(); e != headerFields.end(); e++) {
			if((*e).first == name) {
				if(res != headerFields.end()) {
					throw HTTPBadMessageException(Log::msg(
						"multiple header fields with name <", name, ">",
						"were found while calling getHeaderFieldByName"
					));
				}
				res = e;
			}
		}
		return res;
	}
	size_t HTTPParser::countHeaderFieldByName(const string& name) const {
		size_t count = 0;
		for(auto const& e : headerFields) {
			if(e.first == name) {
				count++;
			}
		}
		return count;
	}
	void HTTPParser::eraseHeaerFieldByName(const string& name) {
		for(auto e = headerFields.begin(); e != headerFields.end(); e++) {
			if((*e).first == name) {
				headerFields.erase(e);
			}
		}
	}

	void HTTPParser::__checkLeadingSpaces(stringstream& ss) {
		// check leading spaces
		if(isspace(ss.peek())) {
			throw HTTPBadMessageException("while parsing an HTTP message, line begins with spaces");
		}
	}

	void HTTPParser::__checkSkipOneSP(stringstream& ss) {
		if(ss.peek() != ' ') {
			throw HTTPBadMessageException(Log::msg(
				"while parsing an HTTP message, expected space(SP), got <", ss.peek(), ">"));	
		}
		ss.ignore();
		if(isspace(ss.peek())) {
			throw HTTPBadMessageException("while parsing an HTTP message, got unexpected space char");
		}
	}

	void HTTPParser::__checkEndl(stringstream& ss) {
		// check ending spaces
		if(isspace(ss.peek())) {
			throw HTTPBadMessageException("while parsing an HTTP message, line ends with spaces");
		}

		// check if there is more content
		if(ss) {
			throw HTTPBadMessageException("while parsing an HTTP message, too much content at the end of the line");
		}
	}

	HTTPParser::HTTPParserException::HTTPParserException(const string& msg) : msg(msg) {} 
	const char* HTTPParser::HTTPParserException::what() const throw() {
		return msg.c_str();
	}
	HTTPParser::HTTPBadMessageException::HTTPBadMessageException(const string& msg) : msg(msg) {} 
	const char* HTTPParser::HTTPBadMessageException::what() const throw() {
		return msg.c_str();
	}

	string HTTPParser::getCRLFLine() {
		if(buffer.size() == 0) {
			throw HTTPParserException("buffer was empty, nothing to get");
		}

		for(size_t i = 0; i < buffer.size(); i++) {
			if(buffer[i] == '\r') {

				// i is the last char
				if(i == buffer.size() - 1) {
					throw HTTPParserException(Log::msg("'\r' was found while parsing <", 
						string(buffer.begin(), buffer.end()), ">"));
				}

				// the next char is not '\n'
				if(buffer[i + 1] != '\n') {
					throw HTTPBadMessageException(Log::msg("'\r' was found while parsing <", 
						string(buffer.begin(), buffer.end()), ">"));
				}

				// this is what we want!
				else {
					string line = "";
					if(i > 0) {
						line = string(buffer.begin(), buffer.begin() + i); // ignore "\n\r"
					}
					buffer.erase(buffer.begin(), buffer.begin() + i + 2); // extract from buffer
					return line;
				}
			} else if(buffer[i] == '\n') {
				throw HTTPBadMessageException(Log::msg("'\n' was found while parsing <", 
					string(buffer.begin(), buffer.end()), ">"));
			}
		}
		
		throw HTTPParserException("No 'CR LF' found in buffer");
	}

	void HTTPParser::parseChunkedMessageBody() {
		stringstream body;
		// mini-function
		auto getChunkSize = [this, &body]()->size_t {
			const string line = getCRLFLine();
			body << line << "\r\n";

			stringstream ss;
			ss << line;
			string chunkSizeStr;
			ss >> chunkSizeStr;
			int chunkSize = utils::nonNegHexStrToInt(chunkSizeStr);
			if(chunkSize == -1) {
				throw HTTPBadMessageException("while parsing chunked message,"\
					" failed to recognize chunk size");
			}
			return (size_t)chunkSize;
		};

		size_t chunkSize = getChunkSize();
		while(chunkSize > 0) {
			// read chunk data
			if(buffer.size() >= chunkSize) {
				body << string(buffer.begin(), buffer.begin() + chunkSize);
				buffer.erase(buffer.begin(), buffer.begin() + chunkSize);
			} else {
				throw HTTPParserException("buffer size < Content-Length");
			}
			// read CR LF
			if(getCRLFLine() != "") { // if buffer is not long enough, exceptions will be thrown
				throw HTTPBadMessageException("while parsing chunked message,"\
					" expected CR LF at the end of the chunk data");
			}
			body << "\r\n";
			chunkSize = getChunkSize();
		}

		// [BROKEN]: we'll ignore the trailer-part
		if(buffer.size() > 0) { // if there is a trailer
			string line;
			while((line = getCRLFLine()) != "") {
				body << line << "\r\n";
			}
		}

		// [BROKEN]: we won't set Content-Length, instead we keep the message as it is
		messageBody = body.str();
	}

	void HTTPParser::setBuffer(const vector<char>& buffer) {
		clear();
		this->buffer = buffer;
	}

	vector<char> HTTPParser::getBuffer() const {
		return buffer;
	}

	void HTTPParser::parseHeaderFields() {
		string line;
		while((line = getCRLFLine()) != "") {
			stringstream ss;
			string temp;
			ss << line;

			__checkLeadingSpaces(ss);

			// find ':'
			size_t colIndex = 0;
			for(; colIndex < line.size(); colIndex++) {
				if(line[colIndex] == ':') {
					break;
				}
			}
			if(colIndex == line.size()) {
				throw HTTPBadMessageException(Log::msg("illegal header-field line <", line, ">"));
			}

			// get field name
			string name(line.begin(), line.begin() + colIndex);
			for(char c : name) {
				if(isspace(c)) {
					throw HTTPBadMessageException("No space allowed in filed name or between filed name and ':'");
				}
			}

			// get filed value
			string value = "";
			// strip OWS at two ends
			size_t begin = colIndex + 1, end = line.size() - 1;
			while(begin <= end && isspace(line[begin])) {
				begin++;
			}
			while(begin <= end && isspace(line[end])) {
				end--;
			}
			if(begin <= end) {
				value = string(line.begin() + begin, line.begin() + end + 1);
			}

			headerFields.insert(make_pair(name, value));
		}

	}

	void HTTPParser::clear() {
		buffer.clear();
		headerFields.clear();
		messageBody = "";
	}









	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPRequestParser Implementation ///////////////////
	/////////////////////////////////////////////////////////////////////////////////
	void HTTPRequestParser::parseRequestLine() {
		string line = getCRLFLine();
		if(line == "") {
			throw HTTP400Exception("request line is empty");
		}

		stringstream ss;
		string temp;
		ss << line;

		__checkLeadingSpaces(ss);

		// get method
		static const set<string> METHODS = { "GET", "POST", "CONNECT" };
		ss >> temp;
		if(METHODS.find(temp) == METHODS.end()) {
			throw HTTP400Exception(Log::msg(
				"request method <", temp, "> not recognized"
			));
		}
		requestLine.method = temp;

		__checkSkipOneSP(ss);

		// get request target
		if(!ss) {
			throw HTTPParserException("request line incomplete");	
		}
		ss >> requestLine.requestTarget; // we'll keep whatever it is

		__checkSkipOneSP(ss);

		// get HTTP version
		if(!ss) {
			throw HTTP400Exception("request line incomplete");
		}
		ss >> temp;
		if(temp.length() != 8 || temp.substr(0, 5) != "HTTP/" || 
			temp[6] != '.' || !utils::isDigit(temp[5]) || !utils::isDigit(temp[7])) {
			throw HTTP400Exception("request HTTP version not recognized");
		}
		requestLine.httpVersion = temp;

		__checkEndl(ss);
	}

	void HTTPRequestParser::parseMessageBody() {
		// rule 3
		// Transfer-Encoding = 1#transfer-coding
		auto const transferEncodingfield = getHeaderFieldByName("Transfer-Encoding");
		if(transferEncodingfield != headerFields.end()) {
			eraseHeaerFieldByName("Content-Length");

			string finalEncoding;
			stringstream ss;
			ss << (*transferEncodingfield).second;
			while(ss >> finalEncoding) {}
			if(finalEncoding == "chunked") {
				parseChunkedMessageBody();
				return;
			} else {
				throw HTTP400Exception("final encoding is NOT chunked for \
					'Transfer-Encoding' for request, close connection");
			}
		}
		// rule 4 & 5
		size_t contentLengthCount = countHeaderFieldByName("Content-Length");
		if(contentLengthCount > 1) {
			throw HTTP400Exception("status contains multiple Content-Length fields");
		} else if(contentLengthCount == 1) {
			// check if length is valid
			stringstream ss;
			const string contentLengthStr = (*getHeaderFieldByName("Content-Length")).second;
			ss << contentLengthStr;
			int contentLength;
			string fool;
			ss >> contentLength;
			if(ss >> fool || contentLength < 0) {
				throw HTTP400Exception(Log::msg(
					"invalid Content-Length field <", contentLengthStr, ">"
				));
			}
			// rule 5
			if(size_t(contentLength) > buffer.size()) {
				throw HTTPParserException(Log::msg(
					"while parsing message body, expected length <", contentLength,
					">, got length <", buffer.size(), "> in buffer"
				));
			}
			if(buffer.size() >= size_t(contentLength)) {
				messageBody = string(buffer.begin(), buffer.begin() + contentLength);
				buffer.erase(buffer.begin(), buffer.begin() + contentLength);
			} else {
				throw HTTPParserException("buffer size < Content-Length");
			}
		}
		// rule 6
		messageBody = "";
	}

	HTTPRequestParser::HTTP400Exception::HTTP400Exception(const string& msg) :
		HTTPBadMessageException(msg) {}

	void HTTPRequestParser::setBuffer(const vector<char>& bufferContent) {
		clear();
		buffer = bufferContent;
	}

	void HTTPRequestParser::clear() {
		HTTPParser::clear();
		requestLine = HTTPRequest::RequestLine();
	}

	HTTPRequest HTTPRequestParser::build() {
		parseRequestLine();
		parseHeaderFields(); // from parent class
		parseMessageBody(); // from parent class
		const HTTPRequest req(requestLine, headerFields, messageBody);
		Log::verbose("Successfully build request:\n" + req.toStr());
		return req;
	}

	HTTPRequestParser::AuthorityForm HTTPRequestParser::parseAuthorityForm(const string& str, bool isConnect) {
		AuthorityForm af;
		const size_t sp = str.find(':');
		if(isConnect && sp == string::npos) {
			throw HTTP400Exception("Bad authority-form in CONNECT: "\
				"a ':' was expected and none found");
		}
		af.host = str.substr(0, sp);
		af.port = (sp == string::npos ? "" : str.substr(sp + 1, string::npos));
		return af;
	}

	HTTPRequestParser::AuthorityForm HTTPRequestParser::parseAuthorityForm(const HTTPRequest& req) {
		const HTTPRequest::RequestLine& line = req.requestLine;
		if(line.method != "CONNECT") {
			throw HTTP400Exception("According to rfc7230 5.3.3: the authority-form "\
				"of request-target is only used for CONNECT requests");
		}
		return parseAuthorityForm(line.requestTarget, true);
	}

	HTTPRequestParser::AbsoluteForm HTTPRequestParser::parseAbsoluteForm(const HTTPRequest& req) {
		const HTTPRequest::RequestLine& line = req.requestLine;
		if(line.method != "GET" && line.method != "POST") {
			throw HTTP400Exception("This program only supports absolute-form for GET & POST");
		}
		if(line.requestTarget.find("http://") != 0) {
			throw HTTP400Exception(Log::msg(
				"Bad request-target: " + line.requestTarget
			));
		}

		AbsoluteForm absoluteForm;
		string rest = line.requestTarget.substr(7, string::npos);
		size_t sp = rest.find('/');
		if(sp != string::npos) { // if there is path
			absoluteForm.path = rest.substr(sp, string::npos);
			absoluteForm.authorityForm = parseAuthorityForm(rest.substr(0, sp), false);
		} else {
			absoluteForm.path = "";
			absoluteForm.authorityForm = parseAuthorityForm(rest, false);
		}
		return absoluteForm;
	}




	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPStatusParser Implementation ////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	HTTPStatusParser::HTTPStatusParser() : 
		isRespToCONNECT(false),
		isStatusComplete(false)
		{}

	void HTTPStatusParser::setRespToCONNECT(bool b) {
		isRespToCONNECT = b;
	}

	void HTTPStatusParser::setStatusComplete(bool b) {
		isStatusComplete = b;
	}

	void HTTPStatusParser::parseStatusLine() {
		string line = getCRLFLine();
		if(line == "") {
			throw HTTPBadStatusException("status line is empty");
		}

		stringstream ss;
		string temp;
		ss << line;

		__checkLeadingSpaces(ss);

		// get http version
		ss >> temp;
		if(temp.length() != 8 || temp.substr(0, 5) != "HTTP/" || 
			temp[6] != '.' || !utils::isDigit(temp[5]) || !utils::isDigit(temp[7])) {
			throw HTTPBadStatusException("status line: HTTP version not recognized");
		}
		statusLine.httpVersion = temp;

		__checkSkipOneSP(ss);

		// get status code
		ss >> temp;
		if(temp.length() != 3 || !utils::isDigit(temp[0]) || 
			!utils::isDigit(temp[1]) || !utils::isDigit(temp[2])) {
			throw HTTPBadStatusException("status line: status code not recognized");
		}
		statusLine.statusCode = temp;

		__checkSkipOneSP(ss);

		// get reason phrase
		getline(ss, statusLine.reasonPhrase);

		__checkEndl(ss);
	}

	void HTTPStatusParser::parseMessageBody() {
		// rule 1
		if(statusLine.statusCode[0] == '1' || statusLine.statusCode == "204" ||
				statusLine.statusCode == "304") {
			messageBody = "";
			return;
		}
		// rule 2
		if(isRespToCONNECT && statusLine.statusCode[0] == '2') {
			messageBody = "";
			return;
		}
		// rule 3
		// Transfer-Encoding = 1#transfer-coding
		auto const transferEncodingfield = getHeaderFieldByName("Transfer-Encoding");
		if(transferEncodingfield != headerFields.end()) {
			eraseHeaerFieldByName("Content-Length");

			string finalEncoding;
			stringstream ss;
			ss << (*transferEncodingfield).second;
			while(ss >> finalEncoding) {}
			if(finalEncoding == "chunked") {
				parseChunkedMessageBody();
				return;
			} else {
				if(!isStatusComplete) {
					throw StatusNotCompleteException(
						"while Transfer-Encoding does NOT have 'chunked',\
						data should be read until connection is closed");
				} else {
					messageBody = string(buffer.begin(), buffer.end());
					buffer.clear();
					return;
				}
			}
		}
		// rule 4 & 5
		size_t contentLengthCount = countHeaderFieldByName("Content-Length");
		if(contentLengthCount > 1) {
			throw HTTPBadStatusException("status contains multiple Content-Length fields");
		} else if(contentLengthCount == 1) {
			// check if length is valid
			stringstream ss;
			const string contentLengthStr = (*getHeaderFieldByName("Content-Length")).second;
			ss << contentLengthStr;
			int contentLength;
			string fool;
			ss >> contentLength;
			if(ss >> fool || contentLength < 0) {
				throw HTTPBadStatusException(Log::msg(
					"invalid Content-Length field <", contentLengthStr, ">"
				));
			}
			// rule 5
			if(size_t(contentLength) > buffer.size()) {
				throw HTTPParserException(Log::msg(
					"while parsing message body, expected length <", contentLength,
					">, got length <", buffer.size(), "> in buffer"
				));
			}
			if(buffer.size() >= size_t(contentLength)) {
				messageBody = string(buffer.begin(), buffer.begin() + contentLength);
				buffer.erase(buffer.begin(), buffer.begin() + contentLength);
			} else {
				throw HTTPParserException("buffer size < Content-Length");
			}
			return;
		}
		// rule 7
		if(!isStatusComplete) {
			throw StatusNotCompleteException(
				"according to rule 7 in section 3.3.3, "\
				"data should be read until connection is closed");
		}
		messageBody = string(buffer.begin(), buffer.end());
		buffer.clear();
	}

	HTTPStatusParser::HTTPBadStatusException::HTTPBadStatusException(const string& msg) :
		HTTPBadMessageException(msg) {}

	HTTPStatusParser::StatusNotCompleteException::StatusNotCompleteException(const string& msg) : msg(msg) {}
	const char* HTTPStatusParser::StatusNotCompleteException::what() const throw() {
		return msg.c_str();
	}

	void HTTPStatusParser::setBuffer(const vector<char>& bufferContent) {
		clear();
		buffer = bufferContent;
	}

	void HTTPStatusParser::clear() {
		HTTPParser::clear();
		statusLine = HTTPStatus::StatusLine();
		isRespToCONNECT = false;
		isStatusComplete = false;
	}

	HTTPStatus HTTPStatusParser::build() {
		parseStatusLine();
		parseHeaderFields(); // from parent class
		parseMessageBody(); // from parent class
		const HTTPStatus result(statusLine, headerFields, messageBody);
		Log::verbose("Successfully build status with header:\n" + result.headerToStr());
		return result;
	}






	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// Shortcuts Implementation ///////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	string sc::getHTTP400HTMLStr(const string& error) {
		const string html = "<!DOCTYPE html PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"\
			"<html><head><meta http-equiv=\"Content-Type\" content=\"text/html\">\n"\
			"<title>400 Bad Request</title>\n</head><body><h1>400 Bad Request</h1>\n<p>" +
			error + "</p>\n<hr><address>zq29 HTTP Cache Proxy</address></body></html>\n";

		HTTPStatus::StatusLine sl;
		sl.httpVersion = "HTTP/1.1";
		sl.statusCode = "400";
		sl.reasonPhrase = "Bad Request";

		set<pair<string, string>> headers;
		stringstream ss;
		ss << html.length();
		headers.insert(make_pair("Content-Length", ss.str()));

		HTTPStatus resp(sl, headers, html);
		return resp.toStr();
	}

	string sc::getHTTP502HTMLStr(const string& error) {
		const string html = "<!DOCTYPE html PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"\
			"<html><head><meta http-equiv=\"Content-Type\" content=\"text/html\">\n"\
			"<title>502 Bad Gateway</title>\n</head><body><h1>502 Bad Gateway</h1>\n<p>" +
			error + "</p>\n<hr><address>zq29 HTTP Cache Proxy</address></body></html>\n";

		HTTPStatus::StatusLine sl;
		sl.httpVersion = "HTTP/1.1";
		sl.statusCode = "502";
		sl.reasonPhrase = "Bad Gateway";

		set<pair<string, string>> headers;
		stringstream ss;
		ss << html.length();
		headers.insert(make_pair("Content-Length", ss.str()));

		HTTPStatus resp(sl, headers, html);
		return resp.toStr();
	}

	string sc::hackStatusHTML(string html) {
		size_t sp = html.find("<body>");
		if(sp == string::npos) { return html; }

		html.insert(sp + 6, "<h1>zq29 HTTP Cache Proxy</h1>");
		cout << "Hacked! proof: " << html.substr(sp, 50);
		return html;
	}

	HTTPStatus sc::buildStatusFromStr(const string& str) {
		try {
			vector<char> buffer(str.begin(), str.end());

			HTTPStatusParser p;
			p.setBuffer(buffer);
			return p.build();
		} catch(const exception& e) {
			Log::debug(e.what());
		}
		return HTTPStatus();
	}

}
	using zq29Inner::HTTPMessage;
	using zq29Inner::HTTPRequest;
	using zq29Inner::HTTPStatus;
	using zq29Inner::HTTPParser;
	using zq29Inner::HTTPRequestParser;
	using zq29Inner::HTTPStatusParser;

	using namespace zq29Inner::utils;
	using namespace zq29Inner::sc;
}

#endif