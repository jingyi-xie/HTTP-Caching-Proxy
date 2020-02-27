CPPFLAGS=-Wall -Werror -pedantic -std=c++17 -g -DNDEBUG
COMMON=log.hpp

main: main.cpp
	g++-9 $(CPPFLAGS) main.cpp -o main -lpthread

tests: httpparserTest cacheTest proxy_main

proxy_main: socket/proxy_main.cpp socket/proxy.hpp
	g++-9 $(CPPFLAGS) socket/proxy_main.cpp -o proxy_main -lpthread

httpparserTest: httpparser/httpparserTest.cpp httpparser/httpparser.hpp $(COMMON)
	g++-9 $(CPPFLAGS) httpparser/httpparserTest.cpp -o httpparserTest

cacheTest: cache/cacheTest.cpp cache/cache.hpp $(COMMON)
	g++-9 $(CPPFLAGS) cache/cacheTest.cpp -o cacheTest

clean:
	rm main httpparserTest cacheTest proxy_main
