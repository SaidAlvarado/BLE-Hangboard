#include "shell.h"


#define SHELL_DEFAULT_BUFSIZE (128)

// Shell Callback definition
static int _cmd_world(int argc, char **argv);
static int _cmd_bye(int argc, char **argv);

// Main functions
int main(void) {

    // Define 2  commands
    const shell_command_t commands[] = {
        {"hello", "reply \'world\'", _cmd_world},
        {"hi", "reply \'bye\'", _cmd_bye},
        {NULL, NULL, NULL}};                    // This NULL termination is important


    char line_buf[SHELL_DEFAULT_BUFSIZE];
    // start the shell
    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

// First command definition
static int _cmd_world(int argc, char **argv) {
    (void)argc;
    (void)argv;

    puts("world!");


    return 0;
}

// Second command definition
static int _cmd_bye(int argc, char **argv) {
    (void)argc;
    (void)argv;

    puts("bye!");


    return 0;
}