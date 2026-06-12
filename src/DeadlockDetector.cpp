// TODO(Khanh): Hien thuc Wait-For Graph va DFS phat hien chu trinh.
// cpp
#include "../include/DeadlockDetector.hpp"
#include <vector>

int WFGraph::getPid(const string &pid) {
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
  int from = getPid(wait);
  int to = getPid(hold);
  adj[from].insert(to);
}

void WFGraph::removeEdge(const string &wait, const string &hold) {
  int from = getPid(wait);
  int to = getPid(hold);
  adj[from].erase(to);
}

bool WFGraph::dfs(int idx, vector<bool> &visited, vector<bool> &inStack) {
  visited[idx] = true;
  inStack[idx] = true;

  for (const int &i : this->adj[idx]) {
    if (!visited[i]) {
      if (dfs(i, visited, inStack)) {
        return true;
      }
    } else if (inStack[i] == true) {
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

bool WFGraph::precheck_deadlock() { return false; }

// DeadlockDetector

void DeadlockDetector::addWaitRelation(const std::string &waitingPid,
                                       const std::string &holdingPid) {

  graph.addEdge(waitingPid, holdingPid);
}

void DeadlockDetector::removeWaitRelation(const std::string &waitingPid,
                                          const std::string &holdingPid) {

  graph.removeEdge(waitingPid, holdingPid);
}

bool DeadlockDetector::detectDeadlock() { return graph.deadlockDetection(); }

bool DeadlockDetector::precheckDeadlock() { return graph.precheck_deadlock(); }
