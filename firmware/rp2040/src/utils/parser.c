#include "utils/parser.h"
#include <string.h>
#include <stdbool.h>

// A simple parser using strtok.
// Note: strtok is not re-entrant, but is simple for this use case.
int parser_split_string(char* input_string, char* argv[], int max_args) {
    int argc = 0;
    const char* delimiter = " \t\n\r"; // space, tab, newline, carriage return

    char* token = strtok(input_string, delimiter);
    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token = strtok(NULL, delimiter);
    }

    return argc;
}