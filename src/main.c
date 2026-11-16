#include "shell.h"
#include <ctype.h>

#define MAX_IF_LINES 32

static void free_arguments(char** arglist) {
    if (arglist == NULL) {
        return;
    }

    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
}

static char* trim_whitespace(char* text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static int is_keyword(const char* line, const char* keyword) {
    return strcmp(trim_whitespace((char*)line), keyword) == 0;
}

static void record_readline_history(const char* line) {
    if (line != NULL && line[0] != '\0') {
        add_history(line);
        add_history_entry(line);
    }
}

static int execute_line(char* line) {
    int last_status = EXIT_SUCCESS;
    char* command = strtok(line, ";");

    while (command != NULL) {
        char** arglist = tokenize(command);
        if (arglist != NULL) {
            int argument_count = 0;
            while (arglist[argument_count] != NULL) {
                argument_count++;
            }

            int background = argument_count > 0 &&
                             strcmp(arglist[argument_count - 1], "&") == 0;
            if (background) {
                free(arglist[argument_count - 1]);
                arglist[argument_count - 1] = NULL;
            }

            if (arglist[0] != NULL && !handle_builtin(arglist)) {
                last_status = execute_command(arglist, background, command);
            }
            free_arguments(arglist);
        }
        command = strtok(NULL, ";");
    }

    return last_status;
}

static char* read_block_line(void) {
    char* line = read_cmd(PROMPT, stdin);
    if (line != NULL) {
        record_readline_history(line);
    }
    return line;
}

static int execute_if_block(char* condition, char* then_lines[],
                            int then_count, char* else_lines[],
                            int else_count) {
    char** condition_args = tokenize(condition);
    if (condition_args == NULL) {
        fprintf(stderr, "if: missing condition\n");
        return EXIT_FAILURE;
    }

    int condition_status = execute_command(condition_args, 0, condition);
    free_arguments(condition_args);

    char** selected_lines = condition_status == EXIT_SUCCESS
                                ? then_lines
                                : else_lines;
    int selected_count = condition_status == EXIT_SUCCESS
                             ? then_count
                             : else_count;
    int status = condition_status;

    for (int i = 0; i < selected_count; i++) {
        status = execute_line(selected_lines[i]);
    }
    return status;
}

static int read_and_execute_if(char* first_line) {
    char* then_lines[MAX_IF_LINES] = {0};
    char* else_lines[MAX_IF_LINES] = {0};
    int then_count = 0;
    int else_count = 0;
    int in_else = 0;
    int status = EXIT_FAILURE;
    char* condition = trim_whitespace(first_line) + 2;
    char* owned_condition = NULL;

    if (*condition == '\0') {
        owned_condition = read_block_line();
        if (owned_condition == NULL) {
            fprintf(stderr, "if: unexpected end of input\n");
            return EXIT_FAILURE;
        }
        condition = owned_condition;
    }

    char* then_line = read_block_line();
    if (then_line == NULL || !is_keyword(then_line, "then")) {
        fprintf(stderr, "if: expected 'then'\n");
        free(then_line);
        free(owned_condition);
        return EXIT_FAILURE;
    }
    free(then_line);

    char* line;
    while ((line = read_block_line()) != NULL) {
        char* trimmed = trim_whitespace(line);
        if (is_keyword(trimmed, "fi")) {
            free(line);
            status = execute_if_block(condition, then_lines, then_count,
                                      else_lines, else_count);
            break;
        }
        if (is_keyword(trimmed, "else")) {
            in_else = 1;
            free(line);
            continue;
        }

        char** target = in_else ? else_lines : then_lines;
        int* count = in_else ? &else_count : &then_count;
        if (*count >= MAX_IF_LINES) {
            fprintf(stderr, "if: too many commands in block\n");
            free(line);
            status = EXIT_FAILURE;
            break;
        }
        target[(*count)++] = line;
    }

    if (line == NULL) {
        fprintf(stderr, "if: expected 'fi'\n");
        status = EXIT_FAILURE;
    }

    for (int i = 0; i < then_count; i++) {
        free(then_lines[i]);
    }
    for (int i = 0; i < else_count; i++) {
        free(else_lines[i]);
    }
    free(owned_condition);
    return status;
}

int main(void) {
    char* cmdline;

    rl_attempted_completion_function = complete_input;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        reap_background_jobs();
        record_readline_history(cmdline);

        if (cmdline[0] == '!') {
            char* recalled_cmdline = recall_history(cmdline);
            free(cmdline);
            cmdline = recalled_cmdline;
            if (cmdline == NULL) {
                continue;
            }
        }

        char* trimmed = trim_whitespace(cmdline);
        if (strncmp(trimmed, "if", 2) == 0 &&
            (trimmed[2] == '\0' || isspace((unsigned char)trimmed[2]))) {
            read_and_execute_if(trimmed);
        } else {
            execute_line(cmdline);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
