#include "shell.h"

int main() {
    char* cmdline;

    rl_attempted_completion_function = complete_input;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        reap_background_jobs();

        if (cmdline[0] != '\0') {
            add_history(cmdline);
        }

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

        char* command = strtok(cmdline, ";");
        while (command != NULL) {
            char** arglist = tokenize(command);
            if (arglist != NULL) {
                int last = 0;
                while (arglist[last] != NULL) {
                    last++;
                }

                int background = last > 0 &&
                                 strcmp(arglist[last - 1], "&") == 0;
                if (background) {
                    free(arglist[last - 1]);
                    arglist[last - 1] = NULL;
                }

                if (arglist[0] != NULL && !handle_builtin(arglist)) {
                    execute_command(arglist, background, command);
                }

                for (int i = 0; arglist[i] != NULL; i++) {
                    free(arglist[i]);
                }
                free(arglist);
            }

            command = strtok(NULL, ";");
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}