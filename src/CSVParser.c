#include "CSVParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * CSVParser — đọc và validate dataset CSV.
 *
 * Định dạng mỗi dòng (sau header): time, process_id, action, resource_id, duration
 *   - Bỏ qua header và các dòng trống.
 *   - Mỗi dòng phải có đúng 5 cột, nếu không sẽ exit() kèm số dòng.
 *   - action phải là "request" hoặc "release"; duration >= 0.
 * Sau khi đọc xong, danh sách event được sắp xếp tăng dần theo `time`.
 */

static char* trim(const char* input) {
    // Cắt khoảng trắng phía trước.
    while (isspace((unsigned char)*input)) {
        input++;
    }

    // Nếu chuỗi rỗng thì trả về chuỗi rỗng được cấp phát.
    if (*input == '\0') {
        char* empty = malloc(1);
        empty[0] = '\0';
        return empty;
    }

    // Cắt khoảng trắng phía sau.
    const char* end = input + strlen(input) - 1;
    while (end > input && isspace((unsigned char)*end)) {
        end--;
    }

    // Trả về bản copy của phần đã cắt.
    size_t len = (end - input) + 1;
    char* res = malloc(len + 1);
    strncpy(res, input, len);
    res[len] = '\0';
    return res;
}

// Tách chuỗi theo delimiter, trả về mảng các chuỗi (caller free từng phần tử).
static char** split(const char* input, char delimiter, size_t* out_count) {
    size_t count = 0;
    size_t capacity = 10;
    char** result = malloc(capacity * sizeof(char*));
    const char* start = input;
    const char* end;

    while ((end = strchr(start, delimiter)) != NULL) {
        // Tăng capacity nếu cần.
        if (count >= capacity) {
            capacity *= 2;
            result = realloc(result, capacity * sizeof(char*));
        }

        size_t len = end - start;
        result[count] = malloc(len + 1);
        strncpy(result[count], start, len);
        result[count][len] = '\0';
        start = end + 1;
        count++;
    }

    // Phần tử cuối cùng nằm sau delimiter cuối.
    if (count >= capacity) {
        capacity += 1;
        result = realloc(result, capacity * sizeof(char*));
    }

    size_t len = strlen(start);
    result[count] = malloc(len + 1);
    strcpy(result[count], start);
    count++;

    *out_count = count;
    return result;
}

// Chuyển chuỗi sang int, có validate; nếu lỗi thì in kèm số dòng và exit.
static int parseInt(const char* text, int lineNumber, const char* fieldName) {
    char* trimmed = trim(text);
    if (strlen(trimmed) == 0) {
        fprintf(stderr, "Invalid integer value for %s at line %d\n", fieldName, lineNumber);
        free(trimmed);
        exit(EXIT_FAILURE);
    }

    char* endptr;
    // strtol cho phép phân biệt parse lỗi (an toàn hơn atoi).
    long value = strtol(trimmed, &endptr, 10);

    // Nếu còn ký tự không phải số -> lỗi.
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid integer value for %s at line %d: '%s'\n", fieldName, lineNumber, trimmed);
        free(trimmed);
        exit(EXIT_FAILURE);
    }

    free(trimmed);
    return (int)value;
}

// Hàm so sánh để qsort theo `time` tăng dần.
static int compare_events(const void* a, const void* b) {
    const Event* ea = (const Event*)a;
    const Event* eb = (const Event*)b;
    return ea->time - eb->time;
}

Event* CSVParser_parse(const char* path, size_t* out_count) {
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Unable to open CSV file: %s\n", path);
        exit(EXIT_FAILURE);
    }

    size_t capacity = 10;
    size_t count = 0;
    Event* events = malloc(capacity * sizeof(Event));

    char line[1024];
    int lineNumber = 0;

    // Đọc và bỏ qua dòng header
    if (!fgets(line, sizeof(line), file)) {
        fprintf(stderr, "CSV file is empty: %s\n", path);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    lineNumber = 1;

    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        line[strcspn(line, "\r\n")] = '\0';

        char* trimmed_line = trim(line);
        if (strlen(trimmed_line) == 0) {
            free(trimmed_line);
            continue;
        }
        free(trimmed_line);

        size_t col_count = 0;
        char** columns = split(line, ',', &col_count);

        if (col_count != 5) {
            fprintf(stderr, "Invalid CSV format at line %d: expected 5 columns, got %zu\n", lineNumber, col_count);
            exit(EXIT_FAILURE);
        }

        Event event;
        event.time = parseInt(columns[0], lineNumber, "time");
        event.processId = trim(columns[1]);
        event.action = trim(columns[2]);
        event.resourceId = trim(columns[3]);
        event.duration = parseInt(columns[4], lineNumber, "duration");

        if (strlen(event.processId) == 0) {
            fprintf(stderr, "Empty process_id at line %d\n", lineNumber);
            exit(EXIT_FAILURE);
        }
        if (strlen(event.resourceId) == 0) {
            fprintf(stderr, "Empty resource_id at line %d\n", lineNumber);
            exit(EXIT_FAILURE);
        }
        if (strcmp(event.action, "request") != 0 && strcmp(event.action, "release") != 0) {
            fprintf(stderr, "Invalid action at line %d: must be 'request' or 'release'\n", lineNumber);
            exit(EXIT_FAILURE);
        }
        if (event.duration < 0) {
            fprintf(stderr, "Invalid duration at line %d: must be >= 0\n", lineNumber);
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < col_count; i++) {
            free(columns[i]);
        }
        free(columns);

        if (count >= capacity) {
            capacity *= 2;
            events = realloc(events, capacity * sizeof(Event));
        }
        events[count++] = event;
    }

    fclose(file);

    qsort(events, count, sizeof(Event), compare_events);

    *out_count = count;
    return events;
}