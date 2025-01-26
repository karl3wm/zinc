CXX=g++-14
CXXFLAGS=-std=c++26 -fcoroutines -ggdb -O0 -Wall -Wextra -ftrapv -fsanitize=address -fsanitize=leak -fsanitize=undefined -pedantic
LDLIBS=-lssl -lcrypto -lboost_url
LDFLAGS=$(CXXFLAGS)
CC=${CXX}

test_http: test_http.o http_beast.o

openai_client: openai_client.hpp
	$(CXX) $(CXXFLAGS) $< -o $@
