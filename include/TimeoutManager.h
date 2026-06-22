#pragma once
#include "../include/DeadlockDetector.hpp"
#include "Models.hpp"

#include <map>
#include <vector>

struct TimeoutConfig {
  int timeout{5};
  TimeoutStrategy strategy{TimeoutStrategy::Kill};
  int retryDelay{1};
  int maxRetries{3};
  int maxRollbacks{3};
};

class TimeoutManager {
public:
  explicit TimeoutManager(TimeoutConfig config);

  vector<TimeoutRecord>
  checkTimeouts(int currentTime, map<string, Process> &processes,
                map<string, Resource> &resources,
                vector<PendingRequest> &pendingRequests,
                DeadlockDetector &detector);

  const TimeoutConfig &config() const;

private:
  TimeoutConfig config_;

  TimeoutRecord killProcess(int currentTime, Process &process,
                            const PendingRequest &request, int waitingTime,
                            bool deadlocked,
                            map<string, Resource> &resources,
                            vector<PendingRequest> &pendingRequests);

  TimeoutRecord retryRequest(int currentTime, Process &process,
                             PendingRequest request, int waitingTime,
                             bool deadlocked,
                             map<string, Resource> &resources,
                             vector<PendingRequest> &pendingRequests,
                             size_t requestIndex);

  TimeoutRecord rollbackProcess(int currentTime, Process &process,
                                const PendingRequest &request, int waitingTime,
                                bool deadlocked,
                                map<string, Resource> &resources,
                                vector<PendingRequest> &pendingRequests);
};

