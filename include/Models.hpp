#pragma once

#include <optional>
#include <set>
#include <string>

using namespace std;

enum class ProcessState {
    New,
    Running,
    Blocked,
    Completed,
    Terminated,
};

enum class TimeoutStrategy {
    Kill,
    Retry,
    Rollback,
};

struct Event {
    int time{};
    std::string processId;
    std::string action;
    std::string resourceId;
    int duration{};
};

struct PendingRequest {
    std::string processId;
    std::string resourceId;
    int requestTime{};
    int duration{};
    int retryCount{};
};

struct Process {
    std::string id;
    ProcessState state{ProcessState::New};
    std::set<std::string> heldResources;
    std::optional<int> requestTime;
    std::optional<std::string> waitingFor;
    int retryAfter{};
    int rollbackCount{};
    bool completionCounted{};

    bool isAlive() const {
        return state != ProcessState::Completed && state != ProcessState::Terminated;
    }
};

struct Resource {
    std::string id;
    std::optional<std::string> owner;
    std::optional<int> releaseTime;

    bool isFree() const {
        return !owner.has_value();
    }
};

struct TimeoutRecord {
    int time{};
    std::string processId;
    std::string resourceId;
    int waitingTime{};
    TimeoutStrategy strategy{TimeoutStrategy::Kill};
    bool deadlockedAtTimeout{};
    bool killed{};
    bool retried{};
    bool rolledBack{};
    bool falsePositive{};
};

struct SimulationMetrics {
    int killedProcesses{};
    int timeoutEvents{};
    int retryEvents{};
    int rollbackEvents{};
    int deadlockResolved{};
    int completedProcesses{};
    int totalProcesses{};
    int falsePositives{};

    double throughput() const {
        return totalProcesses == 0 ? 0.0 : static_cast<double>(completedProcesses) / totalProcesses;
    }

    double falsePositiveRate() const {
        return timeoutEvents == 0 ? 0.0 : static_cast<double>(falsePositives) / timeoutEvents;
    }
};

