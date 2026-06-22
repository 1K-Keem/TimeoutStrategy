#ifndef TIMEOUT_MANAGER_H
#define TIMEOUT_MANAGER_H

#include "Models.h"
#include "DeadlockDetector.h"

#include <stddef.h>

typedef struct {
    int timeout;
    TimeoutStrategy strategy;
    int retryDelay;
    int maxRetries;
    int maxRollbacks;
} TimeoutConfig;

typedef struct {
    TimeoutConfig config_;
} TimeoutManager;

void TimeoutManager_init(TimeoutManager *mgr, TimeoutConfig config);

TimeoutConfig TimeoutManager_getConfig(const TimeoutManager *mgr);

/*
 * Duyet pendingRequests, kiem tra timeout, xu ly theo strategy.
 * Tra ve mang TimeoutRecord cap phat dong, ghi so luong vao *outCount.
 * pendingRequestsCount co the bi giam (phan tu bi xoa trong ham).
 * Caller phai goi free() sau khi dung xong.
 */
TimeoutRecord *TimeoutManager_checkTimeouts(
    TimeoutManager *mgr,
    int currentTime,
    ProcessEntry *processes,
    size_t processesCount,
    ResourceEntry *resources,
    size_t resourcesCount,
    PendingRequest *pendingRequests,
    size_t *pendingRequestsCount,
    DeadlockDetector *detector,
    size_t *outCount);

#endif /* TIMEOUT_MANAGER_H */

