#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        if (cmdline[0] == '!') {
            char* recalled_cmdline = recall_history(cmdline);
            free(cmdline);
            cmdline = recalled_cmdline;
            if (cmdline == NULL) {
                continue;
            }
        } else if (cmdline[0] != '\0') {
            add_history_entry(cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}