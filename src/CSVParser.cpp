#include "../include/CSVParser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

string CSVParser::trim(const string &input) {
  auto start = input.find_first_not_of(" \t\r\n");
  if (start == string::npos) {
    return {};
  }
  auto end = input.find_last_not_of(" \t\r\n");
  return input.substr(start, end - start + 1);
}

vector<string> CSVParser::split(const string &input, char delimiter) {
  vector<string> result;
  string token;
  istringstream stream(input);
  while (getline(stream, token, delimiter)) {
    result.push_back(token);
  }
  return result;
}

int CSVParser::parseInt(const string &text, int lineNumber,
                        const string &fieldName) {
  const string trimmed = trim(text);
  if (trimmed.empty()) {
    throw runtime_error("Invalid integer value for " + fieldName + " at line " +
                        to_string(lineNumber));
  }
  try {
    size_t pos = 0;
    int value = stoi(trimmed, &pos);
    if (pos != trimmed.size()) {
      throw invalid_argument("extra characters");
    }
    return value;
  } catch (const exception &) {
    throw runtime_error("Invalid integer value for " + fieldName + " at line " +
                        to_string(lineNumber) + ": '" + trimmed + "'");
  }
}

vector<Event> CSVParser::parse(const string &path) {
  ifstream input(path);
  if (!input.is_open()) {
    throw runtime_error("Unable to open CSV file: " + path);
  }

  vector<Event> events;
  string line;
  int lineNumber = 0;

  if (!getline(input, line)) {
    throw runtime_error("CSV file is empty: " + path);
  }
  lineNumber = 1;

  while (getline(input, line)) {
    lineNumber++;
    if (trim(line).empty()) {
      continue;
    }

    const vector<string> columns = split(line, ',');
    if (columns.size() != 5) {
      throw runtime_error("Invalid CSV format at line " +
                          to_string(lineNumber) + ": expected 5 columns, got " +
                          to_string(columns.size()));
    }

    Event event;
    event.time = parseInt(columns[0], lineNumber, "time");
    event.processId = trim(columns[1]);
    event.action = trim(columns[2]);
    event.resourceId = trim(columns[3]);
    event.duration = parseInt(columns[4], lineNumber, "duration");

    if (event.processId.empty()) {
      throw runtime_error("Empty process_id at line " + to_string(lineNumber));
    }
    if (event.resourceId.empty()) {
      throw runtime_error("Empty resource_id at line " + to_string(lineNumber));
    }
    if (event.action != "request" && event.action != "release") {
      throw runtime_error("Invalid action at line " + to_string(lineNumber) +
                          ": must be 'request' or 'release'");
    }
    if (event.duration < 0) {
      throw runtime_error("Invalid duration at line " + to_string(lineNumber) +
                          ": must be >= 0");
    }

    events.push_back(move(event));
  }

  sort(events.begin(), events.end(),
       [](const Event &a, const Event &b) { return a.time < b.time; });

  return events;
}
