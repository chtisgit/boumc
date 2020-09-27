
SHELL ?= /bin/sh
CXX ?= g++
RM ?= rm -f

# asan
# CFLAGS = -fsanitize=address
# LFLAGS = -fsanitize=address

MINISAT := MiniSat-p_v1.14

CFLAGS := -g -Wall -O3 $(CFLAGS)
CXXFLAGS := -std=c++14 $(CXXFLAGS)
LFLAGS := -g $(LFLAGS)

LIBS := $(MINISAT)/libminisat.a $(LIBS)

all: boumc

$(MINISAT)/libminisat.a:
	cd $(MINISAT) && $(MAKE) libminisat.a

boumc: aag.o main.o translate.o $(LIBS)
	$(CXX) $(LFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

test:
	find tests -name '*.sh' | xargs -n1 sh

clean:
	$(RM) *.o
	cd $(MINISAT) && $(MAKE) clean

.PHONY: all clean tests
