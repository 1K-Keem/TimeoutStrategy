#include "../include/TimeoutManager.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Helper                                                             */
/* ------------------------------------------------------------------ */


static Process *findProcess(ProcessEntry *processes, size_t count, const char *processId)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(processes[i].key, processId) == 0)
            return &processes[i].value;
    }
    return NULL;
}

static Resource *findResource(ResourceEntry *resources, size_t count, const char *resourceId)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(resources[i].key, resourceId) == 0)
            return &resources[i].value;
    }
    return NULL;
}

static void pushRecord(TimeoutRecord **arr, size_t *count, size_t *cap, TimeoutRecord rec)
{
    if (*count >= *cap) {
        size_t newCap = (*cap == 0) ? 8 : (*cap) * 2;
        *arr = realloc(*arr, newCap * sizeof(TimeoutRecord));
        *cap = newCap;
    }
    (*arr)[*count] = rec;
    (*count)++;
}

/* Xóa tất cả pending request của một process khỏi mảng (dồn in-place, free chuỗi). */
static void removePendingByProcess(
    PendingRequest *pending, size_t *count, const char *processId)
{
    size_t keep = 0;
    size_t i;
    for (i = 0; i < *count; i++) {
        if (strcmp(pending[i].processId, processId) == 0) {
            free(pending[i].processId);
            free(pending[i].resourceId);
        } else {
            if (keep != i)
                pending[keep] = pending[i];
            keep++;
        }
    }
    *count = keep;
}

/* Xóa 1 pending request tại vị trí index, free chuỗi rồi dồn các phần tử sau lên. */
static void removePendingAt(PendingRequest *pending, size_t *count, size_t index)
{
    free(pending[index].processId);
    free(pending[index].resourceId);
    size_t i;
    for (i = index + 1; i < *count; i++)
        pending[i - 1] = pending[i];
    (*count)--;
}

/* Giải phóng toàn bộ tài nguyên mà process đang giữ và reset owner trong resource map. */
static void releaseHeldResources(
    Process *process, ResourceEntry *resources, size_t resourcesCount)
{
    size_t i;
    for (i = 0; i < process->heldResourcesCount; i++) {
        Resource *res = findResource(resources, resourcesCount, process->heldResources[i]);
        if (res) {
            free(res->owner);
            res->owner = NULL;
            res->has_releaseTime = false;
        }
        free(process->heldResources[i]);
    }
    process->heldResourcesCount = 0;
}

/* Tạo TimeoutRecord, processId và resourceId được strdup (caller có nhiệm vụ free). */
static TimeoutRecord makeRecord(
    int currentTime, const char *processId, const char *resourceId,
    int waitingTime, TimeoutStrategy strategy, bool deadlocked,
    bool killed, bool retried, bool rolledBack)
{
    TimeoutRecord rec;
    rec.time               = currentTime;
    rec.processId          = strdup(processId);
    rec.resourceId         = strdup(resourceId);
    rec.waitingTime        = waitingTime;
    rec.strategy           = strategy;
    rec.deadlockedAtTimeout = deadlocked;
    rec.killed             = killed;
    rec.retried            = retried;
    rec.rolledBack         = rolledBack;
    rec.falsePositive      = !deadlocked;
    return rec;
}

/* ------------------------------------------------------------------ */
/* Các hàm xử lý chính                                                  */
/* ------------------------------------------------------------------ */

static TimeoutRecord doKillProcess(
    TimeoutManager *mgr,
    int currentTime,
    Process *process,
    const char *resourceId,   /* lưu trước khi dọn pending */
    int waitingTime,
    bool deadlocked,
    ResourceEntry *resources,
    size_t resourcesCount,
    PendingRequest *pendingRequests,
    size_t *pendingCount)
{
    process->state = PROCESS_STATE_TERMINATED;
    process->has_requestTime = false;
    free(process->waitingFor);
    process->waitingFor = NULL;

    releaseHeldResources(process, resources, resourcesCount);
    removePendingByProcess(pendingRequests, pendingCount, process->id);

    return makeRecord(currentTime, process->id, resourceId, waitingTime,
                      mgr->config_.strategy, deadlocked,
                      true, false, false);
}

static TimeoutRecord doRetryRequest(
    TimeoutManager *mgr,
    int currentTime,
    Process *process,
    PendingRequest request,   /* truyền bằng giá trị (copy) */
    int waitingTime,
    bool deadlocked,
    ResourceEntry *resources,
    size_t resourcesCount,
    PendingRequest *pendingRequests,
    size_t *pendingCount,
    size_t requestIndex)
{
    /* Lưu trước các chuỗi cần dùng, vì removePendingAt sẽ free chuỗi gốc. */
    char *savedProcessId  = strdup(request.processId);
    char *savedResourceId = strdup(request.resourceId);
    int   savedRetryCount = request.retryCount + 1;

    /* Gỡ pending hiện tại (free chuỗi gốc bên trong). */
    removePendingAt(pendingRequests, pendingCount, requestIndex);

    process->has_requestTime = false;
    free(process->waitingFor);
    process->waitingFor = NULL;

    if (savedRetryCount > mgr->config_.maxRetries) {
        /* Vượt giới hạn retry -> leo thang kill để chống livelock. */
        TimeoutRecord rec = doKillProcess(
            mgr, currentTime, process, savedResourceId, waitingTime,
            deadlocked, resources, resourcesCount, pendingRequests, pendingCount);
        free(savedProcessId);
        free(savedResourceId);
        return rec;
    }

    process->state      = PROCESS_STATE_RUNNING;
    process->retryAfter = currentTime + mgr->config_.retryDelay;

    /* Đẩy lại pending với thông tin mới: requestTime = retryAfter,
       hai chuỗi processId/resourceId là heap (do caller free sau). */
    PendingRequest newReq;
    newReq.processId  = savedProcessId;
    newReq.resourceId = savedResourceId;
    newReq.requestTime = process->retryAfter;
    newReq.duration    = request.duration;
    newReq.retryCount  = savedRetryCount;
    pendingRequests[*pendingCount] = newReq;
    (*pendingCount)++;

    TimeoutRecord rec = makeRecord(currentTime, process->id, savedResourceId, waitingTime,
                                   mgr->config_.strategy, deadlocked,
                                   false, true, false);
    return rec;
}

static TimeoutRecord doRollbackProcess(
    TimeoutManager *mgr,
    int currentTime,
    Process *process,
    const char *resourceId,   /* lưu trước khi dọn pending */
    int waitingTime,
    bool deadlocked,
    ResourceEntry *resources,
    size_t resourcesCount,
    PendingRequest *pendingRequests,
    size_t *pendingCount)
{
    process->rollbackCount += 1;

    /* Vượt ngưỡng maxRollbacks -> leo thang kill để tránh livelock. */
    if (process->rollbackCount > mgr->config_.maxRollbacks) {
        return doKillProcess(
            mgr, currentTime, process, resourceId, waitingTime,
            deadlocked, resources, resourcesCount, pendingRequests, pendingCount);
    }

    /* Thu hồi toàn bộ tài nguyên process đang giữ. */
    releaseHeldResources(process, resources, resourcesCount);

    /* Xóa mọi pending của process này. */
    removePendingByProcess(pendingRequests, pendingCount, process->id);

    /* Đưa process về trạng thái ban đầu. */
    process->state = PROCESS_STATE_NEW;
    process->has_requestTime = false;
    free(process->waitingFor);
    process->waitingFor = NULL;

    return makeRecord(currentTime, process->id, resourceId, waitingTime,
                      mgr->config_.strategy, deadlocked,
                      false, false, true);
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

void TimeoutManager_init(TimeoutManager *mgr, TimeoutConfig config)
{
    if (config.timeout < 1) {
        fprintf(stderr, "TIMEOUT must be >= 1\n");
        exit(1);
    }
    mgr->config_ = config;
}

TimeoutConfig TimeoutManager_getConfig(const TimeoutManager *mgr)
{
    return mgr->config_;
}

TimeoutRecord *TimeoutManager_checkTimeouts(
    TimeoutManager *mgr,
    int currentTime,
    ProcessEntry *processes,
    size_t processesCount,
    ResourceEntry *resources,
    size_t resourcesCount,
    PendingRequest *pendingRequests,
    size_t *pendingRequestsCountPtr,
    DeadlockDetector *detector,
    size_t *outCount)
{
    TimeoutRecord *records = NULL;
    size_t count = 0;
    size_t cap   = 0;

    size_t pendingCount = *pendingRequestsCountPtr;

    size_t index = 0;
    while (index < pendingCount) {
        PendingRequest *req = &pendingRequests[index];

        Process *process = findProcess(processes, processesCount, req->processId);
        if (!process || process->state != PROCESS_STATE_BLOCKED) {
            index++;
            continue;
        }

        int waitingTime = currentTime - req->requestTime;
        if (waitingTime < mgr->config_.timeout) {
            index++;
            continue;
        }

        /* Lưu resourceId trước, vì các hàm xử lý bên dưới có thể free
           chuỗi gốc nằm trong pending. */
        char *savedResourceId = strdup(req->resourceId);

        bool deadlocked = DeadlockDetector_isInDeadlock(detector, req->processId);

        TimeoutRecord rec;
        if (mgr->config_.strategy == TIMEOUT_STRATEGY_KILL) {
            rec = doKillProcess(
                mgr, currentTime, process, savedResourceId, waitingTime,
                deadlocked, resources, resourcesCount,
                pendingRequests, &pendingCount);
            free(savedResourceId);
            pushRecord(&records, &count, &cap, rec);
            index = 0;

        } else if (mgr->config_.strategy == TIMEOUT_STRATEGY_ROLLBACK) {
            rec = doRollbackProcess(
                mgr, currentTime, process, savedResourceId, waitingTime,
                deadlocked, resources, resourcesCount,
                pendingRequests, &pendingCount);
            free(savedResourceId);
            pushRecord(&records, &count, &cap, rec);
            index = 0;

        } else { /* TIMEOUT_STRATEGY_RETRY */
            PendingRequest reqCopy = *req;
            rec = doRetryRequest(
                mgr, currentTime, process, reqCopy, waitingTime,
                deadlocked, resources, resourcesCount,
                pendingRequests, &pendingCount, index);
            free(savedResourceId);
            pushRecord(&records, &count, &cap, rec);
            if (rec.killed)
                index = 0;
        }
    }

    *pendingRequestsCountPtr = pendingCount;
    *outCount = count;
    return records;
}


