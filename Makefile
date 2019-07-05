
SHELL ?= /bin/sh
CXX ?= g++
RM ?= rm -f

CFLAGS := -g -Wall -O3 $(CFLAGS)
CXXFLAGS := -std=c++11 $(CXXFLAGS)

all: boumc

boumc: aag.o main.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

tests:
	find tests -name '*.sh' | xargs -n1 sh

clean:
	$(RM) *.o

.PHONY: all clean tests