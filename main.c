#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int path_count = 0;
int path_max_size = 1;
char** path;

void error_handle(const char* error_message) {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(0);
}

void execute_command(char* command) {
    char* sep;
    while ((sep = strsep(&command, " ")) != NULL) {

        // standard commands
        if (strcmp(sep, "exit") == 0) {
            exit(0);
        }
        else if (strcmp(sep, "cd") == 0) {
            char* arg = strsep(&command, " ");
            if (arg == NULL) {
                error_handle("Command cd is missing an argument\n");
            }
            chdir(arg);
            return;
        }
        else if (strcmp(sep, "path") == 0) {
            char* arg;
            while ((arg = strsep(&command, " ")) != NULL) {
                if (path_count >= path_max_size) {
                    path_max_size += 5;
                    path = realloc(path, path_max_size * sizeof(char*));
                    if (path == NULL) {
                        error_handle("Error while reallocating path variable\n");
                    }
                }

                path[path_count] = arg;
                path_count++;
            }
            return;
        }

        // non-standard commands
        for (int i = 0; i < path_count; i++) {

            char file_path[1024];
            strcpy(file_path, path[i]);
            strcat(file_path, "/");
            strcat(file_path, sep);

            int result = access(file_path, X_OK);
            if (result != 0) {
                continue;
            }


            pid_t pid = fork();
            if (pid < 0) {
                error_handle("Error while forking!\n");
            }
            else if (pid == 0) {
                char *myargs[3];
                myargs[0] = strdup(file_path);
                myargs[1] = strdup("main.c");
                myargs[2] = NULL;
                execv(myargs[0], myargs);
                error_handle("Error while executing the specified command!\n");
            }
            else {
                wait(NULL);
            }
            return;
        }

        error_handle("Error: command not found\n");

    }
}

void operation_loop(FILE* file) {
    char* buffer = NULL;
    size_t len = 0;

    size_t op;
    printf("wish> ");
    while ((op = getline(&buffer, &len, file)) != -1) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (file != stdin)
            printf("%s\n", buffer);

        execute_command(buffer);
        printf("wish> ");
    }
    printf("exit");

    free(buffer);
}



int main(int argc, char **argv) {
    path = malloc(path_max_size * sizeof(char*));
    if (path == NULL) {
        error_handle("Error while allocating memory\n");
    }
    path[path_count] = "/bin";
    path_count++;

    if (argc == 1) {
        operation_loop(stdin);
    }
    if (argc == 2) {
        FILE* file = fopen(argv[1], "r");
        if (!file) {
            error_handle("Error while opening file\n");
        }
        operation_loop(file);
        fclose(file);
    }

    return 0;
}

