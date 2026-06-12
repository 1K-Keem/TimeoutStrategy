#include "../include/CSVParser.hpp"
#include "../include/Models.hpp"
#include "../include/SimulationEngine.hpp"
#include "../include/TimeoutManager.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
       << "  -v|--verbose in log tung su kien theo time unit\n"
       << "  -c|--compare chay ca 3 chien luoc (kill/retry/rollback) va in bang so sanh\n";
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

void printMetrics(const TimeoutConfig &config, const string &datasetPath,
                  const SimulationMetrics &metrics) {
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
}

void printCompareTable(const TimeoutConfig &base, const string &datasetPath,
                       const vector<Event> &events) {
  const TimeoutStrategy strategies[] = {
      TimeoutStrategy::Kill, TimeoutStrategy::Retry, TimeoutStrategy::Rollback};

  cout << "=== So sanh chien luoc ===\n";
  cout << "dataset : " << datasetPath << "\n";
  cout << "timeout : " << base.timeout << "\n\n";

  cout << left << setw(10) << "strategy" << right << setw(10) << "completed"
       << setw(8) << "killed" << setw(8) << "retries" << setw(10) << "rollbacks"
       << setw(10) << "resolved" << setw(8) << "fp" << setw(12) << "throughput"
       << setw(10) << "fp_rate" << "\n";
  cout << string(86, '-') << "\n";

  for (const auto strategy : strategies) {
    TimeoutConfig config = base;
    config.strategy = strategy;
    SimulationEngine engine(config, false);
    const SimulationMetrics m = engine.run(events);

    cout << left << setw(10) << strategyName(strategy) << right << setw(10)
         << m.completedProcesses << setw(8) << m.killedProcesses << setw(8)
         << m.retryEvents << setw(10) << m.rollbackEvents << setw(10)
         << m.deadlockResolved << setw(8) << m.falsePositives << setw(12)
         << fixed << setprecision(3) << m.throughput() << setw(10)
         << m.falsePositiveRate() << "\n";
  }
}

} // namespace

int main(int argc, char **argv) {
  // Tach flag (-v/--verbose, -c/--compare) khoi tham so vi tri.
  bool verbose = false;
  bool compare = false;
  vector<string> pos;
  for (int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "-c" || arg == "--compare") {
      compare = true;
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
      // Trong che do compare ca maxRetries lan maxRollbacks deu duoc dung.
      config.maxRetries = parsePositiveInt(pos[4], "max_retries|max_rollbacks");
      config.maxRollbacks = config.maxRetries;
    }
  } catch (const exception &ex) {
    cerr << "Loi tham so: " << ex.what() << "\n";
    printUsage(argv[0]);
    return 1;
  }

  try {
    const vector<Event> events = CSVParser::parse(datasetPath);

    if (compare) {
      printCompareTable(config, datasetPath, events);
      return 0;
    }

    SimulationEngine engine(config, verbose);
    if (verbose) {
      cout << "=== Event log ===\n";
    }
    const SimulationMetrics metrics = engine.run(events);
    if (verbose) {
      cout << "\n";
    }

    printMetrics(config, datasetPath, metrics);
  } catch (const exception &ex) {
    cerr << "Loi: " << ex.what() << "\n";
    return 1;
  }

  return 0;
}
