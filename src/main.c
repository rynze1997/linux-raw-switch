#include <stdio.h>

#include "switch/switch.h"
#include "cli/cli.h"

int main(void) {
    printf("Starting Simple Switch on %d ports...\n", MAX_PORTS);

    switch_init();
    switch_start();

    cli_run(); // Blocks until "exit" or EOF

    printf("Exiting...\n");
    switch_stop();

    return 0;
}
