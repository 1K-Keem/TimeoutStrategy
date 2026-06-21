#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <stddef.h>
#include "Models.h"

Event* CSVParser_parse(const char* path, size_t* out_count);

#endif