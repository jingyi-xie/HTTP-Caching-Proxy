CPPFLAGS=-Wall -Werror -pedantic -std=c++17 -g
COMMON=log.hpp


tests: httpparserTest cacheTest

httpparserTest: httpparser/httpparserTest.cpp httpparser/httpparser.hpp $(COMMON)
	g++-9.1 $(CPPFLAGS) httpparser/httpparserTest.cpp -o httpparserTest

cacheTest: cacheTest.cpp cache.hpp $(COMMON)
	g++-9.1 $(CPPFLAGS) cacheTest.cpp -o cacheTest

clean:
	rm httpparserTest cacheTest