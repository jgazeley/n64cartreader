#ifndef CLI_CMD_TEST_H
#define CLI_CMD_TEST_H

#include "cli/command.h"

/**  
 * Install the built-in “fillbuf”, “dumpbuf”, etc. commands  
 * and populate their descriptor pointers for the menu.  
 */
void test_cmds_init(void);

/* These let your menu.c link menu items → command descriptors */
extern const cli_command_t *cmd_fill_buffer_desc;
extern const cli_command_t *cmd_dump_buffer_desc;
extern const cli_command_t *cmd_write_message_desc;
extern const cli_command_t *cmd_stream_buffer_desc;
extern const cli_command_t *cmd_send_buffer_desc;

#endif // CLI_CMD_TEST_H
