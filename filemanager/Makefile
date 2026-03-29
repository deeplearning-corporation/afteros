# Makefile for AfterOS File Manager
CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -static -std=c++11 -O2 -Wall
TARGET = FileManager.elf
SOURCES = FileManager.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) /path/to/AfterOS/bin/

.PHONY: all clean install
