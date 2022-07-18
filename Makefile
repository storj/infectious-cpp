CXX ?= clang++
LIBTOOL ?= libtool

CXXFLAGS += -Wall -Wextra -Werror -pedantic -std=c++20

CPP_FILES := $(wildcard *.cpp)

.PHONY: all clean

all: libinfectious.a

libinfectious.a: $(CPP_FILES:.cpp=.o)
	$(LIBTOOL) -static -o $@ $^

clean:
	$(RM) *.o libinfectious.a
