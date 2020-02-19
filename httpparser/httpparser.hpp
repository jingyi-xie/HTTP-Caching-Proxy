#ifndef ZQ29_HTTPPARSER
#define ZQ29_HTTPPARSER

#include <set>
#include <tuple>
#include <vector>
#include <cctype>
#include <sstream>

#include "../log.hpp"

/*

Search BROKEN to find places where this program breaks the HTTP standard

*/

/*

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

*/

namespace zq29 {
namespace zq29Inner {

	bool isDigit(char c);

	class HTTPMessage {
	public:
		

	protected:

	};

	class HTTPRequest : public HTTPMessage {
	public:
		struct RequestLine {
			string method;
			string requestTarget;
			string httpVersion;
		};
	private:
		RequestLine requestLine;
	};

	class HTTPStatus : public HTTPMessage {
	public:
		struct StatusLine {
			string httpVersion;
			string statusCode;
			string reasonPhrase;
		};
	private:
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

		void __checkLeadingSpaces(stringstream& ss);
		void __checkSkipOneSP(stringstream& ss);
		void __checkEndl(stringstream& ss);

		/*
		 * extract (return and erase from the buffer) a line that
		 * ends with "CR LF" from the buffer
		 * returns a string WITHOUT "CR LF" at the end
		 * if the buffer contains no "CR LF", throw a CRLFException
		 * if only CR or only LF is found rather than CR LF,
		 * throw a CRLFException
		*/
		string getCRLFLine();
		class CRLFException {
		private:
			const char* msg;
		public:
			CRLFException(const char* msg = "");
			const char* what() const;
		};

		class HTTPParserException {
		private:
			const char* msg;
		public:
			HTTPParserException(const char* msg = "");
			const char* what() const;
		};

		/*
		 * extract header fields from buffer,
		 * set headerFields
		 * will parse the end of header (CR LF) as well
		*/
		void parseHeaderFields();

	public:
		/*
		 * before set the buffer, it will also triggers to clear the object
		*/
		void setBuffer(const vector<char>& bufferContent);
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
	};


	/*
	 * 
	*/
	class HTTPStatusParser : public HTTPParser {
	protected: // for testing purpose
		
		HTTPStatus::StatusLine statusLine;

		void parseStatusLine();
	};





	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// Utils Implementation ///////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	bool isDigit(char c) {
		return (c >= '0' && c <= '9');
	}


	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPMessage Implementation /////////////////////////
	/////////////////////////////////////////////////////////////////////////////////




	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPRequest Implementation /////////////////////////
	/////////////////////////////////////////////////////////////////////////////////



	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPStatus Implementation //////////////////////////
	/////////////////////////////////////////////////////////////////////////////////









	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPParser Implementation //////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	void HTTPParser::__checkLeadingSpaces(stringstream& ss) {
		// check leading spaces
		if(isspace(ss.peek())) {
			throw HTTPParserException("while parsing an HTTP message, line begins with spaces");
		}
	}

	void HTTPParser::__checkSkipOneSP(stringstream& ss) {
		if(ss.peek() != ' ') {
			throw HTTPParserException(Log::msg(
				"while parsing an HTTP message, expected space(SP), got <", ss.peek(), ">").c_str());	
		}
		ss.ignore();
		if(isspace(ss.peek())) {
			throw HTTPParserException("while parsing an HTTP message, got unexpected space char");
		}
	}

	void HTTPParser::__checkEndl(stringstream& ss) {
		// check ending spaces
		if(isspace(ss.peek())) {
			throw HTTPParserException("while parsing an HTTP message, line ends with spaces");
		}

		// check if there is more content
		if(ss) {
			throw HTTPParserException("while parsing an HTTP message, too much content at the end of the line");
		}
	}

	HTTPParser::CRLFException::CRLFException(const char* msg) {
		this->msg = msg;
	} 
	const char* HTTPParser::CRLFException::what() const {
		return msg;
	}
	HTTPParser::HTTPParserException::HTTPParserException(const char* msg) {
		this->msg = msg;
	} 
	const char* HTTPParser::HTTPParserException::what() const {
		return msg;
	}

	string HTTPParser::getCRLFLine() {
		if(buffer.size() == 0) {
			throw CRLFException("buffer was empty, nothing to get");
		}

		for(size_t i = 0; i < buffer.size(); i++) {
			if(buffer[i] == '\r') {

				// i is the last char, or the next char is not '\n'
				if(i == buffer.size() - 1 || buffer[i + 1] != '\n') {
					throw CRLFException(Log::msg("'\r' was found while parsing <", 
						string(buffer.begin(), buffer.end()), ">").c_str());
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
				throw CRLFException(Log::msg("'\n' was found while parsing <", 
					string(buffer.begin(), buffer.end()), ">").c_str());
			}
		}
		
		throw CRLFException("No 'CR LF' found in buffer");
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
				throw HTTPParserException(Log::msg("illegal header-field line <", line, ">").c_str());
			}

			// get field name
			string name(line.begin(), line.begin() + colIndex);
			for(char c : name) {
				if(isspace(c)) {
					throw HTTPParserException("No space allowed in filed name or between filed name and ':'");
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
	}









	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPRequestParser Implementation ///////////////////
	/////////////////////////////////////////////////////////////////////////////////
	void HTTPRequestParser::parseRequestLine() {
		string line = getCRLFLine();
		if(line == "") {
			throw HTTPParserException("request line is empty");
		}

		stringstream ss;
		string temp;
		ss << line;

		__checkLeadingSpaces(ss);

		// get method
		static const set<string> METHODS = { "GET", "POST", "CONNECT" };
		ss >> temp;
		if(METHODS.find(temp) == METHODS.end()) {
			throw HTTPParserException("request method not recognized");
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
			throw HTTPParserException("request line incomplete");
		}
		ss >> temp;
		if(temp.length() != 8 || temp.substr(0, 5) != "HTTP/" || 
			temp[6] != '.' || !isDigit(temp[5]) || !isDigit(temp[7])) {
			throw HTTPParserException("request HTTP version not recognized");
		}
		requestLine.httpVersion = temp;

		__checkEndl(ss);
	}






	/////////////////////////////////////////////////////////////////////////////////
	//////////////////////////// HTTPStatusParser Implementation ////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	void HTTPStatusParser::parseStatusLine() {
		string line = getCRLFLine();
		if(line == "") {
			throw HTTPParserException("status line is empty");
		}

		stringstream ss;
		string temp;
		ss << line;

		__checkLeadingSpaces(ss);

		// get http version
		ss >> temp;
		if(temp.length() != 8 || temp.substr(0, 5) != "HTTP/" || 
			temp[6] != '.' || !isDigit(temp[5]) || !isDigit(temp[7])) {
			throw HTTPParserException("status line: HTTP version not recognized");
		}
		statusLine.httpVersion = temp;

		__checkSkipOneSP(ss);

		// get status code
		ss >> temp;
		if(temp.length() != 3 || !isDigit(temp[0]) || 
			!isDigit(temp[1]) || !isDigit(temp[2])) {
			throw HTTPParserException("status line: status code not recognized");
		}
		statusLine.statusCode = temp;

		__checkSkipOneSP(ss);

		// get reason phrase
		getline(ss, statusLine.reasonPhrase);

		__checkEndl(ss);
	}




}
	using zq29Inner::HTTPMessage;
	using zq29Inner::HTTPRequest;
	using zq29Inner::HTTPStatus;
	using zq29Inner::HTTPParser;
	using zq29Inner::HTTPRequestParser;
	using zq29Inner::HTTPStatusParser;
}

#endif