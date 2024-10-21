#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_PATHS 100

// Function to print error message
void print_error(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

int main(int argc, char *argv[]) {

    if(argc > 2) {
        print_error();
        exit(1);
    }

    FILE *input = stdin;

    if(argc == 2) {
        input = fopen(argv[1], "r");
        if(!input){
            print_error();
            exit(1);
        }
    }

    char *line = NULL;
    size_t len = 0;
    char *paths[MAX_PATHS] = { "/bin", NULL };

    while(1) {
        if(argc == 1){
            printf("wish> ");
            fflush(stdout);
        }
        if(getline(&line, &len, input) == -1){
            break;
        }

        line[strcspn(line, "\n")] = 0;

        if(strlen(line) == 0){
            continue;
        }

        // Split the line into parallel commands
        char *parallel_commands[100];
        int parallel_count = 0;
        char *parallel_token = strtok(line, "&");
        while(parallel_token != NULL){
            parallel_commands[parallel_count++] = parallel_token;
            parallel_token = strtok(NULL, "&");
        }
        parallel_commands[parallel_count] = NULL;

        pid_t child_pids[parallel_count];
        int child_index = 0;

        for(int i = 0; i < parallel_count; i++){
            // Trim whitespace
            char *cmd = parallel_commands[i];
            while(*cmd == ' ') cmd++;
            char *end = cmd + strlen(cmd) - 1;
            while(end > cmd && *end == ' ') end--;
            *(end + 1) = '\0';

            // Handle redirection (before tokenization)
            char *redirect_pos = strchr(cmd, '>');
            int fd = -1;
            if (redirect_pos != NULL) {

                if(cmd == redirect_pos){
                    print_error();
                    continue;
                }
                *redirect_pos = '\0';  
                redirect_pos++;       

                if (strchr(redirect_pos, '>') != NULL){
                    print_error();
                    continue;
                }
                // Trim leading spaces in the file name
                while(*redirect_pos == ' ') redirect_pos++;

                if(strlen(redirect_pos) == 0 || strchr(redirect_pos, ' ') != NULL){
                    print_error();
                    continue;
                }

                // Open file for redirection
                fd = open(redirect_pos, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                if(fd < 0) {
                    print_error();
                    continue;
                }
            }

            // Tokenize the command
            char *tokens[100];
            int token_count = 0;
            char *token = strtok(cmd, " ");
            while (token != NULL) {
                tokens[token_count++] = token;
                token = strtok(NULL, " ");
            }
            tokens[token_count] = NULL;

            if(token_count == 0) {
                continue;
            }

            // Built-in commands
            if (strcmp(tokens[0], "exit") == 0){
                if(token_count != 1) {
                    print_error();
                    continue;
                }
                exit(0);
            } else if(strcmp(tokens[0], "cd") == 0) {
                if(token_count != 2) {
                    print_error();
                    continue;
                }
                if (chdir(tokens[1]) != 0) {
                    print_error();
                }
                continue;
            } else if (strcmp(tokens[0], "path") == 0){
                for (int j = 0; j < MAX_PATHS; j++){
                    paths[j] = NULL;
                }

                for (int j = 1; j < token_count; j++) {
                    if(j - 1 < MAX_PATHS){
                        paths[j - 1] = strdup(tokens[j]);
                    }
                }
                continue;
            }

            // Check if paths are empty
            if (paths[0] == NULL) {
                print_error();
                if(fd != -1) close(fd);
                continue;
            }

            // Search for command in paths
            char command[256];
            int found = 0;
            for(int p = 0; paths[p] != NULL; p++){
                snprintf(command, sizeof(command), "%s/%s", paths[p], tokens[0]);
                if(access(command, X_OK) == 0) {
                    found = 1;
                    break;
                }
            }

            if(!found){
                print_error();
                if (fd != -1) close(fd);
                continue;
            }

            // Fork and execute the command
            pid_t pid = fork();
            if(pid < 0){
                print_error();
                exit(1);
            } else if(pid == 0){
                // Child process
                if(fd != -1){
                    if (dup2(fd, STDOUT_FILENO) == -1){ 
                        print_error();
                        exit(1);
                    }
                    if(dup2(fd, STDERR_FILENO) == -1){
                        print_error();
                        exit(1);
                    }
                    close(fd);
                }
                execv(command, tokens);
                print_error();
                exit(1);
            } else {
                // Parent process
                if (fd != -1){
                    close(fd);
                }

                child_pids[child_index++] = pid;
            }
        }

        // Wait for all child processes to complete before proceeding
        for (int i = 0; i < child_index; i++){
            int status;
            waitpid(child_pids[i], &status, 0);
        }

        fflush(stdout);
    }

    free(line);
    if(argc == 2){
        fclose(input);
    }

    return 0;
}
