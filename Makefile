CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

CORE = \
	src/CSVParser.cpp \
	src/DeadlockDetector.cpp \
	src/SimulationEngine.cpp \
	src/TimeoutManager.cpp

SRC = src/main.cpp $(CORE)
TEST_SRC = tests/test_main.cpp $(CORE)

ifeq ($(OS),Windows_NT)
	RM = del /f /q
	EXE = timeout_strategy.exe
	TEST_EXE = run_tests.exe
	RUN = .\\
else
	RM = rm -f
	EXE = timeout_strategy
	TEST_EXE = run_tests
	RUN = ./
endif

all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(EXE)

$(TEST_EXE): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_EXE)

test: $(TEST_EXE)
	$(RUN)$(TEST_EXE)

clean:
	$(RM) $(EXE) $(TEST_EXE)

rebuild: clean all

.PHONY: all test clean rebuild
