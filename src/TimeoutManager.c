#include "../include/TimeoutManager.hpp"
#include <algorithm>
#include <stdexcept>

TimeoutManager::TimeoutManager(TimeoutConfig config) : config_(config) {
  if (config_.timeout < 1) {
    throw invalid_argument("TIMEOUT must be >= 1");
  }
}

const TimeoutConfig &TimeoutManager::config() const { return config_; }

vector<TimeoutRecord> TimeoutManager::checkTimeouts(
    int currentTime, map<string, Process> &processes,
    map<string, Resource> &resources,
    vector<PendingRequest> &pendingRequests, DeadlockDetector &detector) {
  vector<TimeoutRecord> records;
  for (size_t index = 0; index < pendingRequests.size();) {
    auto request = pendingRequests[index];
    auto &process = processes.at(request.processId);
    if (process.state != ProcessState::Blocked) {
      ++index;
      continue;
    }

    int waitingTime = currentTime - request.requestTime;
    if (waitingTime < config_.timeout) {
      ++index;
      continue;
    }

    // True positive = process nay thuc su nam trong chu trinh cho (deadlock).
    // Chi check rieng process bi timeout, khong dung cycle toan cuc -> tranh
    // tinh nham false positive cho process cho lau ma khong deadlock.
    bool deadlocked = detector.isInDeadlock(request.processId);

    if (config_.strategy == TimeoutStrategy::Kill) {
      records.push_back(killProcess(currentTime, process, request, waitingTime,
                                    deadlocked, resources, pendingRequests));
      index = 0;
    } else if (config_.strategy == TimeoutStrategy::Rollback) {
      records.push_back(rollbackProcess(currentTime, process, request,
                                        waitingTime, deadlocked, resources,
                                        pendingRequests));
      // rollback (hoac leo thang kill) xoa moi pending cua process -> duyet lai
      index = 0;
    } else {
      auto record = retryRequest(currentTime, process, request, waitingTime,
                                 deadlocked, resources, pendingRequests, index);
      records.push_back(record);
      if (record.killed) {
        index = 0;
      }
    }
  }
  return records;
}

TimeoutRecord
TimeoutManager::killProcess(int currentTime, Process &process,
                            const PendingRequest &request, int waitingTime,
                            bool deadlocked,
                            map<string, Resource> &resources,
                            vector<PendingRequest> &pendingRequests) {
  process.state = ProcessState::Terminated;
  process.requestTime.reset();
  process.waitingFor.reset();

  for (const auto &resourceId : process.heldResources) {
    auto &resource = resources.at(resourceId);
    resource.owner.reset();
    resource.releaseTime.reset();
  }
  process.heldResources.clear();

  pendingRequests.erase(remove_if(pendingRequests.begin(),
                                       pendingRequests.end(),
                                       [&](const PendingRequest &item) {
                                         return item.processId == process.id;
                                       }),
                        pendingRequests.end());

  return TimeoutRecord{currentTime,
                       process.id,
                       request.resourceId,
                       waitingTime,
                       config_.strategy,
                       deadlocked,
                       true,
                       false,
                       false,
                       !deadlocked};
}

TimeoutRecord TimeoutManager::retryRequest(
    int currentTime, Process &process, PendingRequest request, int waitingTime,
    bool deadlocked, map<string, Resource> &resources,
    vector<PendingRequest> &pendingRequests, size_t requestIndex) {
  pendingRequests.erase(pendingRequests.begin() +
                        static_cast<long long>(requestIndex));
  request.retryCount += 1;
  process.requestTime.reset();
  process.waitingFor.reset();

  if (request.retryCount > config_.maxRetries) {
    return killProcess(currentTime, process, request, waitingTime, deadlocked,
                       resources, pendingRequests);
  }

  process.state = ProcessState::Running;
  process.retryAfter = currentTime + config_.retryDelay;
  request.requestTime = process.retryAfter;
  pendingRequests.push_back(request);
  return TimeoutRecord{currentTime,
                       process.id,
                       request.resourceId,
                       waitingTime,
                       config_.strategy,
                       deadlocked,
                       false,
                       true,
                       false,
                       !deadlocked};
}

TimeoutRecord TimeoutManager::rollbackProcess(
    int currentTime, Process &process, const PendingRequest &request,
    int waitingTime, bool deadlocked,
    map<string, Resource> &resources,
    vector<PendingRequest> &pendingRequests) {
  process.rollbackCount += 1;

  // Vuot nguong rollback -> leo thang sang kill de tranh livelock.
  if (process.rollbackCount > config_.maxRollbacks) {
    return killProcess(currentTime, process, request, waitingTime, deadlocked,
                       resources, pendingRequests);
  }

  // Thu hoi toan bo tai nguyen process dang giu.
  for (const auto &resourceId : process.heldResources) {
    auto &resource = resources.at(resourceId);
    resource.owner.reset();
    resource.releaseTime.reset();
  }
  process.heldResources.clear();

  // Xoa moi pending cua process (engine se re-inject lai tu dau).
  pendingRequests.erase(remove_if(pendingRequests.begin(),
                                       pendingRequests.end(),
                                       [&](const PendingRequest &item) {
                                         return item.processId == process.id;
                                       }),
                        pendingRequests.end());

  // Tra process ve trang thai ban dau (chay lai tu dau).
  process.state = ProcessState::New;
  process.requestTime.reset();
  process.waitingFor.reset();

  return TimeoutRecord{currentTime,
                       process.id,
                       request.resourceId,
                       waitingTime,
                       config_.strategy,
                       deadlocked,
                       false,
                       false,
                       true,
                       !deadlocked};
}
