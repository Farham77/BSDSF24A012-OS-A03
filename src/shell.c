#include "shell.h"

static char* command_history[HISTORY_SIZE];
static int history_count;
static int history_start;

static char* copy_string(const char* value) {
    size_t length = strlen(value) + 1;
    char* copy = malloc(length);

    if (copy == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memcpy(copy, value, length);
    return copy;
}

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

void add_history_entry(const char* cmdline) {
    int index;

    if (cmdline == NULL || cmdline[0] == '\0') {
        return;
    }

    if (history_count < HISTORY_SIZE) {
        index = (history_start + history_count) % HISTORY_SIZE;
        history_count++;
    } else {
        index = history_start;
        history_start = (history_start + 1) % HISTORY_SIZE;
        free(command_history[index]);
    }

    command_history[index] = copy_string(cmdline);
}

char* recall_history(const char* cmdline) {
    char* end;
    long requested_number;
    int index;

    if (cmdline == NULL || cmdline[0] != '!') {
        return NULL;
    }

    if (cmdline[1] == '\0') {
        fprintf(stderr, "history: expected a command number after '!'\n");
        return NULL;
    }

    requested_number = strtol(cmdline + 1, &end, 10);
    if (*end != '\0' || requested_number < 1 ||
        requested_number > history_count) {
        fprintf(stderr, "history: command number out of range\n");
        return NULL;
    }

    index = (history_start + (int)requested_number - 1) % HISTORY_SIZE;
    return copy_string(command_history[index]);
}

int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing operand\n");
        } else if (chdir(arglist[1]) == -1) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <directory>  Change the current working directory\n");
        printf("  exit            Exit the shell\n");
        printf("  help            Display this help message\n");
        printf("  history         Display command history\n");
        printf("  jobs            List background jobs\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            int index = (history_start + i) % HISTORY_SIZE;
            printf("%d %s\n", i + 1, command_history[index]);
        }
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;
}