#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>

#include "cli.h"
#include "switch/switch.h"

/*------------------------------------------------------------------------------
 * Definitions
 *----------------------------------------------------------------------------*/
#define CMD_BUFFER_SIZE 200
#define MAX_ARGS 8

/*------------------------------------------------------------------------------
 * Types
 *----------------------------------------------------------------------------*/
typedef void (*cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    const char *usage;
} cli_command_t;

/*------------------------------------------------------------------------------
 * Static Variables
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 * Static Function Declarations
 *----------------------------------------------------------------------------*/

 /**
  * @brief Handle the help command.
  *        Print the available commands.
  *
  * @param argc The number of arguments
  * @param argv The arguments
  */
static void cmd_help(int argc, char **argv);

/**
 * @brief Handle the connect command.
 *        Connect a switch port to a network interface.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 */
static void cmd_connect(int argc, char **argv);

/**
 * @brief Handle the disconnect command.
 *        Disconnect a switch port from a network interface.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 */
static void cmd_disconnect(int argc, char **argv);

/**
 * @brief Handle the show command.
 *        Show the status of the switch ports.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 */
static void cmd_show(int argc, char **argv);
/**
 * @brief Parse the arguments from a command line.
 *
 * @param line The command line to parse
 * @param argv The arguments
 * @param max_args The maximum number of arguments
 */
static int parse_args(char *line, char **argv, int max_args);

/**
 * @brief Find a command in the command table.
 *
 * @param name The name of the command to find
 * @return The command, or NULL if not found
 */
static cli_command_t *find_command(const char *name);

/*------------------------------------------------------------------------------
 * Command Table
 *----------------------------------------------------------------------------*/
 static cli_command_t commands[] = {
    {"connect", cmd_connect, "connect <port> <interface> - Bind a switch port to a network interface"},
    {"disconnect", cmd_disconnect, "disconnect <port> - Disconnect a switch port from a network interface"},
    {"show", cmd_show, "Show the status of the switch ports"},
    {"help",    cmd_help,    "help                      - Show available commands"},
    {NULL, NULL, NULL}
};

/*------------------------------------------------------------------------------
 * Static Function Implementations
 *----------------------------------------------------------------------------*/

 /* ---------------- Command Handlers ---------------- */
static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Available commands:\n");
    for (int i = 0; commands[i].name != NULL; i++) {
        printf("  %s\n", commands[i].usage);
    }
    printf("  exit                      - Shut down the switch\n");
}

static void cmd_connect(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: connect <port> <interface>\n");
        return;
    }

    int port = atoi(argv[1]);
    const char *iface = argv[2];

    if (switch_connect_port(port, iface) < 0) {
        printf("Error: Invalid port number. Use 1-%d.\n", MAX_PORTS);
        return;
    }

    printf("Command sent: Connect Port %d to %s\n", port, iface);
}

static void cmd_disconnect(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: disconnect <port>\n");
        return;
    }

    int port = atoi(argv[1]);
    switch_disconnect_port(port);
}

static void cmd_show(int argc, char **argv) {
    (void)argc;
    (void)argv;

    switch_show_port_status();
}

/* ---------------- Helper Functions ---------------- */
static int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *token = strtok(line, " \t");

    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    return argc;
}

static cli_command_t *find_command(const char *name) {
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

/*------------------------------------------------------------------------------
 * Public Functions
 *----------------------------------------------------------------------------*/
void cli_run(void) {
    char cmd_buffer[CMD_BUFFER_SIZE];
    char *argv[MAX_ARGS];

    while (1) {
        printf("Switch> ");
        if (fgets(cmd_buffer, sizeof(cmd_buffer), stdin) == NULL) {
            break;
        }

        // Remove trailing newline
        cmd_buffer[strcspn(cmd_buffer, "\n")] = '\0';

        // If only pressed enter, continue
        if (cmd_buffer[0] == '\0') {
            continue;
        }

        if (strcmp(cmd_buffer, "exit") == 0) {
            break;
        }

        int argc = parse_args(cmd_buffer, argv, MAX_ARGS);
        if (argc == 0) {
            continue;
        }

        cli_command_t *cmd = find_command(argv[0]);
        if (cmd != NULL) {
            cmd->handler(argc, argv);
        } else {
            printf("Unknown command: %s. Type 'help' for available commands.\n", argv[0]);
        }
    }
}
