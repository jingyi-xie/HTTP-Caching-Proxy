CPPFLAGS=-Wall -Werror -pedantic -std=c++17 -g
COMMON=log.hpp

tests: httpparserTest cacheTest proxy_main

proxy_main: socket/proxy_main.cpp socket/proxy.hpp
	g++-9.1 $(CPPFLAGS) socket/proxy_main.cpp -o proxy_main -lpthread

httpparserTest: httpparser/httpparserTest.cpp httpparser/httpparser.hpp $(COMMON)
	g++-9.1 $(CPPFLAGS) httpparser/httpparserTest.cpp -o httpparserTest

cacheTest: cache/cacheTest.cpp cache/cache.hpp $(COMMON)
	g++-9.1 $(CPPFLAGS) cache/cacheTest.cpp -o cacheTest

clean:
	rm httpparserTest cacheTest proxy_main