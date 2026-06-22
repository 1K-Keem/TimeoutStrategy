CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude

CORE = \
	src/CSVParser.c \
	src/DeadlockDetector.c \
	src/SimulationEngine.c \
	src/TimeoutManager.c

SRC      = src/main.c $(CORE)
TEST_SRC = tests/test_timeout.c $(CORE)

ifeq ($(OS),Windows_NT)
	RM       = del /f /q
	EXE      = timeout_strategy.exe
	TEST_EXE = run_tests.exe
	RUN      = .\
else
	RM       = rm -f
	EXE      = timeout_strategy
	TEST_EXE = run_tests
	RUN      = ./
endif

all: $(EXE)

$(EXE): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(EXE)

$(TEST_EXE): $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_EXE)

test: $(TEST_EXE)
	$(RUN)$(TEST_EXE)

clean:
	$(RM) $(EXE) $(TEST_EXE)

rebuild: clean all

.PHONY: all test clean rebuild
