#pragma once

#include "Models.hpp"

#include <map>
#include <vector>

struct TimeoutConfig {
    int timeout{5};
    TimeoutStrategy strategy{TimeoutStrategy::Kill};
    int retryDelay{1};
    int maxRetries{3};
};

class TimeoutManager {
public:
    explicit TimeoutManager(TimeoutConfig config);

    std::vector<TimeoutRecord> checkTimeouts(
        int currentTime,
        std::map<std::string, Process>& processes,
        std::map<std::string, Resource>& resources,
        std::vector<PendingRequest>& pendingRequests
    );

    const TimeoutConfig& config() const;

private:
    TimeoutConfig config_;

    TimeoutRecord killProcess(
        int currentTime,
        Process& process,
        const PendingRequest& request,
        int waitingTime,
        bool deadlocked,
        std::map<std::string, Resource>& resources,
        std::vector<PendingRequest>& pendingRequests
    );

    TimeoutRecord retryRequest(
        int currentTime,
        Process& process,
        PendingRequest request,
        int waitingTime,
        bool deadlocked,
        std::vector<PendingRequest>& pendingRequests,
        std::size_t requestIndex
    );
};
