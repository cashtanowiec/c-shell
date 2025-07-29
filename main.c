#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


int path_count = 0;
int path_max_size = 1;
char** path;

void error_handle(const char* error_message) {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(0);
}

// meant for executing non-standard commands
int nonstandard_command_execute(char* command, char* sep) {
    for (int i = 0; i < path_count; i++) {

        char file_path[1024];
        strcpy(file_path, path[i]);
        strcat(file_path, "/");
        strcat(file_path, sep);

        int result = access(file_path, X_OK);
        if (result != 0) {
            continue;
        }

        int arguments_count = 0;
        int arguments_max_count = 1;
        char** arguments = malloc(arguments_max_count * sizeof(char*));
        if (arguments == NULL) {
            error_handle("Error while allocating memory");
        }
        char* arg;
        while ((arg = strsep(&command, " ")) != NULL) {

            if (arguments_count >= arguments_max_count) {
                arguments_max_count += 5;
                arguments = realloc(arguments, arguments_max_count * sizeof(char*));
                if (arguments == NULL) {
                    error_handle("Error while allocating memory");
                }
            }

            arguments[arguments_count] = arg;
            arguments_count++;
        }

        const pid_t pid = fork();
        if (pid < 0) {
            error_handle("Error while forking!\n");
        }
        else if (pid == 0) {
            char *myargs[2 + arguments_count];
            myargs[0] = strdup(file_path);

            for (int j = 0; j < arguments_count; j++) {
                myargs[1+j] = arguments[j];
            }
            myargs[1+arguments_count] = NULL;
            execv(myargs[0], myargs);

            error_handle("Error while executing the specified command!\n");
        }
        else {
            wait(NULL);
            printf("\n");
            return 0;
        }
    }
    return 1;
}

// meant for executing built in commands
int standard_command_execute(char* command, char* sep) {

    if (strcmp(sep, "exit") == 0) {
        exit(0);
    }
    else if (strcmp(sep, "cd") == 0) {
        char* arg = strsep(&command, " ");
        if (arg == NULL) {
            error_handle("Command cd is missing an argument\n");
        }
        chdir(arg);
        return 0;
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
        return 0;
    }

    return 1;
}

// meant for parsing text
void execute_command(char* command) {

    // std redirect
    char* std_redirect_buffer = malloc(1024 * sizeof(char));
    strcpy(std_redirect_buffer, command);
    char* std_redirect_sep;

    std_redirect_sep = strstr(std_redirect_buffer, " > ");
    if (std_redirect_sep != NULL) {
        std_redirect_sep += 3;
        std_redirect_sep = strsep(&std_redirect_sep, " ");
    }

    // error redirect
    char* err_redirect_buffer = malloc(1024 * sizeof(char));
    strcpy(err_redirect_buffer, command);
    char* err_redirect_sep;

    err_redirect_sep = strstr(err_redirect_buffer, " 2> ");
    if (err_redirect_sep != NULL) {
        err_redirect_sep += 4;
        err_redirect_sep = strsep(&err_redirect_sep, " ");
    }
    printf("%s", err_redirect_sep);

    char* sep;
    while ((sep = strsep(&command, " ")) != NULL) {

        // delete redirects from OG command
        if (command != NULL) {
            char modified_command[1024];
            strcpy(modified_command, command);
            for (int i = 0; i < strlen(modified_command); i++) {
                if (i >= 2 && modified_command[i] == '>' && modified_command[i-1] == '2') {
                    modified_command[i-2] = '\0';
                    break;
                }
                else if (i >= 1 && modified_command[i] == '>' && modified_command[i-1] == ' ') {
                    modified_command[i-1] = '\0';
                    break;
                }
            }
            strcpy(command, modified_command);
        }


        // change output if necessary
        int std_file = -1;
        int saved_std = dup(STDOUT_FILENO);

        int err_file = -1;
        int saved_err = dup(STDERR_FILENO);

        if (std_redirect_sep != NULL) {
            std_file = open(std_redirect_sep, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (std_file < 0) {
                error_handle("Error while opening file for stdout!");
            }
            dup2(std_file, STDOUT_FILENO);
            close(std_file);
        }
        if (err_redirect_sep != NULL) {

            err_file = open(err_redirect_sep, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (err_file < 0) {
                error_handle("Error while opening file for stderr!");
            }
            dup2(err_file, STDERR_FILENO);
            close(err_file);
        }

        // standard commands
        if (standard_command_execute(command, sep) == 0) {
            dup2(saved_std, STDOUT_FILENO);
            close(saved_std);

            dup2(saved_err, STDERR_FILENO);
            close(saved_err);
            return;
        }

        // non-standard commands
        if (nonstandard_command_execute(command, sep) == 0) {
            dup2(saved_std, STDOUT_FILENO);
            close(saved_std);

            dup2(saved_err, STDERR_FILENO);
            close(saved_err);
            return;
        }

        dup2(saved_std, STDOUT_FILENO);
        close(saved_std);

        dup2(saved_err, STDERR_FILENO);
        close(saved_err);
        error_handle("Error: command not found\n");
    }
}

// standard loop which acts as CLI
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

