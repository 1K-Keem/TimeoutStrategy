#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Models.h — định nghĩa các kiểu dữ liệu dùng chung cho toàn bộ mô phỏng:
 * Process, Resource, Event, PendingRequest, TimeoutRecord, SimulationMetrics
 * cùng các trạng thái và chiến lược timeout. Vì code là C thuần nên không
 * có constructor/method, mọi thao tác đều qua hàm tự do.
 */

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_COMPLETED,
    PROCESS_STATE_TERMINATED
} ProcessState;

typedef enum {
    TIMEOUT_STRATEGY_KILL,
    TIMEOUT_STRATEGY_RETRY,
    TIMEOUT_STRATEGY_ROLLBACK
} TimeoutStrategy;

typedef struct {
    int time;
    char* processId;
    char* action;
    char* resourceId;
    int duration;
} Event;

typedef struct {
    char* processId;
    char* resourceId;
    int requestTime;
    int duration;
    int retryCount;
} PendingRequest;

typedef struct {
    char* id;
    ProcessState state;

    char** heldResources;
    size_t heldResourcesCount;
    size_t heldResourcesCapacity;

    int requestTime;
    bool has_requestTime;

    char* waitingFor;

    int retryAfter;
    int rollbackCount;
    bool completionCounted;
} Process;

static inline bool Process_isAlive(const Process* p) {
    return p->state != PROCESS_STATE_COMPLETED && p->state != PROCESS_STATE_TERMINATED;
}

typedef struct {
    char* id;

    char* owner;

    int releaseTime;
    bool has_releaseTime;
} Resource;

static inline bool Resource_isFree(const Resource* r) {
    return r->owner == NULL;
}

typedef struct {
    int time;
    char* processId;
    char* resourceId;
    int waitingTime;
    TimeoutStrategy strategy;
    bool deadlockedAtTimeout;
    bool killed;
    bool retried;
    bool rolledBack;
    bool falsePositive;
} TimeoutRecord;

typedef struct {
    int killedProcesses;
    int timeoutEvents;
    int retryEvents;
    int rollbackEvents;
    int deadlockResolved;
    int completedProcesses;
    int totalProcesses;
    int falsePositives;
} SimulationMetrics;

static inline double SimulationMetrics_throughput(const SimulationMetrics* m) {
    return m->totalProcesses == 0 ? 0.0 : (double)(m->completedProcesses) / m->totalProcesses;
}

static inline double SimulationMetrics_falsePositiveRate(const SimulationMetrics* m) {
    return m->timeoutEvents == 0 ? 0.0 : (double)(m->falsePositives) / m->timeoutEvents;
}

/* Các kiểu entry dùng chung giữa SimulationEngine và TimeoutManager
 * (mô phỏng cặp key-value như std::map<string, T>). */
typedef struct {
    char* key;
    Process value;
} ProcessEntry;

typedef struct {
    char* key;
    Resource value;
} ResourceEntry;

typedef struct {
    char* key;
    int value;
} IntEntry;
