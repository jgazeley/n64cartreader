#ifndef CLI_CORE_H
#define CLI_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CLI_MODE_MENU,
    CLI_MODE_PARSER
} cli_mode_t;

bool cli_core_init(void);
void cli_core_task(void);

void cli_core_set_mode(cli_mode_t new_mode);
cli_mode_t cli_core_get_mode(void);

void cli_core_enable(void);
void cli_core_disable(void);
bool cli_core_is_enabled(void);

/* Application-provided identity */
void cli_set_banner(const char * const *lines, size_t count);
void cli_print_banner(void);

void cli_core_set_prompt(const char *prompt);
const char *cli_core_get_prompt(void);

#endif // CLI_CORE_H