CXX=mpic++
CXXFLAGS= -O3 -std=c++11 -march=native -g -Wall

all: clean benchmark test ddtest

benchmark: benchmark.cpp ../fsgrid.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<
test: test.cpp ../fsgrid.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<
ddtest: ddtest.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	-rm test ddtest benchmark
