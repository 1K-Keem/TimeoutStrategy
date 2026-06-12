// Wait-For Graph + DFS phat hien chu trinh.
#include "../include/DeadlockDetector.hpp"
#include <vector>

int WFGraph::getOrCreatePid(const string &pid) {
  auto it = pidToIndex.find(pid);
  if (it != pidToIndex.end()) {
    return it->second;
  }
  int idx = adj.size();
  pidToIndex[pid] = idx;
  indexToPid.push_back(pid);
  adj.emplace_back();
  return idx;
}

void WFGraph::addEdge(const string &wait, const string &hold) {
  int from = getOrCreatePid(wait);
  int to = getOrCreatePid(hold);
  adj[from].insert(to);
}

void WFGraph::removeEdge(const string &wait, const string &hold) {
  int from = getOrCreatePid(wait);
  int to = getOrCreatePid(hold);
  adj[from].erase(to);
}

void WFGraph::removeOutGoingEdge(const string &pid) {
  auto p = pidToIndex.find(pid);
  if (p == pidToIndex.end()) {
    return;
  }
  int idx = p->second;
  adj[idx].clear();
}

void WFGraph::removePid(const string &pid) {
  auto it = pidToIndex.find(pid);
  if (it == pidToIndex.end())
    return;
  int idx = it->second;
  adj[idx].clear();
  for (auto &neighbors : adj) {
    neighbors.erase(idx);
  }
}

void WFGraph::clear() {
  this->pidToIndex.clear();
  this->indexToPid.clear();
  this->adj.clear();
}

bool WFGraph::dfs(int idx, vector<bool> &visited, vector<bool> &inStack) {
  visited[idx] = true;
  inStack[idx] = true;
  for (const int &i : this->adj[idx]) {
    if (!visited[i]) {
      if (dfs(i, visited, inStack)) {
        return true;
      }
    } else if (inStack[i]) {
      return true;
    }
  }
  inStack[idx] = false;
  return false;
}

bool WFGraph::hasCycle() {
  int size = adj.size();
  vector<bool> visited(size, false);
  vector<bool> inStack(size, false);
  for (int i = 0; i < size; ++i) {
    if (!visited[i]) {
      if (dfs(i, visited, inStack)) {
        return true;
      }
    }
  }
  return false;
}

bool WFGraph::deadlockDetection() { return hasCycle(); }

// Kiem tra reachability tu cur ve target (co duong di cur -> ... -> target).
bool WFGraph::dfsReach(int cur, int target, vector<bool> &visited) {
  if (cur == target) {
    return true;
  }
  visited[cur] = true;
  for (const int &next : adj[cur]) {
    if (next == target) {
      return true;
    }
    if (!visited[next] && dfsReach(next, target, visited)) {
      return true;
    }
  }
  return false;
}

// pid nam trong mot chu trinh khi va chi khi co duong di pid -> ... -> pid.
bool WFGraph::pidInCycle(const string &pid) {
  auto it = pidToIndex.find(pid);
  if (it == pidToIndex.end()) {
    return false;
  }
  int idx = it->second;
  vector<bool> visited(adj.size(), false);
  for (const int &next : adj[idx]) {
    vector<bool> reachVisited(adj.size(), false);
    if (dfsReach(next, idx, reachVisited)) {
      return true;
    }
  }
  (void)visited;
  return false;
}

// DeadlockDetector

void DeadlockDetector::addWaitRelation(const std::string &waitingPid,
                                       const std::string &holdingPid) {
  graph.addEdge(waitingPid, holdingPid);
}

void DeadlockDetector::removeWaitRelation(const std::string &waitingPid,
                                          const std::string &holdingPid) {
  graph.removeEdge(waitingPid, holdingPid);
}

void DeadlockDetector::removeWatingProcess(const string &pid) {
  this->graph.removeOutGoingEdge(pid);
}

void DeadlockDetector::removeProcess(const string &pid) {
  this->graph.removePid(pid);
}

void DeadlockDetector::clear() { this->graph.clear(); }

bool DeadlockDetector::detectDeadlock() { return graph.deadlockDetection(); }

bool DeadlockDetector::isInDeadlock(const std::string &pid) {
  return graph.pidInCycle(pid);
}
