# Makefile for EZLogger native tests (macOS/Linux/Windows)

# Detect OS
ifeq ($(OS),Windows_NT)
	DETECTED_OS := Windows
else
	DETECTED_OS := $(shell uname)
endif

# Compiler (use g++ on Windows for MinGW/MSYS2)
ifeq ($(DETECTED_OS),Windows)
	CXX := g++
else ifeq ($(DETECTED_OS),Darwin)
	CXX := clang++
else
	CXX := g++
endif

# Compiler flags
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -g
CXXFLAGS += -DLOG_ENABLE_CONSOLE=1 -DLOG_ENABLE_JSON_MSGBUF=1 -DLOG_ENABLE_TEXT_MSGBUF=0 -DLOG_GLOBAL_LEVEL=4
INCLUDES := -Imock -I../../src -I../../examples/advanced_logger

# Platform-specific linker flags
ifeq ($(DETECTED_OS),Windows)
	LDFLAGS :=
	EXE_EXT := .exe
else
	LDFLAGS := -lpthread
	EXE_EXT :=
endif

# macOS: use libc++
ifeq ($(DETECTED_OS),Darwin)
	CXXFLAGS += -stdlib=libc++
endif

TARGET := ezlog_test$(EXE_EXT)
SOURCES := test_main.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ Build complete: ./$(TARGET)"

%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET)

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) ezlog_test ezlog_test.exe
	@echo "✓ Clean complete"

# Helper targets
help:
	@echo "EZLogger Test Suite - Build Commands:"
	@echo "  make          - Build the test executable"
	@echo "  make run      - Build and run tests"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make help     - Show this help message"
