CPPFLAGS=-Wall -Werror -pedantic -std=c++11 -g
LIBS=log.hpp


tests: httpparserTest

httpparserTest: httpparser/httpparserTest.cpp httpparser/httpparser.hpp $(LIBS)
	g++ $(CPPFLAGS) httpparser/httpparserTest.cpp -o httpparserTest

clean:
	rm httpparserTest