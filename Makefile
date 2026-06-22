CC = gcc
CFLAGS ?= -std=c17 -Wall -Wextra
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS ?=
LDLIBS ?=
PYTHON ?= python3
VENV ?= .venv

CORE = \
	src/CSVParser.c \
	src/DeadlockDetector.c \
	src/SimulationEngine.c \
	src/TimeoutManager.c

SRC       = src/main.c $(CORE)
TEST_SRC  = tests/test_timeout.c $(CORE)
EXE       = timeout_strategy
TEST_EXE  = run_tests
RUN_ARGS ?= data/three_process_deadlock.csv 3 kill

all: $(EXE)
	@echo "Built ./$(EXE)"

help:
	@echo "Targets:"
	@echo "  make                         Build ./$(EXE)"
	@echo "  make run RUN_ARGS=\"...\"      Build and run the simulator"
	@echo "  make deps                    Create .venv and install Python deps"
	@echo "  make benchmark               Build and run benchmark"
	@echo "  make test                    Build and run tests"
	@echo "  make clean                   Remove generated binaries"

$(EXE): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) -o $(EXE) $(LDFLAGS) $(LDLIBS)

$(TEST_EXE): $(TEST_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TEST_SRC) -o $(TEST_EXE) $(LDFLAGS) $(LDLIBS)

run: $(EXE)
	./$(EXE) $(RUN_ARGS)

deps:
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/python -m pip install --upgrade pip
	$(VENV)/bin/python -m pip install -r requirements.txt

benchmark: $(EXE)
	bash run.sh benchmark

test: $(TEST_EXE)
	./$(TEST_EXE)

clean:
	rm -f $(EXE) $(TEST_EXE) timeout_strategy.exe run_tests.exe

rebuild: clean all

.PHONY: all help run deps benchmark test clean rebuild
