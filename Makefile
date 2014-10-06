CC=g++
CFLAGS=-c -Wall -pedantic -std=c++11 -O3 -fdiagnostics-color
LDFLAGS=-pthread

SOURCES=request_processor.cpp main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=tiny_web_server

.PHONY: clean

all: $(SOURCES) $(EXECUTABLE) depend 

depend: .depend

.depend: $(SOURCES)
	$(CC) $(CFLAGS) -MM $^ > ./.depend;

include .depend

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS) $(EXECUTABLE) .depend
