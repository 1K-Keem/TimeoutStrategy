CXX = g++
CXXFLAGS = -g -std=c++17 -Wall -Wextra -Iinclude

SRC = \
	src/main.cpp \
	src/CSVParser.cpp \
	src/DeadlockDetector.cpp \
	src/SimulationEngine.cpp \
	src/TimeoutManager.cpp

TARGET = timeout_strategy

ifeq ($(OS),Windows_NT)
	RM = del /f /q
	EXE = $(TARGET).exe
else
	RM = rm -f
	EXE = $(TARGET)
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	$(RM) $(EXE)

rebuild: clean all

.PHONY: all clean rebuild
