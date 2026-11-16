#include "shell.h"
#include <fcntl.h>

#define MAX_JOBS 64

struct background_job {
    pid_t pid;
    char* command;
};

static struct background_job background_jobs[MAX_JOBS];
static int background_job_count;

static int is_operator(const char* token) {
    return strcmp(token, "<") == 0 || strcmp(token, ">") == 0 ||
           strcmp(token, "|") == 0;
}

static int execute_stage(char* arglist[], int start, int end,
                         int input_fd, int output_fd) {
    char* command[MAXARGS + 1];
    int command_count = 0;
    int input_redirect = -1;
    int output_redirect = -1;

    for (int i = start; i < end; i++) {
        if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
            int* redirect_fd = strcmp(arglist[i], "<") == 0
                                   ? &input_redirect
                                   : &output_redirect;
            int flags = strcmp(arglist[i], "<") == 0
                            ? O_RDONLY
                            : O_WRONLY | O_CREAT | O_TRUNC;
            mode_t mode = strcmp(arglist[i], "<") == 0 ? 0 : 0644;

            if (i + 1 >= end || is_operator(arglist[i + 1])) {
                fprintf(stderr, "redirection: missing file operand\n");
                return -1;
            }

            *redirect_fd = open(arglist[++i], flags, mode);
            if (*redirect_fd == -1) {
                perror(arglist[i]);
                return -1;
            }
        } else {
            if (command_count >= MAXARGS) {
                fprintf(stderr, "command: too many arguments\n");
                return -1;
            }
            command[command_count++] = arglist[i];
        }
    }

    if (command_count == 0) {
        fprintf(stderr, "command: missing command\n");
        return -1;
    }
    command[command_count] = NULL;

    if (input_fd != STDIN_FILENO && dup2(input_fd, STDIN_FILENO) == -1) {
        perror("dup2");
        return -1;
    }
    if (output_fd != STDOUT_FILENO && dup2(output_fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        return -1;
    }
    if (input_redirect != -1 &&
        dup2(input_redirect, STDIN_FILENO) == -1) {
        perror("dup2");
        return -1;
    }
    if (output_redirect != -1 &&
        dup2(output_redirect, STDOUT_FILENO) == -1) {
        perror("dup2");
        return -1;
    }

    if (input_redirect != -1) close(input_redirect);
    if (output_redirect != -1) close(output_redirect);
    execvp(command[0], command);
    perror(command[0]);
    return -1;
}

int execute_command(char* arglist[], int background, const char* command_line) {
    int process_count = 0;
    int previous_input = STDIN_FILENO;
    int start = 0;
    pid_t children[MAXARGS];

    for (int i = 0;; i++) {
        if (arglist[i] == NULL || strcmp(arglist[i], "|") == 0) {
            if (arglist[i] == NULL && i == start) {
                fprintf(stderr, "pipe: missing command\n");
                return -1;
            }

            int pipe_fds[2] = {-1, -1};
            int output_fd = STDOUT_FILENO;
            if (arglist[i] != NULL && pipe(pipe_fds) == -1) {
                perror("pipe");
                return -1;
            }
            if (arglist[i] != NULL) {
                output_fd = pipe_fds[1];
            }

            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("fork");
                return -1;
            }
            if (child_pid == 0) {
                int result = execute_stage(arglist, start, i, previous_input,
                                           output_fd);
                _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
            }

            if (process_count >= MAXARGS) {
                fprintf(stderr, "shell: too many pipeline processes\n");
                return -1;
            }
            children[process_count++] = child_pid;
            if (previous_input != STDIN_FILENO) close(previous_input);
            if (output_fd != STDOUT_FILENO) close(output_fd);

            if (arglist[i] == NULL) {
                break;
            }
            previous_input = pipe_fds[0];
            start = i + 1;
        }
    }

    if (background) {
        for (int i = 0; i < process_count; i++) {
            if (background_job_count >= MAX_JOBS) {
                fprintf(stderr, "jobs: job table is full\n");
                break;
            }
            background_jobs[background_job_count].pid = children[i];
            background_jobs[background_job_count].command = strdup(command_line);
            if (background_jobs[background_job_count].command == NULL) {
                perror("strdup");
                return -1;
            }
            background_job_count++;
        }
        return 0;
    }

    for (int i = 0; i < process_count; i++) {
        if (waitpid(children[i], NULL, 0) == -1) {
            perror("waitpid");
            return -1;
        }
    }

    return 0;
}

int execute(char* arglist[]) {
    return execute_command(arglist, 0, arglist[0]);
}

void reap_background_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < background_job_count; i++) {
            if (background_jobs[i].pid == pid) {
                free(background_jobs[i].command);
                background_jobs[i] = background_jobs[background_job_count - 1];
                background_job_count--;
                break;
            }
        }
    }

    if (pid == -1 && errno != ECHILD) {
        perror("waitpid");
    }
}

void print_background_jobs(void) {
    reap_background_jobs();
    for (int i = 0; i < background_job_count; i++) {
        printf("[%d] %s\n", background_jobs[i].pid,
               background_jobs[i].command);
    }
}