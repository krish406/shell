/*
Name: Krish Shah
UID: U7092-5759
This program allows the user to interact with a simple shell called rush. Rush can execute commands stored
in the user's bin such as echo. It also has builtin commands such as such as path, cd, and exit. Rush is also able
to write the output of user commands into output streams other than stdout through redirection. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>

//helper functions
size_t get_tokens(char* tokens[], char* buffer, char* delim);
char* build_path(char* dir, char* cmd, char* error_msg);
int check_redirect(char* tokens[]);

int main(int argc, char* argv[]){
    char error_message[30] = "An error has occurred\n";

    //rush should not be run with extra arguments
    if(argc > 1){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        exit(1);
    }


    int status = 1; //loop control
    size_t buffsize = 256; //max input sizze
    int input; //size of user input
    char* buffer; //holds user input
    char* delim = " \t"; //separates input 
    char* tokens[256]; //holds separated input
    char** dir = malloc(sizeof(char*) * buffsize); //for initialization of path
    dir[0] = "/bin";
    dir[1] = "/usr/bin";
    dir[2] = NULL;

    //holds input
    buffer = (char *)malloc(buffsize * sizeof(char));
    
    if (buffer == NULL) {
        printf("Allocation Failed");
        free(buffer);
        exit(1);
    }
    
    //parent loop
    while(status){
        printf("rush> ");
        fflush(stdout); 

        input = getline(&buffer, &buffsize, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; //terminate buffer with null character

        size_t token_count = get_tokens(tokens, buffer, delim);

        //all whitespace if no tokens obtained
        if(token_count == 0){
            continue;
        }

        //exit builtin
        if((strcmp(tokens[0], "exit") == 0)){
            //exit should have no extra args
            if(token_count != 1){ 
                write(STDERR_FILENO, error_message, strlen(error_message)); 
                continue;
            }
            else {
                exit(0);
            }
        }

        //cd builtin
        if((strcmp(tokens[0], "cd") == 0)){
            //cd should only have one extra arg
            if(token_count != 2){
                write(STDERR_FILENO, error_message, strlen(error_message)); 
            }
            else {
                int chret = chdir(tokens[1]);
                if(chret != 0){ //error handling
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                }
            }
            continue;
        }
        
        //path builtin
        if((strcmp(tokens[0], "path") == 0)){
            //overwrite current path with tmp
            char** tmp = realloc(dir, (token_count - 1) * sizeof(char *));
            if (tmp == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }

            else{
                dir = tmp;
                int pos = 0;
                for(int i = 1; i < token_count; i++){
                    tmp[pos] = strdup(tokens[i]);
                    pos++;
                }
                tmp[pos] = NULL;
            }

            continue;
        }
        
        int pid = fork();
        
        if(pid == 0){
            int redirect_idx = check_redirect(tokens);
            int redirect_flag = 0;

            //if we actually detected an arrow
            if(redirect_idx != -1){
                redirect_flag = 1;
            }

            if(redirect_flag == 1){
                //cant have redirection if no previous argument
                if(redirect_idx == 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }

                //cant have redirection if no output file or if another arrow is in the place of the outfile
                else if(tokens[redirect_idx + 1] == NULL || strcmp(tokens[redirect_idx + 1], ">") == 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }

                //if there are more than two args after the redirection it is invalid
                else if(tokens[redirect_idx + 2] != NULL){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
            }

            if(redirect_flag == 1){
                char* outfile = tokens[redirect_idx + 1];
                tokens[redirect_idx] = NULL;
                close(STDOUT_FILENO);
                int fd = open(outfile, O_CREAT|O_WRONLY|O_TRUNC,S_IRWXU);
                if(fd == -1){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
            }

            char* cmd = tokens[0];
            char* path = build_path(dir[0], cmd, error_message);
            int check_access = access(path, X_OK);
            int found_flag = 0;

            //check all paths to see if execv will work on any of them
            for(int i = 0; dir[i] != NULL; i++){
                char* path = build_path(dir[i], cmd, error_message);
                
                int check_access = access(path, X_OK);

                //success
                if(check_access == 0){
                    found_flag = 1; 
                    tokens[0] = path; //for gradescope print
                    int err = execv(path, tokens);
                    if(err == -1){
                        write(STDERR_FILENO, error_message, strlen(error_message)); 
                        free(path);
                        exit(1);
                    }
                }

                free(path); //free path just allocated
            }

            if(found_flag == 0){
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
        }

        else if(pid == -1){
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            exit(1);
        }

        else{
            wait(NULL);
        }
    }

    free(buffer);
    return 0;
}

//separates the buffer into tokens based on the delimeters
size_t get_tokens(char* tokens[], char* buffer, char* delim){
    int token_count = 0;
    char* token;
    while((token = strsep(&buffer, delim)) != NULL){
        //this handles whitespace as strsep will convert whitespace to null
        if(*token == '\0'){
            continue;
        }
        tokens[token_count] = token;
        token_count++;
    }
    tokens[token_count] = NULL; 
    return token_count;
}

//finds first instance of arrow in redirection commands
int check_redirect(char* tokens[]){
    int arrow_index = -1;
    int iterator = 0;
    while(tokens[iterator] != NULL){
        if(strcmp(tokens[iterator], ">") == 0){
            arrow_index = iterator;
            break;
        }
        iterator++;
    }

    return arrow_index;
}

//creates a path using the command and directory
char* build_path(char* dir, char* cmd, char* error_msg){
    char* path = malloc(strlen(dir) + strlen(cmd) + 2);
    if (path == NULL) {
        write(STDERR_FILENO, error_msg, strlen(error_msg)); 
        exit(1);
    }
    strcpy(path,dir);
    strcat(path, "/");
    strcat(path, cmd);
    return path;
}