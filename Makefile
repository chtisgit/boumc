
SHELL ?= /bin/sh
CXX ?= clang++
RM ?= rm -f

# asan
# CFLAGS = -fsanitize=address
# LFLAGS = -fsanitize=address

CFLAGS := -g -Wall -O3 $(CFLAGS)
CXXFLAGS := -std=c++11 $(CXXFLAGS)
LFLAGS := $(LFLAGS)

LIBS := MiniSat-p_v1.14/Solver.o MiniSat-p_v1.14/Proof.o MiniSat-p_v1.14/File.o $(LIBS)

all: boumc

MiniSat-p_v1.14/Solver.o:
	cd MiniSat-p_v1.14 && $(MAKE) Solver.o

MiniSat-p_v1.14/Proof.o:
	cd MiniSat-p_v1.14 && $(MAKE) Proof.o

MiniSat-p_v1.14/File.o:
	cd MiniSat-p_v1.14 && $(MAKE) File.o

boumc: aag.o main.o formula.o $(LIBS)
	$(CXX) $(LFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

test:
	find tests -name '*.sh' | xargs -n1 sh

clean:
	$(RM) *.o
	cd MiniSat-p_v1.14 && $(MAKE) clean

.PHONY: all clean tests
