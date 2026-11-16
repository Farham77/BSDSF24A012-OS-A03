#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <readline/history.h>
#include <readline/readline.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define HISTORY_SIZE 20
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** complete_input(const char* text, int start, int end);
char** tokenize(char* cmdline);
void add_history_entry(const char* cmdline);
char* recall_history(const char* cmdline);
int handle_builtin(char** arglist);
int execute(char** arglist);

#endif // SHELL_H
