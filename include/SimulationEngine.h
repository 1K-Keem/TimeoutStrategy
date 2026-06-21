#pragma once

#include "../include/Models.h"
#include "../include/TimeoutManager.h"

#include <stdbool.h>
#include <stddef.h>

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

typedef struct {
    Event* events;
    size_t count;
    size_t capacity;
} EventVector;

typedef struct {
    char* key;
    EventVector value;
} EventVectorEntry;

typedef struct SimulationEngine {
    TimeoutManager timeoutManager_;
    DeadlockDetector deadlockDetector_;
    SimulationMetrics metrics_;
    bool verbose_;

    ProcessEntry* processes_;
    size_t processesCount_;
    size_t processesCapacity_;

    ResourceEntry* resources_;
    size_t resourcesCount_;
    size_t resourcesCapacity_;

    PendingRequest* pendingRequests_;
    size_t pendingRequestsCount_;
    size_t pendingRequestsCapacity_;

    IntEntry* remainingEventCount_;
    size_t remainingEventCountCount_;
    size_t remainingEventCountCapacity_;

    EventVectorEntry* processEvents_;
    size_t processEventsCount_;
    size_t processEventsCapacity_;
} SimulationEngine;

SimulationEngine* SimulationEngine_create(const TimeoutConfig* config, bool verbose);
void SimulationEngine_destroy(SimulationEngine* engine);
SimulationMetrics SimulationEngine_run(SimulationEngine* engine, const Event* events, size_t eventsCount);
void SimulationEngine_log(const SimulationEngine* engine, int currentTime, const char* message);
void SimulationEngine_resetState(SimulationEngine* engine);
void SimulationEngine_registerEventSources(SimulationEngine* engine, const Event* events, size_t eventsCount);
void SimulationEngine_ensureProcessExists(SimulationEngine* engine, const char* processId);
Resource* SimulationEngine_ensureResourceExists(SimulationEngine* engine, const char* resourceId);
void SimulationEngine_processEventsAt(SimulationEngine* engine, int currentTime, const Event* events, size_t eventsCount, size_t* nextEventIndex);
void SimulationEngine_releaseExpiredResources(SimulationEngine* engine, int currentTime);
void SimulationEngine_grantPendingRequests(SimulationEngine* engine, int currentTime);
void SimulationEngine_applyTimeouts(SimulationEngine* engine, int currentTime);
void SimulationEngine_allocateResource(SimulationEngine* engine, Process* process, PendingRequest* request, int currentTime);
void SimulationEngine_replayProcess(SimulationEngine* engine, const char* processId, int currentTime);
void SimulationEngine_completeProcess(SimulationEngine* engine, Process* process, int currentTime);
void SimulationEngine_releaseResource(SimulationEngine* engine, const char* resourceId);
void SimulationEngine_checkAndCompleteProcesses(SimulationEngine* engine, int currentTime);
void SimulationEngine_blockProcess(SimulationEngine* engine, Process* process, const Event* event, int currentTime);
bool SimulationEngine_hasFutureRelease(const SimulationEngine* engine, int currentTime);