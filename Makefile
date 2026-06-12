CXX = g++
CXXFLAGS = -g -std=c++17 -Wall -Wextra -Iinclude

SRC = \
	src/main.cpp \
	src/CSVParser.cpp \
	src/DeadlockDetector.cpp \
	src/SimulationEngine.cpp \
	src/TimeoutManager.cpp

TARGET = timeout_strategy

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
