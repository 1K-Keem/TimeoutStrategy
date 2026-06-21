#include "../include/DeadlockDetector.h"
#include <stdlib.h>
#include <string.h>


// set 
static char *str_dup(const char *s)
{
    char *p = (char *)malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}

static void IntSet_init(IntSet *s)
{
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

static void IntSet_insert(IntSet *s, int value)
{
    for (int i = 0; i < s->size; i++)
        if (s->data[i] == value) return;

    if (s->size >= s->capacity)
    {
        s->capacity = s->capacity ? s->capacity * 2 : 4;
        s->data = (int *)realloc(s->data, s->capacity * sizeof(int));
    }

    s->data[s->size++] = value;
}

static void IntSet_erase(IntSet *s, int value)
{
    for (int i = 0; i < s->size; i++)
    {
        if (s->data[i] == value)
        {
            s->data[i] = s->data[s->size - 1];
            s->size--;
            return;
        }
    }
}

static void IntSet_clear(IntSet *s) { s->size = 0; }

static void IntSet_destroy(IntSet *s)
{
    free(s->data);
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}




// WGraph
void WFGraph_init(WFGraph *graph)
{
    graph->pidList = NULL;
    graph->pidCount = 0;
    graph->pidCapacity = 10;
    graph->adj = NULL;
    graph->vertexCount = 0;
}

int WFGraph_getOrCreatePid(WFGraph *graph, const char *pid)
{
    for (int i = 0; i < graph->pidCount; i++)
        if (strcmp(graph->pidList[i], pid) == 0) return i;

    if (graph->pidCount >= graph->pidCapacity)
    {
        int newCap = graph->pidCapacity ? graph->pidCapacity * 2 : 4;

        graph->pidList = (char **)realloc(graph->pidList, newCap * sizeof(char *));

        graph->adj = (IntSet *)realloc(graph->adj, newCap * sizeof(IntSet));

        graph->pidCapacity = newCap;
    }

    int idx = graph->pidCount++;
    graph->pidList[idx] = str_dup(pid);
    IntSet_init(&graph->adj[idx]);
    graph->vertexCount = graph->pidCount;

    return idx;
}


int WFGraph_findPid(
    WFGraph *graph,
    const char *pid)
{
    for(int i = 0; i < graph->pidCount; i++)
    {
        if(strcmp(graph->pidList[i], pid) == 0)
            return i;
    }

    return -1;
}

void WFGraph_addEdge(WFGraph *graph, const char *waitPid, const char *holdPid)
{
    int from = WFGraph_getOrCreatePid(graph, waitPid);
    int to = WFGraph_getOrCreatePid(graph, holdPid);
    IntSet_insert(&graph->adj[from], to);
}

void WFGraph_removeEdge(
    WFGraph *graph,
    const char *wait,
    const char *hold)
{
    int from = WFGraph_findPid(graph, wait);
    int to   = WFGraph_findPid(graph, hold);

    if(from == -1 || to == -1)
        return;

    IntSet_erase(&graph->adj[from], to);
}

void WFGraph_removeOutgoingEdge(WFGraph *graph, const char *pid)
{
    for (int i = 0; i < graph->pidCount; i++)
    {
        if (strcmp(graph->pidList[i], pid) == 0)
        {
            IntSet_clear(&graph->adj[i]);
            return;
        }
    }
}

void WFGraph_removePid(WFGraph *graph, const char *pid)
{
    int idx = -1;

    for (int i = 0; i < graph->pidCount; i++)
        if (strcmp(graph->pidList[i], pid) == 0)
        {
            idx = i;
            break;
        }

    if (idx < 0) return;

    IntSet_clear(&graph->adj[idx]);

    for (int i = 0; i < graph->pidCount; i++)
        for (int j = 0; j < graph->adj[i].size;)
            if (graph->adj[i].data[j] == idx)
                IntSet_erase(&graph->adj[i], idx);
            else
                j++;
}

static bool dfs(WFGraph *graph, int idx, bool *visited, bool *inStack)
{
    visited[idx] = true;
    inStack[idx] = true;

    for (int i = 0; i < graph->adj[idx].size; i++)
    {
        int next = graph->adj[idx].data[i];

        if (!visited[next])
        {
            if (dfs(graph, next, visited, inStack))
                return true;
        }
        else if (inStack[next])
            return true;
    }

    inStack[idx] = false;
    return false;
}

bool WFGraph_deadlockDetection(WFGraph *graph)
{
    int n = graph->vertexCount;

    bool *visited = (bool *)calloc(n, sizeof(bool));
    bool *inStack = (bool *)calloc(n, sizeof(bool));

    bool found = false;

    for (int i = 0; i < n; i++)
        if (!visited[i] && dfs(graph, i, visited, inStack))
        {
            found = true;
            break;
        }

    free(visited);
    free(inStack);

    return found;
}

static bool dfsReach(WFGraph *graph, int cur, int target, bool *visited)
{
    if (cur == target) return true;

    visited[cur] = true;

    for (int i = 0; i < graph->adj[cur].size; i++)
    {
        int next = graph->adj[cur].data[i];

        if (next == target) return true;

        if (!visited[next] &&
            dfsReach(graph, next, target, visited))
            return true;
    }

    return false;
}

bool WFGraph_pidInCycle(WFGraph *graph, const char *pid)
{
    int idx = -1;

    for (int i = 0; i < graph->pidCount; i++)
        if (strcmp(graph->pidList[i], pid) == 0)
        {
            idx = i;
            break;
        }

    if (idx < 0) return false;

    for (int i = 0; i < graph->adj[idx].size; i++)
    {
        bool *visited =
            (bool *)calloc(graph->vertexCount, sizeof(bool));

        bool found =
            dfsReach(graph, graph->adj[idx].data[i], idx, visited);

        free(visited);

        if (found) return true;
    }

    return false;
}

void WFGraph_clear(WFGraph *graph)
{
    for (int i = 0; i < graph->pidCount; i++)
    {
        free(graph->pidList[i]);
        IntSet_destroy(&graph->adj[i]);
    }

    free(graph->pidList);
    free(graph->adj);

    WFGraph_init(graph);
}

//DeadlockDetector
void DeadlockDetector_init(DeadlockDetector *detector)
{
    WFGraph_init(&detector->graph);
}

void DeadlockDetector_destroy(DeadlockDetector *detector)
{
    WFGraph_clear(&detector->graph);
}

void DeadlockDetector_addWaitRelation(
    DeadlockDetector *detector,
    const char *waitingPid,
    const char *holdingPid)
{
    WFGraph_addEdge(&detector->graph, waitingPid, holdingPid);
}

void DeadlockDetector_removeWaitRelation(
    DeadlockDetector *detector,
    const char *waitingPid,
    const char *holdingPid)
{
    WFGraph_removeEdge(&detector->graph, waitingPid, holdingPid);
}

void DeadlockDetector_removeWaitingProcess(
    DeadlockDetector *detector,
    const char *pid)
{
    WFGraph_removeOutgoingEdge(&detector->graph, pid);
}

void DeadlockDetector_removeProcess(
    DeadlockDetector *detector,
    const char *pid)
{
    WFGraph_removePid(&detector->graph, pid);
}

void DeadlockDetector_clear(DeadlockDetector *detector)
{
    WFGraph_clear(&detector->graph);
}

bool DeadlockDetector_detectDeadlock(DeadlockDetector *detector)
{
    return WFGraph_deadlockDetection(&detector->graph);
}

bool DeadlockDetector_isInDeadlock(
    DeadlockDetector *detector,
    const char *pid)
{
    return WFGraph_pidInCycle(&detector->graph, pid);
}
