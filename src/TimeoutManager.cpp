#include "TimeoutManager.hpp"

#include <algorithm>
#include <stdexcept>

TimeoutManager::TimeoutManager(TimeoutConfig config) : config_(config) {
    if (config_.timeout < 1) {
        throw std::invalid_argument("TIMEOUT must be >= 1");
    }
}

const TimeoutConfig& TimeoutManager::config() const {
    return config_;
}

std::vector<TimeoutRecord> TimeoutManager::checkTimeouts(
    int currentTime,
    std::map<std::string, Process>& processes,
    std::map<std::string, Resource>& resources,
    std::vector<PendingRequest>& pendingRequests
) {
    std::vector<TimeoutRecord> records;
    for (std::size_t index = 0; index < pendingRequests.size();) {
        auto request = pendingRequests[index];
        auto& process = processes.at(request.processId);
        if (process.state != ProcessState::Blocked) {
            ++index;
            continue;
        }

        int waitingTime = currentTime - request.requestTime;
        if (waitingTime < config_.timeout) {
            ++index;
            continue;
        }

        // TODO(Khanh): thay gia tri nay bang ket qua DeadlockDetector/Wait-For Graph.
        // Phan Kim chi xu ly timeout; detector la phan rieng cua Khanh.
        bool deadlocked = false;
        if (config_.strategy == TimeoutStrategy::Kill) {
            records.push_back(killProcess(currentTime, process, request, waitingTime, deadlocked, resources, pendingRequests));
            index = 0;
        } else {
            auto record = retryRequest(currentTime, process, request, waitingTime, deadlocked, resources, pendingRequests, index);
            records.push_back(record);
            if (record.killed) {
                index = 0;
            }
        }
    }
    return records;
}

TimeoutRecord TimeoutManager::killProcess(
    int currentTime,
    Process& process,
    const PendingRequest& request,
    int waitingTime,
    bool deadlocked,
    std::map<std::string, Resource>& resources,
    std::vector<PendingRequest>& pendingRequests
) {
    process.state = ProcessState::Terminated;
    process.requestTime.reset();
    process.waitingFor.reset();

    for (const auto& resourceId : process.heldResources) {
        auto& resource = resources.at(resourceId);
        resource.owner.reset();
        resource.releaseTime.reset();
    }
    process.heldResources.clear();

    pendingRequests.erase(
        std::remove_if(pendingRequests.begin(), pendingRequests.end(), [&](const PendingRequest& item) {
            return item.processId == process.id;
        }),
        pendingRequests.end()
    );

    return TimeoutRecord{currentTime, process.id, request.resourceId, waitingTime, config_.strategy, deadlocked, true, false, !deadlocked};
}

TimeoutRecord TimeoutManager::retryRequest(
    int currentTime,
    Process& process,
    PendingRequest request,
    int waitingTime,
    bool deadlocked,
    std::map<std::string, Resource>& resources,
    std::vector<PendingRequest>& pendingRequests,
    std::size_t requestIndex
) {
    pendingRequests.erase(pendingRequests.begin() + static_cast<long long>(requestIndex));
    request.retryCount += 1;
    process.requestTime.reset();
    process.waitingFor.reset();

    if (request.retryCount > config_.maxRetries) {
        return killProcess(currentTime, process, request, waitingTime, deadlocked, resources, pendingRequests);
    }

    process.state = ProcessState::Running;
    process.retryAfter = currentTime + config_.retryDelay;
    request.requestTime = process.retryAfter;
    pendingRequests.push_back(request);
    return TimeoutRecord{currentTime, process.id, request.resourceId, waitingTime, config_.strategy, deadlocked, false, true, !deadlocked};
}
