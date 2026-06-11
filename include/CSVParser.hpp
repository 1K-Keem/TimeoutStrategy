#pragma once

#include <string>
#include <vector>

#include "Models.hpp"

class CSVParser {
public:
    static vector<Event> parse(const string& path);
private:
    static string trim(const string& input);
    static vector<string> split(const string& input, char delimiter);
    static int parseInt(const string& text, int lineNumber, const string& fieldName);
};
