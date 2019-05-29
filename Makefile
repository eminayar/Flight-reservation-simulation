CXXFLAGS=-std=c++11 -lpthread

code: code.cpp
	g++ code.cpp -o code $(CXXFLAGS)