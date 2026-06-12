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
       << " <dataset.csv> [timeout] [kill|retry] [retry_delay] [max_retries]\n"
       << "  timeout      nguong TIMEOUT (so nguyen >= 1, mac dinh 5)\n"
       << "  kill|retry   chien luoc xu ly timeout (mac dinh kill)\n"
       << "  retry_delay  so time unit cho truoc khi xin lai (mac dinh 1)\n"
       << "  max_retries  so lan retry toi da truoc khi kill (mac dinh 3)\n";
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
  return strategy == TimeoutStrategy::Kill ? "kill" : "retry";
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  const string datasetPath = argv[1];
  TimeoutConfig config;

  try {
    if (argc >= 3) {
      config.timeout = parsePositiveInt(argv[2], "timeout");
    }
    if (argc >= 4) {
      const string strategyArg = argv[3];
      if (strategyArg == "kill") {
        config.strategy = TimeoutStrategy::Kill;
      } else if (strategyArg == "retry") {
        config.strategy = TimeoutStrategy::Retry;
      } else {
        throw invalid_argument("Chien luoc phai la 'kill' hoac 'retry': " +
                               strategyArg);
      }
    }
    if (argc >= 5) {
      config.retryDelay = parsePositiveInt(argv[4], "retry_delay");
    }
    if (argc >= 6) {
      config.maxRetries = parsePositiveInt(argv[5], "max_retries");
    }
  } catch (const exception &ex) {
    cerr << "Loi tham so: " << ex.what() << "\n";
    printUsage(argv[0]);
    return 1;
  }

  try {
    const vector<Event> events = CSVParser::parse(datasetPath);

    SimulationEngine engine(config);
    const SimulationMetrics metrics = engine.run(events);

    cout << "=== Cau hinh ===\n";
    cout << "dataset       : " << datasetPath << "\n";
    cout << "timeout       : " << config.timeout << "\n";
    cout << "strategy      : " << strategyName(config.strategy) << "\n";
    cout << "retry_delay   : " << config.retryDelay << "\n";
    cout << "max_retries   : " << config.maxRetries << "\n\n";

    cout << "=== Metrics ===\n";
    cout << "total_processes     : " << metrics.totalProcesses << "\n";
    cout << "completed_processes : " << metrics.completedProcesses << "\n";
    cout << "killed_processes    : " << metrics.killedProcesses << "\n";
    cout << "timeout_events      : " << metrics.timeoutEvents << "\n";
    cout << "retry_events        : " << metrics.retryEvents << "\n";
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
