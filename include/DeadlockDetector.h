// Wait-For Graph + DFS phat hien chu trinh (deadlock).
#ifndef DEADLOCK_DETECTOR_H
#define DEADLOCK_DETECTOR_H

#include <stdbool.h>

typedef struct {
    int *data;
    int size;
    int capacity;
} IntSet;

typedef struct {
    char **pidList;
    int pidCount;
    int pidCapacity;

    IntSet *adj;
    int vertexCount;
} WFGraph;


/* WFGraph */

void WFGraph_init(WFGraph *graph);
void WFGraph_clear(WFGraph *graph);

int WFGraph_getOrCreatePid(
    WFGraph *graph,
    const char *pid);

int WFGraph_findPid(
    WFGraph *graph,
    const char *pid);

void WFGraph_addEdge(
    WFGraph *graph,
    const char *waitPid,
    const char *holdPid);

void WFGraph_removeEdge(
    WFGraph *graph,
    const char *waitPid,
    const char *holdPid);

void WFGraph_removeOutgoingEdge(
    WFGraph *graph,
    const char *pid);

void WFGraph_removePid(
    WFGraph *graph,
    const char *pid);

bool WFGraph_deadlockDetection(
    WFGraph *graph);

bool WFGraph_pidInCycle(
    WFGraph *graph,
    const char *pid);

/* DeadlockDetector */
typedef struct {
    WFGraph graph;
} DeadlockDetector;

void DeadlockDetector_init(
    DeadlockDetector *detector);

void DeadlockDetector_destroy(
    DeadlockDetector *detector);

void DeadlockDetector_addWaitRelation(
    DeadlockDetector *detector,
    const char *waitingPid,
    const char *holdingPid);

void DeadlockDetector_removeWaitRelation(
    DeadlockDetector *detector,
    const char *waitingPid,
    const char *holdingPid);

void DeadlockDetector_removeWaitingProcess(
    DeadlockDetector *detector,
    const char *pid);

void DeadlockDetector_removeProcess(
    DeadlockDetector *detector,
    const char *pid);

void DeadlockDetector_clear(
    DeadlockDetector *detector);

bool DeadlockDetector_detectDeadlock(
    DeadlockDetector *detector);

bool DeadlockDetector_isInDeadlock(
    DeadlockDetector *detector,
    const char *pid);

#endif
