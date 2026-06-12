// Test harness nhe, khong dung framework ngoai.
#include "../include/CSVParser.hpp"
#include "../include/DeadlockDetector.hpp"
#include "../include/Models.hpp"
#include "../include/SimulationEngine.hpp"
#include "../include/TimeoutManager.hpp"

#include <cstdio>
#include <fstream>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond);           \
    }                                                                          \
  } while (0)

static std::string writeTemp(const std::string &name,
                             const std::string &content) {
  std::ofstream out(name);
  out << content;
  out.close();
  return name;
}

static void testParser() {
  std::printf("testParser\n");
  const std::string path = writeTemp(
      "tmp_parse_ok.csv",
      "time,process_id,action,resource_id,duration\n"
      "2,P2,request,R2,5\n"
      "0,P1,request,R1,3\n"
      "1,P1,release,R1,0\n");
  auto events = CSVParser::parse(path);
  CHECK(events.size() == 3);
  // Sort tang dan theo time
  CHECK(events[0].time == 0);
  CHECK(events[1].time == 1);
  CHECK(events[2].time == 2);
  CHECK(events[0].processId == "P1");
  CHECK(events[0].action == "request");
  CHECK(events[0].duration == 3);
  std::remove(path.c_str());

  // Sai so cot
  const std::string bad = writeTemp(
      "tmp_parse_bad.csv",
      "time,process_id,action,resource_id,duration\n"
      "0,P1,request,R1\n");
  bool threw = false;
  try {
    CSVParser::parse(bad);
  } catch (const std::exception &) {
    threw = true;
  }
  CHECK(threw);
  std::remove(bad.c_str());

  // Action khong hop le
  const std::string badAction = writeTemp(
      "tmp_parse_action.csv",
      "time,process_id,action,resource_id,duration\n"
      "0,P1,grab,R1,1\n");
  threw = false;
  try {
    CSVParser::parse(badAction);
  } catch (const std::exception &) {
    threw = true;
  }
  CHECK(threw);
  std::remove(badAction.c_str());
}

static void testDetector() {
  std::printf("testDetector\n");
  DeadlockDetector det;
  // P1 cho P2 cho P3 cho P1 -> chu trinh
  det.addWaitRelation("P1", "P2");
  det.addWaitRelation("P2", "P3");
  det.addWaitRelation("P3", "P1");
  CHECK(det.detectDeadlock());
  CHECK(det.isInDeadlock("P1"));
  CHECK(det.isInDeadlock("P2"));
  CHECK(det.isInDeadlock("P3"));

  // P4 cho P1 nhung khong ai cho P4 -> P4 khong trong chu trinh
  det.addWaitRelation("P4", "P1");
  CHECK(det.detectDeadlock());
  CHECK(!det.isInDeadlock("P4"));

  // Pid chua biet
  CHECK(!det.isInDeadlock("P9"));

  // Go P1 -> het chu trinh
  det.removeProcess("P1");
  CHECK(!det.detectDeadlock());

  // Khong chu trinh: chain don thuan
  DeadlockDetector chain;
  chain.addWaitRelation("A", "B");
  chain.addWaitRelation("B", "C");
  CHECK(!chain.detectDeadlock());
  CHECK(!chain.isInDeadlock("A"));
}

static void testTimeoutKill() {
  std::printf("testTimeoutKill\n");
  TimeoutConfig cfg;
  cfg.timeout = 3;
  cfg.strategy = TimeoutStrategy::Kill;
  TimeoutManager mgr(cfg);

  std::map<std::string, Process> processes;
  std::map<std::string, Resource> resources;

  Process p;
  p.id = "P1";
  p.state = ProcessState::Blocked;
  p.requestTime = 0;
  p.waitingFor = "R2";
  p.heldResources.insert("R1");
  processes["P1"] = p;

  Resource r1;
  r1.id = "R1";
  r1.owner = "P1";
  resources["R1"] = r1;

  std::vector<PendingRequest> pending = {
      PendingRequest{"P1", "R2", 0, 5, 0}};

  DeadlockDetector det; // P1 khong trong chu trinh -> false positive
  auto records = mgr.checkTimeouts(5, processes, resources, pending, det);
  CHECK(records.size() == 1);
  CHECK(records[0].waitingTime == 5);
  CHECK(records[0].killed);
  CHECK(records[0].falsePositive); // khong deadlock that
  CHECK(processes["P1"].state == ProcessState::Terminated);
  CHECK(resources["R1"].isFree()); // resource duoc giai phong
  CHECK(pending.empty());          // pending bi xoa
}

static void testTimeoutRetryEscalation() {
  std::printf("testTimeoutRetryEscalation\n");
  TimeoutConfig cfg;
  cfg.timeout = 2;
  cfg.strategy = TimeoutStrategy::Retry;
  cfg.retryDelay = 1;
  cfg.maxRetries = 2;
  TimeoutManager mgr(cfg);

  std::map<std::string, Process> processes;
  std::map<std::string, Resource> resources;
  Process p;
  p.id = "P1";
  p.state = ProcessState::Blocked;
  p.requestTime = 0;
  p.waitingFor = "R1";
  processes["P1"] = p;
  Resource r1;
  r1.id = "R1";
  r1.owner = "P2";
  resources["R1"] = r1;
  std::vector<PendingRequest> pending = {PendingRequest{"P1", "R1", 0, 5, 0}};
  DeadlockDetector det;

  // Retry 1
  auto rec1 = mgr.checkTimeouts(2, processes, resources, pending, det);
  CHECK(rec1.size() == 1);
  CHECK(rec1[0].retried);
  CHECK(processes["P1"].state == ProcessState::Running);
  CHECK(pending.size() == 1);
  CHECK(pending[0].retryCount == 1);

  // Dua ve Blocked, retry 2
  processes["P1"].state = ProcessState::Blocked;
  pending[0].requestTime = 10;
  auto rec2 = mgr.checkTimeouts(20, processes, resources, pending, det);
  CHECK(rec2.size() == 1);
  CHECK(rec2[0].retried);
  CHECK(pending[0].retryCount == 2);

  // retry 3 -> vuot maxRetries -> kill
  processes["P1"].state = ProcessState::Blocked;
  pending[0].requestTime = 30;
  auto rec3 = mgr.checkTimeouts(40, processes, resources, pending, det);
  CHECK(rec3.size() == 1);
  CHECK(rec3[0].killed);
  CHECK(processes["P1"].state == ProcessState::Terminated);
  CHECK(pending.empty());
}

static void testEngineThroughputBounded() {
  std::printf("testEngineThroughputBounded\n");
  const std::string path = writeTemp(
      "tmp_engine.csv",
      "time,process_id,action,resource_id,duration\n"
      "0,P1,request,R1,10\n"
      "0,P2,request,R2,10\n"
      "0,P3,request,R3,10\n"
      "1,P1,request,R2,0\n"
      "1,P2,request,R3,0\n"
      "1,P3,request,R1,0\n");
  auto events = CSVParser::parse(path);

  TimeoutConfig cfg;
  cfg.timeout = 3;
  cfg.strategy = TimeoutStrategy::Retry;
  SimulationEngine engine(cfg);
  auto m = engine.run(events);

  CHECK(m.totalProcesses == 3);
  CHECK(m.throughput() <= 1.0); // KHONG double-count
  CHECK(m.completedProcesses <= m.totalProcesses);
  CHECK(m.falsePositives <= m.timeoutEvents);
  std::remove(path.c_str());

  // Kill tren cung kich ban deadlock
  SimulationEngine engineKill(TimeoutConfig{3, TimeoutStrategy::Kill, 1, 3});
  auto mk = engineKill.run(events);
  CHECK(mk.throughput() <= 1.0);
  CHECK(mk.killedProcesses >= 1); // co process bi kill de go deadlock
}

int main() {
  testParser();
  testDetector();
  testTimeoutKill();
  testTimeoutRetryEscalation();
  testEngineThroughputBounded();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
