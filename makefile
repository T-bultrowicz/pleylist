CXXFLAGS=-std=c++23 -O2 -Wall -Wextra
TESTOWANIE=playlist_tests1 playlist_tests2 playlist_tests3

default: playlist_example

playlist_example: playlist_example.cpp
	g++ $(CXXFLAGS) -o $@ $^

testy: $(TESTOWANIE)
%: %.cpp
	g++ $(CXXFLAGS) -o $@ $<