#include "../include/CSVParser.h"
#include "../include/Models.h"
#include "../include/SimulationEngine.h"
#include "../include/TimeoutManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] [max_retries|max_rollbacks]\n"
        "  timeout      TIMEOUT threshold (integer >= 1, default 5)\n"
        "  strategy     kill | retry | rollback (default kill)\n"
        "  retry_delay  number of time units to wait before retrying (default 1)\n"
        "  max_*        max retries/rollbacks before escalating to kill (default 3)\n"
        "  -v|--verbose log every event per time unit\n"
        "  -c|--compare run all 3 strategies (kill/retry/rollback) and print comparison table\n",
        prog);
}

static int parsePositiveInt(const char *text, const char *fieldName)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (*end != '\0' || end == text || value < 1) {
        fprintf(stderr, "Invalid value for %s: %s\n", fieldName, text);
        exit(1);
    }
    return (int)value;
}

static const char *strategyName(TimeoutStrategy strategy)
{
    switch (strategy) {
    case TIMEOUT_STRATEGY_KILL:     return "kill";
    case TIMEOUT_STRATEGY_RETRY:    return "retry";
    case TIMEOUT_STRATEGY_ROLLBACK: return "rollback";
    }
    return "unknown";
}

static void printMetrics(const TimeoutConfig *config, const char *datasetPath,
                         const SimulationMetrics *metrics)
{
    printf("=== Config ===\n");
    printf("dataset       : %s\n", datasetPath);
    printf("timeout       : %d\n", config->timeout);
    printf("strategy      : %s\n", strategyName(config->strategy));
    printf("retry_delay   : %d\n", config->retryDelay);
    printf("max_retries   : %d\n", config->maxRetries);
    printf("max_rollbacks : %d\n\n", config->maxRollbacks);

    printf("=== Metrics ===\n");
    printf("total_processes     : %d\n", metrics->totalProcesses);
    printf("completed_processes : %d\n", metrics->completedProcesses);
    printf("killed_processes    : %d\n", metrics->killedProcesses);
    printf("timeout_events      : %d\n", metrics->timeoutEvents);
    printf("retry_events        : %d\n", metrics->retryEvents);
    printf("rollback_events     : %d\n", metrics->rollbackEvents);
    printf("deadlock_resolved   : %d\n", metrics->deadlockResolved);
    printf("false_positives     : %d\n", metrics->falsePositives);
    printf("throughput          : %.3f\n", SimulationMetrics_throughput(metrics));
    printf("false_positive_rate : %.3f\n", SimulationMetrics_falsePositiveRate(metrics));
}

static void printCompareTable(const TimeoutConfig *base, const char *datasetPath,
                              Event *events, size_t eventsCount)
{
    const TimeoutStrategy strategies[] = {
        TIMEOUT_STRATEGY_KILL, TIMEOUT_STRATEGY_RETRY, TIMEOUT_STRATEGY_ROLLBACK
    };

    printf("=== Strategy comparison ===\n");
    printf("dataset : %s\n", datasetPath);
    printf("timeout : %d\n\n", base->timeout);

    printf("%-10s %10s %8s %8s %10s %10s %8s %12s %10s\n",
           "strategy", "completed", "killed", "retries",
           "rollbacks", "resolved", "fp", "throughput", "fp_rate");
    printf("--------------------------------------------------------------------------------------\n");

    int i;
    for (i = 0; i < 3; i++) {
        TimeoutConfig config = *base;
        config.strategy = strategies[i];
        SimulationEngine *engine = SimulationEngine_create(&config, false);
        SimulationMetrics m = SimulationEngine_run(engine, events, eventsCount);
        SimulationEngine_destroy(engine);

        printf("%-10s %10d %8d %8d %10d %10d %8d %12.3f %10.3f\n",
               strategyName(strategies[i]),
               m.completedProcesses, m.killedProcesses,
               m.retryEvents, m.rollbackEvents, m.deadlockResolved,
               m.falsePositives,
               SimulationMetrics_throughput(&m),
               SimulationMetrics_falsePositiveRate(&m));
    }
}

int main(int argc, char **argv)
{
    int verbose = 0;
    int compare = 0;

    /* Mảng lưu tham số vị trí (không phải cờ -v/-c). */
    const char *pos[8];
    int posCount = 0;

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compare") == 0) {
            compare = 1;
        } else if (posCount < 8) {
            pos[posCount++] = argv[i];
        }
    }

    if (posCount == 0) {
        printUsage(argv[0]);
        return 1;
    }

    const char *datasetPath = pos[0];

    TimeoutConfig config;
    config.timeout      = 5;
    config.strategy     = TIMEOUT_STRATEGY_KILL;
    config.retryDelay   = 1;
    config.maxRetries   = 3;
    config.maxRollbacks = 3;

    if (posCount >= 2)
        config.timeout = parsePositiveInt(pos[1], "timeout");

    if (posCount >= 3) {
        if (strcmp(pos[2], "kill") == 0) {
            config.strategy = TIMEOUT_STRATEGY_KILL;
        } else if (strcmp(pos[2], "retry") == 0) {
            config.strategy = TIMEOUT_STRATEGY_RETRY;
        } else if (strcmp(pos[2], "rollback") == 0) {
            config.strategy = TIMEOUT_STRATEGY_ROLLBACK;
        } else {
            fprintf(stderr, "Strategy must be 'kill', 'retry' or 'rollback': %s\n", pos[2]);
            printUsage(argv[0]);
            return 1;
        }
    }

    if (posCount >= 4)
        config.retryDelay = parsePositiveInt(pos[3], "retry_delay");

    if (posCount >= 5) {
        config.maxRetries   = parsePositiveInt(pos[4], "max_retries|max_rollbacks");
        config.maxRollbacks = config.maxRetries;
    }

    size_t eventsCount = 0;
    Event *events = CSVParser_parse(datasetPath, &eventsCount);
    if (!events) {
        fprintf(stderr, "Error: cannot read file %s\n", datasetPath);
        return 1;
    }

    if (compare) {
        printCompareTable(&config, datasetPath, events, eventsCount);
        free(events);
        return 0;
    }

    SimulationEngine *engine = SimulationEngine_create(&config, verbose);
    if (verbose)
        printf("=== Event log ===\n");

    SimulationMetrics metrics = SimulationEngine_run(engine, events, eventsCount);

    if (verbose)
        printf("\n");

    printMetrics(&config, datasetPath, &metrics);

    SimulationEngine_destroy(engine);
    free(events);
    return 0;
}
