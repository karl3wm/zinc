CXX=g++-14
CXXFLAGS=-std=c++26 -ggdb
LDLIBS=-lssl -lcrypto -lboost_url
LDFLAGS=-ggdb
CC=${CXX}

test_http: test_http.o http_beast.o

openai_client: openai_client.hpp
	$(CXX) $(CXXFLAGS) $< -o $@
