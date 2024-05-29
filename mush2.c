#include "mush2.h"

void io_redirect(struct clstage stage, bool verbose) {
    if (verbose) printf("----------\nio_redirect\n----------\n");
    if (stage.inname != NULL) {
            
        if (verbose) printf("redirecting in to %s\n", stage.inname);

        int in_fd = open(stage.inname, O_RDONLY);
        if (in_fd == -1) {
           perror("open");
        }

        if (dup2(in_fd ,STDIN_FILENO) == -1) {
            perror("dup2");
        }
        if (close(in_fd) == -1) {
            perror("close");
        }
    }
    if(stage.outname != NULL) {
            
        if(verbose) printf("redirecting out to %s\n", stage.outname);

        int out_fd = open(stage.outname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (out_fd == -1) {
            perror("open");
        }

        if (dup2(out_fd ,STDOUT_FILENO) == -1) {
            perror("dup2");
        }
        if (close(out_fd) == -1) {
            perror("close");
        }
    }
 
}

void handle_sigint(int signum) {
    /* do nothing */
}

void run_command(struct clstage stage, bool verbose) {
    int this_argc = stage.argc;
    char **this_argv = stage.argv;

    if(verbose) printf("argc: %d\n", this_argc);

    if(verbose) printf("command: %s\n", this_argv[0]);

    io_redirect(stage, verbose);

    if (execvp(this_argv[0], &this_argv[0]) == -1) {
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

void create_pipeline(int length, struct clstage *stage, bool verbose) {
    int pipes[length - 1][2];  /* Array of file descriptors for pipes */
    int i, j;

    /* Create pipes for each stage */
    for (i = 0; i < length - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
        }
    }

    pid_t pids[length];  /* Array to store child pids */
    /* fork children */
    for(i = 0; i < length; i++) {
        pid_t pid = fork();
        
        if (pid < 0) { /* failed */
            perror("fork");
            exit(EXIT_FAILURE);
        } else if(pid == 0) { /* child */
            signal(SIGINT, SIG_DFL);
            if (i > 0) {
                /* if command needs input (not first) */
                if (dup2(pipes[i - 1][0], STDIN) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            if (i < length - 1) {
                /* if command needs output (not last) */
                if (dup2(pipes[i][1], STDOUT) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            /* Close all pipe file descriptors */
            for (j = 0; j < length - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
        
            run_command(stage[i], verbose);
            exit(EXIT_SUCCESS);        
        } else { /* parent */
            pids[i] = pid;
        }
    }

    /* PARENT */

    signal(SIGINT, handle_sigint);

    /* Close all pipe file descriptors in the parent process */
    for (j = 0; j < length - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    for (j = 0; j < length; j++) { /* wait for children */
        int status;
        if (waitpid(pids[j], &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                if (verbose) printf("Process %d succeeded.\n", pids[i]);
            } else {
                if (verbose) printf("Process %d exited with an error value.\n", pids[i]);
            }
        }
    }   
}

void run(FILE *in, bool verbose, bool print_prompt) {
    while (!feof(in)) {
        if (print_prompt) printf("8-P ");
        char *line = readLongString(in);
        if (line == NULL) {
            printf("\n");
            return;
        }
        if (strlen(line) == 0) continue;
        pipeline pipe = crack_pipeline(line);

        if(pipe == NULL) continue;

        if (strcmp(pipe->stage->argv[0], "cd") == 0) {
            if(pipe->stage->argc == 1) {
                const char* home_dir = getenv("HOME");
                if (home_dir == NULL) {
                    perror("getenv");
                    continue;
                }
                if (verbose) printf("changing directory to %s\n", home_dir);
                if ((chdir(home_dir)) != 0) {
                    perror("chdir");
                }
            } else {
                if (verbose) printf("changing directory to %s\n", pipe->stage->argv[1]);
                if((chdir(pipe->stage->argv[1])) != 0) {
                    perror("chdir");
                }
            }
            continue;
        }

        if (verbose) printf("pipe length: %d\n", pipe->length);

        create_pipeline(pipe->length, pipe->stage, verbose);

    }
}

int main(int argc, char *argv[]) {
    FILE *in = stdin;
    int opt;
    bool verbose = false;
    bool print_prompt = true;


    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            default:
                break;
        }
    }

    if(optind == argc - 1) { /* alternate in provided */
        if (access(argv[optind], F_OK) == -1) {
            fprintf(stderr, "%s: No such file or directory\n", argv[optind]);
            exit(EXIT_FAILURE);
        }
        if ((in = fopen(argv[optind], "r")) == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", argv[optind]);
            exit(EXIT_FAILURE);
        }
    } else if(optind != argc) { /* too many arguments */
        printf("usage: ./mush2 [infile]\n");
        exit(EXIT_FAILURE);
    }

    if(verbose) {
        printf("in: %s\n", (in == stdin) ? "stdin" : argv[argc - 1]);
    }

    if (!isatty(fileno(in)) || !isatty(fileno(stdout))) {
        print_prompt = true;
    }

    run(in, verbose, print_prompt);

    if (in != stdin) {
        fclose(in);
    }

    return 0;
}
