#pragma once

#include "../include/Models.hpp"
#include "../include/TimeoutManager.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

class SimulationEngine {
public:
  SimulationEngine(const TimeoutConfig &config);

  SimulationMetrics run(const vector<Event> &events);

  friend class TimeoutManager;

private:
  TimeoutManager timeoutManager_;
  DeadlockDetector deadlockDetector_;
  SimulationMetrics metrics_;

  map<string, Process> processes_;
  map<string, Resource> resources_;
  vector<PendingRequest> pendingRequests_;
  map<string, int> remainingEventCount_;

  void resetState();
  void registerEventSources(const vector<Event> &events);
  void ensureProcessExists(const string &processId);
  Resource &ensureResourceExists(const string &resourceId);

  void processEventsAt(int currentTime, const vector<Event> &events,
                       size_t &nextEventIndex);
  void releaseExpiredResources(int currentTime);
  void grantPendingRequests(int currentTime);
  void applyTimeouts(int currentTime);

  void allocateResource(Process &process, PendingRequest &request,
                        int currentTime);
  void completeProcess(Process &process);
  void releaseResource(const string &resourceId);
  void checkAndCompleteProcesses();
  void blockProcess(Process &process, const Event &event, int currentTime);

  bool hasFutureRelease(int currentTime) const;
};
