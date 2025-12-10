CXXFLAGS=-std=c++23 -O2 -Wall -Wextra

default: playlist_example

playlist_example: playlist_example.cpp
	g++ $(CXXFLAGS) -o $@ $^