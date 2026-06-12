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
  int getPid(const string &pid);
  void addEdge(const string &wait, const string &hold);
  void removeEdge(const string &wait, const string &hold);
  bool deadlockDetection();
  bool precheck_deadlock();
};
