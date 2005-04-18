CC=g++
CFLAGS=-O3 -g0
SOURCES=mipsdec.cpp function.cpp symbols.cpp instruction.cpp register.cpp optimize.cpp codegen.cpp
OBJECTS=$(SOURCES:.cpp=.o)
VPATH=..

all: $(SOURCES) mipsdec

mipsdec: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@
