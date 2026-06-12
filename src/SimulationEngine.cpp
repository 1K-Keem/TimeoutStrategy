#include "../include/SimulationEngine.hpp"

#include <algorithm>

SimulationEngine::SimulationEngine(const TimeoutConfig &config)
    : timeoutManager_(config) {}

void SimulationEngine::resetState() {
  processes_.clear();
  resources_.clear();
  pendingRequests_.clear();
  remainingEventCount_.clear();
  metrics_ = SimulationMetrics{};

  // build WFGraph
  deadlockDetector_.clear();
}

void SimulationEngine::registerEventSources(const vector<Event> &events) {
  for (const auto &event : events) {
    ++remainingEventCount_[event.processId];
    ensureProcessExists(event.processId);
  }
}

// Thêm Process vào Map nếu chưa tồn tại và cập nhật tổng số process
void SimulationEngine::ensureProcessExists(const string &processId) {
  if (processes_.find(processId) == processes_.end()) {
    processes_.emplace(processId, Process{processId});
    ++metrics_.totalProcesses;
  }
}

// Thêm Resource vào Map nếu chưa tồn tại và trả về tham chiếu đến Resource đó
Resource &SimulationEngine::ensureResourceExists(const string &resourceId) {
  auto it = resources_.find(resourceId);
  if (it == resources_.end()) {
    auto [newIt, inserted] =
        resources_.emplace(resourceId, Resource{resourceId});
    it = newIt;
  }
  return it->second;
}

SimulationMetrics SimulationEngine::run(const vector<Event> &sortedEvents) {
  resetState();
  if (sortedEvents.empty()) {
    return metrics_;
  }

  registerEventSources(sortedEvents);

  int currentTime = sortedEvents.front().time;
  size_t nextEventIndex = 0;

  while (true) {
    // Giải phóng tài nguyên hết hạn
    releaseExpiredResources(currentTime);

    // Ưu tiên cấp phát cho các yêu cầu cũ (tránh starvation)
    grantPendingRequests(currentTime);

    // Xử lý sự kiện mới tại thời điểm hiện tại (nếu có request thì cấp phát tài
    // nguyên luôn nếu có)
    processEventsAt(currentTime, sortedEvents, nextEventIndex);

    // Áp dụng chính sách timeout
    applyTimeouts(currentTime);

    // Cấp phát lại sau khi timeout (có thể có tài nguyên được giải phóng sau
    // khi kill hoặc retry)
    grantPendingRequests(currentTime);

    // Quét để tìm các process đã hoàn thành
    checkAndCompleteProcesses();

    if (nextEventIndex >= sortedEvents.size() && pendingRequests_.empty() &&
        !hasFutureRelease(currentTime)) {
      break;
    }

    ++currentTime;
  }

  return metrics_;
}

void SimulationEngine::processEventsAt(int currentTime,
                                       const vector<Event> &events,
                                       size_t &nextEventIndex) {
  while (nextEventIndex < events.size() &&
         events[nextEventIndex].time == currentTime) {
    const auto &event = events[nextEventIndex];
    ensureProcessExists(event.processId);
    auto &process = processes_.at(event.processId);
    auto &resource = ensureResourceExists(event.resourceId);

    if (event.action == "request") {
      if (resource.isFree()) {
        PendingRequest request{event.processId, event.resourceId, currentTime,
                               event.duration, 0};
        allocateResource(process, request, currentTime);
        if (event.duration == 0) {
          completeProcess(process);
        }
      } else {
        blockProcess(process, event, currentTime);
      }
    } else if (event.action == "release") {
      if (resource.owner && *resource.owner == event.processId) {
        releaseResource(event.resourceId);
      }
    }

    // Cập nhật số lượng sự kiện còn lại cho process này
    auto remainingIt = remainingEventCount_.find(event.processId);
    if (remainingIt != remainingEventCount_.end()) {
      --remainingIt->second;
    }

    ++nextEventIndex;
  }
}

void SimulationEngine::releaseExpiredResources(int currentTime) {
  vector<string> resourcesToRelease;
  for (const auto &[resourceId, resource] : resources_) {
    if (resource.releaseTime.has_value() &&
        *resource.releaseTime <= currentTime) {
      resourcesToRelease.push_back(resourceId);
    }
  }

  // Release riêng từng resource sau khi đã thu thập hết để tránh sửa đổi
  // iterator map
  for (const auto &resourceId : resourcesToRelease) {
    releaseResource(resourceId);
  }
}

void SimulationEngine::grantPendingRequests(int currentTime) {
  pendingRequests_.erase(
      remove_if(pendingRequests_.begin(), pendingRequests_.end(),
                [&](PendingRequest &request) {
                  if (request.requestTime > currentTime) {
                    return false;
                  }
                  auto &resource = ensureResourceExists(request.resourceId);
                  if (!resource.isFree()) {
                    return false;
                  }
                  auto &process = processes_.at(request.processId);
                  allocateResource(process, request, currentTime);
                  if (request.duration == 0) {
                    completeProcess(process);
                  }
                  return true;
                }),
      pendingRequests_.end());
}

void SimulationEngine::applyTimeouts(int currentTime) {
  const auto records = timeoutManager_.checkTimeouts(
      currentTime, processes_, resources_, pendingRequests_, deadlockDetector_);
  for (const auto &record : records) {
    ++metrics_.timeoutEvents;
    if (record.killed) {

      // build WFGraph
      deadlockDetector_.removeWatingProcess(record.processId);

      ++metrics_.killedProcesses;
    }
    if (record.retried) {
      ++metrics_.retryEvents;
    }
    if (record.deadlockedAtTimeout) {
      ++metrics_.deadlockResolved;
    } else {
      ++metrics_.falsePositives;
    }
  }
}

void SimulationEngine::allocateResource(Process &process,
                                        PendingRequest &request,
                                        int currentTime) {
  auto &resource = ensureResourceExists(request.resourceId);
  resource.owner = process.id;
  process.heldResources.insert(request.resourceId);
  process.state = ProcessState::Running;
  process.requestTime.reset();
  process.waitingFor.reset();

  if (request.duration > 0) {
    resource.releaseTime = currentTime + request.duration;
  } else {
    resource.releaseTime.reset();
  }

  // build WFGraph
  deadlockDetector_.removeWatingProcess(process.id);
}

void SimulationEngine::completeProcess(Process &process) {
  if (process.state == ProcessState::Completed ||
      process.state == ProcessState::Terminated) {
    return;
  }

  // build WFGraph
  deadlockDetector_.removeProcess(process.id);

  process.state = ProcessState::Completed;
  process.requestTime.reset();
  process.waitingFor.reset();

  for (const auto &resourceId : process.heldResources) {
    auto &resource = resources_.at(resourceId);
    resource.owner.reset();
    resource.releaseTime.reset();
  }
  process.heldResources.clear();
  ++metrics_.completedProcesses;
}

void SimulationEngine::releaseResource(const string &resourceId) {
  auto &resource = resources_.at(resourceId);
  if (!resource.owner.has_value())
    return;

  string ownerId = *resource.owner;

  resource.owner.reset();
  resource.releaseTime.reset();

  auto &ownerProcess = processes_.at(ownerId);
  ownerProcess.heldResources.erase(resourceId);
}

void SimulationEngine::checkAndCompleteProcesses() {
  for (auto &[id, process] : processes_) {
    if (process.state == ProcessState::Running &&
        process.heldResources.empty() && remainingEventCount_[id] == 0 &&
        !process.waitingFor.has_value()) {

      completeProcess(process);
    }
  }
}

// Chặn process nếu tài nguyên đang được yêu cầu không có sẵn
void SimulationEngine::blockProcess(Process &process, const Event &event,
                                    int currentTime) {
  process.state = ProcessState::Blocked;
  process.requestTime = currentTime;
  process.waitingFor = event.resourceId;
  pendingRequests_.push_back(PendingRequest{event.processId, event.resourceId,
                                            currentTime, event.duration, 0});

  // build WFGraph - add edge
  // auto &resource = resources_.at(event.resourceId);
  auto &resource = this->ensureResourceExists(event.resourceId);

  if (resource.owner.has_value()) {
    deadlockDetector_.addWaitRelation(process.id, *resource.owner);
  }
}

// Kiểm tra xem có tài nguyên nào sẽ được giải phong trong tương lai không (để
// check điều kiện dừng)
bool SimulationEngine::hasFutureRelease(int currentTime) const {
  return any_of(resources_.begin(), resources_.end(), [&](const auto &pair) {
    const auto &resource = pair.second;
    return resource.releaseTime.has_value() &&
           *resource.releaseTime > currentTime;
  });
}
