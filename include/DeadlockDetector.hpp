// Todo: Khánh
// hpp

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

public:
  int getOrCreatePid(const string &pid);
  void addEdge(const string &wait, const string &hold);
  void removeEdge(const string &wait, const string &hold);

  void removeOutGoingEdge(const string &pid);
  void removePid(const string &pid);
  bool deadlockDetection();
  void clear();
  // extend
  bool precheck_deadlock();
};

class DeadlockDetector {
private:
  WFGraph graph;

public:
  DeadlockDetector() = default;
  ~DeadlockDetector() = default;

  void addWaitRelation(const std::string &waitingPid,
                       const std::string &holdingPid);

  void removeWaitRelation(const std::string &waitingPid,
                          const std::string &holdingPid);

  void removeWatingProcess(const string &pid);
  void removeProcess(const string &pid);

  void clear();

  bool detectDeadlock();

  bool precheckDeadlock();
};
