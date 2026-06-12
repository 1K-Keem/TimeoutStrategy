#include "../include/CSVParser.hpp"
#include "../include/Models.hpp"
#include "../include/SimulationEngine.hpp"
#include "../include/TimeoutManager.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace {

void printUsage(const char *prog) {
  cerr << "Usage: " << prog
       << " <dataset.csv> [timeout] [kill|retry|rollback] [retry_delay] "
          "[max_retries|max_rollbacks]\n"
       << "  timeout      nguong TIMEOUT (so nguyen >= 1, mac dinh 5)\n"
       << "  strategy     kill | retry | rollback (mac dinh kill)\n"
       << "  retry_delay  so time unit cho truoc khi xin lai (mac dinh 1)\n"
       << "  max_*        so lan retry/rollback toi da truoc khi kill (mac dinh 3)\n"
       << "  -v|--verbose in log tung su kien theo time unit\n";
}

int parsePositiveInt(const string &text, const string &fieldName) {
  size_t pos = 0;
  int value = stoi(text, &pos);
  if (pos != text.size()) {
    throw invalid_argument("Gia tri khong hop le cho " + fieldName + ": " + text);
  }
  return value;
}

const char *strategyName(TimeoutStrategy strategy) {
  switch (strategy) {
  case TimeoutStrategy::Kill:
    return "kill";
  case TimeoutStrategy::Retry:
    return "retry";
  case TimeoutStrategy::Rollback:
    return "rollback";
  }
  return "unknown";
}

} // namespace

int main(int argc, char **argv) {
  // Tach flag (-v/--verbose) khoi tham so vi tri.
  bool verbose = false;
  vector<string> pos;
  for (int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else {
      pos.push_back(arg);
    }
  }

  if (pos.empty()) {
    printUsage(argv[0]);
    return 1;
  }

  const string datasetPath = pos[0];
  TimeoutConfig config;

  try {
    if (pos.size() >= 2) {
      config.timeout = parsePositiveInt(pos[1], "timeout");
    }
    if (pos.size() >= 3) {
      const string strategyArg = pos[2];
      if (strategyArg == "kill") {
        config.strategy = TimeoutStrategy::Kill;
      } else if (strategyArg == "retry") {
        config.strategy = TimeoutStrategy::Retry;
      } else if (strategyArg == "rollback") {
        config.strategy = TimeoutStrategy::Rollback;
      } else {
        throw invalid_argument(
            "Chien luoc phai la 'kill', 'retry' hoac 'rollback': " +
            strategyArg);
      }
    }
    if (pos.size() >= 4) {
      config.retryDelay = parsePositiveInt(pos[3], "retry_delay");
    }
    if (pos.size() >= 5) {
      if (config.strategy == TimeoutStrategy::Rollback) {
        config.maxRollbacks = parsePositiveInt(pos[4], "max_rollbacks");
      } else {
        config.maxRetries = parsePositiveInt(pos[4], "max_retries");
      }
    }
  } catch (const exception &ex) {
    cerr << "Loi tham so: " << ex.what() << "\n";
    printUsage(argv[0]);
    return 1;
  }

  try {
    const vector<Event> events = CSVParser::parse(datasetPath);

    SimulationEngine engine(config, verbose);
    if (verbose) {
      cout << "=== Event log ===\n";
    }
    const SimulationMetrics metrics = engine.run(events);
    if (verbose) {
      cout << "\n";
    }

    cout << "=== Cau hinh ===\n";
    cout << "dataset       : " << datasetPath << "\n";
    cout << "timeout       : " << config.timeout << "\n";
    cout << "strategy      : " << strategyName(config.strategy) << "\n";
    cout << "retry_delay   : " << config.retryDelay << "\n";
    cout << "max_retries   : " << config.maxRetries << "\n";
    cout << "max_rollbacks : " << config.maxRollbacks << "\n\n";

    cout << "=== Metrics ===\n";
    cout << "total_processes     : " << metrics.totalProcesses << "\n";
    cout << "completed_processes : " << metrics.completedProcesses << "\n";
    cout << "killed_processes    : " << metrics.killedProcesses << "\n";
    cout << "timeout_events      : " << metrics.timeoutEvents << "\n";
    cout << "retry_events        : " << metrics.retryEvents << "\n";
    cout << "rollback_events     : " << metrics.rollbackEvents << "\n";
    cout << "deadlock_resolved   : " << metrics.deadlockResolved << "\n";
    cout << "false_positives     : " << metrics.falsePositives << "\n";
    cout << "throughput          : " << metrics.throughput() << "\n";
    cout << "false_positive_rate : " << metrics.falsePositiveRate() << "\n";
  } catch (const exception &ex) {
    cerr << "Loi: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}








