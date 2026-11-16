#include "shell.h"
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

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
    (void)fp;
    return readline(prompt);
}

static int add_completion(char*** matches, size_t* count,
                          const char* text, const char* value) {
    for (size_t i = 0; i < *count; i++) {
        if (strcmp((*matches)[i], value) == 0) {
            return 1;
        }
    }

    char** resized = realloc(*matches, sizeof(char*) * (*count + 2));
    if (resized == NULL) {
        perror("realloc");
        return 0;
    }

    resized[*count] = copy_string(value);
    resized[*count + 1] = NULL;
    *matches = resized;
    (*count)++;
    (void)text;
    return 1;
}

char** complete_input(const char* text, int start, int end) {
    char** matches = NULL;
    size_t count = 0;
    (void)end;

    if (start == 0) {
        const char* path = getenv("PATH");
        if (path != NULL) {
            char* path_copy = copy_string(path);
            char* directory = strtok(path_copy, ":");

            while (directory != NULL) {
                DIR* dir = opendir(directory[0] == '\0' ? "." : directory);
                if (dir != NULL) {
                    struct dirent* entry;
                    while ((entry = readdir(dir)) != NULL) {
                        char candidate[PATH_MAX];
                        int written = snprintf(candidate, sizeof(candidate),
                                                "%s/%s", directory,
                                                entry->d_name);
                        if (written >= 0 && (size_t)written < sizeof(candidate) &&
                            strncmp(entry->d_name, text, strlen(text)) == 0 &&
                            access(candidate, X_OK) == 0) {
                            add_completion(&matches, &count, text,
                                           entry->d_name);
                        }
                    }
                    closedir(dir);
                }
                directory = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    DIR* current_directory = opendir(".");
    if (current_directory != NULL) {
        struct dirent* entry;
        while ((entry = readdir(current_directory)) != NULL) {
            if (strncmp(entry->d_name, text, strlen(text)) == 0) {
                add_completion(&matches, &count, text, entry->d_name);
            }
        }
        closedir(current_directory);
    }

    return matches;
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
        if (*cp == '<' || *cp == '>' || *cp == '|') {
            len = 1;
            cp++;
        } else {
            len = 0;
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
                   *cp != '<' && *cp != '>' && *cp != '|') {
                cp++;
                len++;
            }
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