// Wait-For Graph + DFS phat hien chu trinh (deadlock).
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std;

class WFGraph {
private:
  unordered_map<string, int> pidToIndex;
  vector<string> indexToPid;
  vector<unordered_set<int>> adj;

  bool hasCycle();
  bool dfs(int idx, vector<bool> &visited, vector<bool> &inStack);
  bool dfsReach(int cur, int target, vector<bool> &visited);

public:
  int getOrCreatePid(const string &pid);
  void addEdge(const string &wait, const string &hold);
  void removeEdge(const string &wait, const string &hold);

  void removeOutGoingEdge(const string &pid);
  void removePid(const string &pid);
  bool deadlockDetection();
  bool pidInCycle(const string &pid);
  void clear();
};

class DeadlockDetector {
private:
  WFGraph graph;

public:
  DeadlockDetector() = default;
  ~DeadlockDetector() = default;

  void addWaitRelation(const string &waitingPid,
                       const string &holdingPid);

  void removeWaitRelation(const string &waitingPid,
                          const string &holdingPid);

  void removeWatingProcess(const string &pid);
  void removeProcess(const string &pid);

  void clear();

  bool detectDeadlock();
  bool isInDeadlock(const string &pid);
};
