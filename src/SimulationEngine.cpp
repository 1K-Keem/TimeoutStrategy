#include "../include/SimulationEngine.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

SimulationEngine::SimulationEngine(const TimeoutConfig &config, bool verbose)
    : timeoutManager_(config), verbose_(verbose) {}

void SimulationEngine::log(int currentTime, const string &message) const
{
  if (verbose_)
  {
    cout << "Time " << currentTime << ": " << message << "\n";
  }
}

void SimulationEngine::resetState()
{
  processes_.clear();
  resources_.clear();
  pendingRequests_.clear();
  remainingEventCount_.clear();
  processEvents_.clear();
  metrics_ = SimulationMetrics{};
  deadlockDetector_.clear();
}

void SimulationEngine::registerEventSources(const vector<Event> &events)
{
  for (const auto &event : events)
  {
    ++remainingEventCount_[event.processId];
    processEvents_[event.processId].push_back(event);
    ensureProcessExists(event.processId);
  }
}

// Them Process vao Map neu chua ton tai va cap nhat tong so process
void SimulationEngine::ensureProcessExists(const string &processId)
{
  if (processes_.find(processId) == processes_.end())
  {
    Process process;
    process.id = processId;
    processes_.emplace(processId, std::move(process));
    ++metrics_.totalProcesses;
  }
}

// Them Resource vao Map neu chua ton tai va tra ve tham chieu den Resource do
Resource &SimulationEngine::ensureResourceExists(const string &resourceId)
{
  auto it = resources_.find(resourceId);
  if (it == resources_.end())
  {
    Resource resource;
    resource.id = resourceId;
    auto [newIt, inserted] = resources_.emplace(resourceId, std::move(resource));
    it = newIt;
  }
  return it->second;
}

SimulationMetrics SimulationEngine::run(const vector<Event> &sortedEvents)
{
  resetState();
  if (sortedEvents.empty())
  {
    return metrics_;
  }

  registerEventSources(sortedEvents);

  int currentTime = sortedEvents.front().time;
  size_t nextEventIndex = 0;

  while (true)
  {
    // Giai phong tai nguyen het han
    releaseExpiredResources(currentTime);

    // Uu tien cap phat cho cac yeu cau cu (tranh starvation)
    grantPendingRequests(currentTime);

    // Xu ly su kien moi tai thoi diem hien tai
    processEventsAt(currentTime, sortedEvents, nextEventIndex);

    // Ap dung chinh sach timeout
    applyTimeouts(currentTime);

    // Cap phat lai sau khi timeout (co the co tai nguyen duoc giai phong)
    grantPendingRequests(currentTime);

    // Quet de tim cac process da hoan thanh
    checkAndCompleteProcesses(currentTime);

    if (nextEventIndex >= sortedEvents.size() && pendingRequests_.empty() &&
        !hasFutureRelease(currentTime))
    {
      break;
    }

    ++currentTime;
  }

  return metrics_;
}

void SimulationEngine::processEventsAt(int currentTime,
                                       const vector<Event> &events,
                                       size_t &nextEventIndex)
{
  while (nextEventIndex < events.size() &&
         events[nextEventIndex].time == currentTime)
  {
    const auto &event = events[nextEventIndex];
    ensureProcessExists(event.processId);
    auto &process = processes_.at(event.processId);

    // Process da Completed/Terminated khong duoc tiep tuc nhan event.
    // Neu khong chan o day, kill/complete co the bi "hoi sinh" boi cac
    // event con lai trong dataset.
    if (!process.isAlive())
    {
      auto remainingIt = remainingEventCount_.find(event.processId);
      if (remainingIt != remainingEventCount_.end())
      {
        --remainingIt->second;
      }
      ++nextEventIndex;
      continue;
    }

    auto &resource = ensureResourceExists(event.resourceId);

    if (event.action == "request")
    {
      if (resource.isFree())
      {
        PendingRequest request{event.processId, event.resourceId, currentTime,
                               event.duration, 0};
        allocateResource(process, request, currentTime);
        log(currentTime, event.processId + " requests " + event.resourceId +
                             " -> Granted");
        if (event.duration == 0)
        {
          completeProcess(process, currentTime);
        }
      }
      else
      {
        blockProcess(process, event, currentTime);
        log(currentTime, event.processId + " requests " + event.resourceId +
                             " -> Blocked (held by " + *resource.owner + ")");
      }
    }
    else if (event.action == "release")
    {
      if (resource.owner && *resource.owner == event.processId)
      {
        releaseResource(event.resourceId);
        log(currentTime,
            event.processId + " releases " + event.resourceId);
      }
    }

    auto remainingIt = remainingEventCount_.find(event.processId);
    if (remainingIt != remainingEventCount_.end())
    {
      --remainingIt->second;
    }

    ++nextEventIndex;
  }
}

void SimulationEngine::releaseExpiredResources(int currentTime)
{
  vector<string> resourcesToRelease;
  for (const auto &[resourceId, resource] : resources_)
  {
    if (resource.releaseTime.has_value() &&
        *resource.releaseTime <= currentTime)
    {
      resourcesToRelease.push_back(resourceId);
    }
  }
  
  for (const auto &resourceId : resourcesToRelease)
  {
    releaseResource(resourceId);
  }
}

void SimulationEngine::grantPendingRequests(int currentTime)
{
  pendingRequests_.erase(
      remove_if(pendingRequests_.begin(), pendingRequests_.end(),
                [&](PendingRequest &request)
                {
                  auto &process = processes_.at(request.processId);
                  // Bo cac pending cua process da ket thuc (tranh "hoi sinh"
                  // process da Completed/Terminated -> double-count throughput).
                  if (!process.isAlive())
                  {
                    return true;
                  }
                  if (request.requestTime > currentTime)
                  {
                    return false;
                  }
                  auto &resource = ensureResourceExists(request.resourceId);
                  if (!resource.isFree())
                  {
                    return false;
                  }
                  allocateResource(process, request, currentTime);
                  log(currentTime, request.processId + " acquires " +
                                       request.resourceId +
                                       " -> Granted (was waiting)");
                  if (request.duration == 0)
                  {
                    completeProcess(process, currentTime);
                  }
                  return true;
                }),
      pendingRequests_.end());
}

void SimulationEngine::applyTimeouts(int currentTime)
{
  const auto records = timeoutManager_.checkTimeouts(
      currentTime, processes_, resources_, pendingRequests_, deadlockDetector_);
  for (const auto &record : records)
  {
    ++metrics_.timeoutEvents;

    // False positive: process bi timeout nhung KHONG nam trong chu trinh that.
    if (!record.deadlockedAtTimeout)
    {
      ++metrics_.falsePositives;
    }

    if (record.retried)
    {
      ++metrics_.retryEvents;
    }
    if (record.rolledBack)
    {
      ++metrics_.rollbackEvents;
    }

    const string fp = record.deadlockedAtTimeout ? "deadlock" : "false positive";
    const string base = record.processId + " TIMEOUT (waited " +
                        std::to_string(record.waitingTime) + ", " + fp + ")";

    // Cap nhat Wait-For Graph theo hanh dong, do cycle truoc/sau de biet
    // deadlock co thuc su duoc go khong.
    const bool cycleBefore = deadlockDetector_.detectDeadlock();
    if (record.killed)
    {
      deadlockDetector_.removeProcess(record.processId);
      ++metrics_.killedProcesses;
      log(currentTime, base + " -> Killed");
    }
    else if (record.rolledBack)
    {
      // Process tra ve trang thai ban dau: go khoi do thi va re-inject events.
      deadlockDetector_.removeProcess(record.processId);
      replayProcess(record.processId, currentTime);
      log(currentTime, base + " -> Rolled back");
    }
    else if (record.retried)
    {
      deadlockDetector_.removeWatingProcess(record.processId);
      log(currentTime, base + " -> Retry");
    }
    const bool cycleAfter = deadlockDetector_.detectDeadlock();

    if (record.deadlockedAtTimeout && cycleBefore && !cycleAfter)
    {
      ++metrics_.deadlockResolved;
      log(currentTime, "Deadlock resolved (cycle broken)");
    }
  }
}

// Rollback: chay lai process tu dau bang cach re-inject toan bo request event
// cua no vao hang doi pending tai thoi diem hien tai.
void SimulationEngine::replayProcess(const string &processId, int currentTime)
{
  auto it = processEvents_.find(processId);
  if (it == processEvents_.end())
  {
    return;
  }

  // Don sach pending con sot cua process (phong truong hop).
  pendingRequests_.erase(remove_if(pendingRequests_.begin(),
                                   pendingRequests_.end(),
                                   [&](const PendingRequest &item)
                                   {
                                     return item.processId == processId;
                                   }),
                         pendingRequests_.end());

  // Moi event cua process da duoc bieu dien lai thanh pending -> khong con
  // event nguon nao cho doc nua.
  remainingEventCount_[processId] = 0;

  auto &process = processes_.at(processId);
  process.state = ProcessState::New;
  process.requestTime.reset();
  process.waitingFor.reset();

  for (const auto &event : it->second)
  {
    if (event.action == "request")
    {
      pendingRequests_.push_back(PendingRequest{
          event.processId, event.resourceId, currentTime, event.duration, 0});
    }
  }
}

void SimulationEngine::allocateResource(Process &process,
                                        PendingRequest &request,
                                        int currentTime)
{
  auto &resource = ensureResourceExists(request.resourceId);
  resource.owner = process.id;
  process.heldResources.insert(request.resourceId);
  process.state = ProcessState::Running;
  process.requestTime.reset();
  process.waitingFor.reset();

  if (request.duration > 0)
  {
    resource.releaseTime = currentTime + request.duration;
  }
  else
  {
    resource.releaseTime.reset();
  }

  deadlockDetector_.removeWatingProcess(process.id);
}

void SimulationEngine::completeProcess(Process &process, int currentTime)
{
  if (process.state == ProcessState::Completed ||
      process.state == ProcessState::Terminated)
  {
    return;
  }

  deadlockDetector_.removeProcess(process.id);

  process.state = ProcessState::Completed;
  process.requestTime.reset();
  process.waitingFor.reset();

  for (const auto &resourceId : process.heldResources)
  {
    auto &resource = resources_.at(resourceId);
    resource.owner.reset();
    resource.releaseTime.reset();
  }
  process.heldResources.clear();
  if (!process.completionCounted)
  {
    ++metrics_.completedProcesses;
    process.completionCounted = true;
  }
  log(currentTime, process.id + " -> Completed");
}

void SimulationEngine::releaseResource(const string &resourceId)
{
  auto &resource = resources_.at(resourceId);
  if (!resource.owner.has_value())
    return;

  string ownerId = *resource.owner;

  resource.owner.reset();
  resource.releaseTime.reset();

  auto &ownerProcess = processes_.at(ownerId);
  ownerProcess.heldResources.erase(resourceId);
}

void SimulationEngine::checkAndCompleteProcesses(int currentTime)
{
  for (auto &[id, process] : processes_)
  {
    if (process.state == ProcessState::Running &&
        process.heldResources.empty() && remainingEventCount_[id] == 0 &&
        !process.waitingFor.has_value())
    {
      completeProcess(process, currentTime);
    }
  }
}

// Chan process neu tai nguyen dang yeu cau khong co san
void SimulationEngine::blockProcess(Process &process, const Event &event,
                                    int currentTime)
{
  process.state = ProcessState::Blocked;
  process.requestTime = currentTime;
  process.waitingFor = event.resourceId;
  pendingRequests_.push_back(PendingRequest{event.processId, event.resourceId,
                                            currentTime, event.duration, 0});

  auto &resource = this->ensureResourceExists(event.resourceId);
  if (resource.owner.has_value())
  {
    deadlockDetector_.addWaitRelation(process.id, *resource.owner);
  }
}

// Kiem tra co tai nguyen nao se duoc giai phong trong tuong lai khong
bool SimulationEngine::hasFutureRelease(int currentTime) const
{
  return any_of(resources_.begin(), resources_.end(), [&](const auto &pair)
                {
    const auto &resource = pair.second;
    return resource.releaseTime.has_value() &&
           *resource.releaseTime > currentTime; });
}
