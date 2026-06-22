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
 * Duyệt mảng pendingRequests, kiểm tra timeout của từng request và xử lý
 * theo strategy đang cấu hình.
 * Trả về mảng TimeoutRecord cấp phát động, ghi số lượng vào *outCount.
 * pendingRequestsCount có thể giảm sau khi gọi (vì phần tử bị xóa).
 * Caller có trách nhiệm gọi free() cho mảng kết quả khi không dùng nữa.
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

