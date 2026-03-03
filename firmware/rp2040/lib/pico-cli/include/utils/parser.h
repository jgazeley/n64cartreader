#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 8 // Maximum number of arguments a command can have

/**
 * @brief Parses a mutable input string into an array of arguments (argv).
 *
 * This function modifies the input string by replacing delimiters with null terminators.
 *
 * @param input_string The mutable string to parse.
 * @param argv An array of char pointers to be filled by the parser.
 * @param max_args The maximum number of arguments the argv array can hold.
 * @return The number of arguments found (argc).
 */
int parser_split_string(char* input_string, char* argv[], int max_args);

#endif // PARSER_H