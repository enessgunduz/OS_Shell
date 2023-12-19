#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 80
#define MAX_BOOKMARKS 10
#define MAX_PATH 256

// Function declarations
void setup(char inputBuffer[], char *args[], int *background);
void executeCommand(char *args[], int background);
void searchFiles(char *path, char *keyword, int recursive);
void searchInFile(char *filename, char *keyword);
int handleInternalCommands(char *args[]);
void handleIOredirection(char *args[]);
void handleBookmarkCommand(char *args[]);
void printBookmarks();

char *bookmarks[MAX_BOOKMARKS];
int numBookmarks = 0;
pid_t foregroundProcess = 0;

void setup(char inputBuffer[], char *args[], int *background) {
    printf("myshell: ");
    fflush(stdout);  // Flush the output buffer

    int length, i, start, ct;
    ct = 0;
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);
    start = -1;
    if (length == 0)
        exit(0);

    if ((length < 0) && (errno != EINTR)) {
        perror("error reading the command");
        exit(-1);
    }

    for (i = 0; i < length; i++) {
        switch (inputBuffer[i]) {
            case ' ':
            case '\t':
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                start = -1;
                break;
            case '\n':
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL;
                break;
            default:
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&') {
                    *background = 1;
                    inputBuffer[i - 1] = '\0';
                }
        }
    }
    args[ct] = NULL;
}

void executeCommand(char *args[], int background) {
    // Check for internal commands
    if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "search") == 0 || strcmp(args[0], "bookmark") == 0) {
        handleInternalCommands(args);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(args[0], args);
        // execvp failed
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        if (!background) {
            // Wait for the foreground process to complete
            foregroundProcess = pid;
            waitpid(pid, NULL, 0);
            foregroundProcess = 0;
        }
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}



int handleInternalCommands(char *args[]) {

    char *path = ".";

    if (strcmp(args[0], "^Z") == 0) {
        // Stop the currently running foreground process
        if (foregroundProcess != 0) {
            kill(foregroundProcess, SIGSTOP);
            printf("Foreground process stopped: %d\n", foregroundProcess);
        } else {
            printf("No foreground process to stop.\n");
        }
        return 1; // Internal command handled
    } else if (strcmp(args[0], "search") == 0) {
        if (args[1] != NULL) {
          if (strcmp(args[1], "-r") == 0) {
            if (args[2] != NULL) {
              printf("-- yes -r -----> %s\n", args[2]);
              searchFiles(".", args[2], 1);
            }
          }
          else {
            printf("-- no -r -----> %s\n", args[1]);
            searchFiles(".", args[1], 0);      
          }
        } else {
            printf("Usage: search <keyword>\n");
        }
        return 1; // Internal command handled
    } else if (strcmp(args[0], "bookmark") == 0) {
        handleBookmarkCommand(args);
        return 1; // Internal command handled
    } else if (strcmp(args[0], "exit") == 0) {
        // Terminate the shell process
        if (foregroundProcess == 0) {
            exit(0);
        } else {
            printf("Cannot exit while there are background processes running.\n");
        }
        return 1; // Internal command handled
    }
    return 0; // Not an internal command
}


void handleIOredirection(char *args[]) {
    int i;
    int inputRedirect = 0;
    int outputRedirect = 0;
    char *inputFile = NULL;
    char *outputFile = NULL;

    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            // Input redirection
            inputRedirect = 1;
            args[i] = NULL;
            inputFile = args[i + 1];
        } else if (strcmp(args[i], ">") == 0) {
            // Output redirection
            outputRedirect = 1;
            args[i] = NULL;
            outputFile = args[i + 1];
        } else if (strcmp(args[i], "2>") == 0) {
            // Error redirection
            outputRedirect = 1;
            args[i] = NULL;
            outputFile = args[i + 1];
        }
    }

    if (inputRedirect) {
        int fd = open(inputFile, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (outputRedirect) {
        int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

void handleBookmarkCommand(char *args[]) {
    if (args[1] != NULL) {
        if (strcmp(args[1], "-l") == 0) {
            printBookmarks();
        } else if (strcmp(args[1], "-i") == 0) {
            if (args[2] != NULL) {
                int index = atoi(args[2]);
                if (index >= 0 && index < numBookmarks) {
                    printf("Executing bookmark %d: %s\n", index, bookmarks[index]);
                    char *bookmarkArgs[MAX_LINE / 2 + 1];
                    int i;
                    for (i = 0; i <= MAX_LINE / 2; i++) {
                        bookmarkArgs[i] = NULL;
                    }
                    // Tokenize the bookmark command
                    char *token = strtok(bookmarks[index], " ");
                    i = 0;
                    while (token != NULL) {
                        bookmarkArgs[i++] = token;
                        token = strtok(NULL, " ");
                    }
                    executeCommand(bookmarkArgs, 0);
                } else {
                    printf("Invalid bookmark index.\n");
                }
            } else {
                printf("Usage: bookmark -i <index>\n");
            }
        } else if (strcmp(args[1], "-d") == 0) {
            if (args[2] != NULL) {
                int index = atoi(args[2]);
                if (index >= 0 && index < numBookmarks) {
                    printf("Deleting bookmark %d: %s\n", index, bookmarks[index]);
                    free(bookmarks[index]);
                    // Shift the remaining bookmarks up
                    for (int i = index; i < numBookmarks - 1; i++) {
                        bookmarks[i] = bookmarks[i + 1];
                    }
                    numBookmarks--;
                } else {
                    printf("Invalid bookmark index.\n");
                }
            } else {
                printf("Usage: bookmark -d <index>\n");
            }
        } else {
            // Add a new bookmark
            if (numBookmarks < MAX_BOOKMARKS) {
                bookmarks[numBookmarks] = strdup(args[1]);
                printf("Bookmark added: %s\n", bookmarks[numBookmarks]);
                numBookmarks++;
            } else {
                printf("Bookmark limit reached.\n");
            }
        }
    } else {
        printf("Usage: bookmark <command>\n");
    }
}

void printBookmarks() {
    for (int i = 0; i < numBookmarks; i++) {
        printf("%d \"%s\"\n", i, bookmarks[i]);
    }
}

void searchInFile(char *filename, char *keyword) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }

    char line[256];
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        // Search for the word in the line
        if (strstr(line, keyword) != NULL) {
            printf("%d:  '%s' -> %s\n", line_number, filename, line);
        }
    }

    fclose(file);
}

void searchFilesRecursive(char *path, char *keyword) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR) {
                // Ignore "." and ".." directories
                if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                    char new_path[255];
                    strcpy(new_path, path);
                    strcat(new_path, "/");
                    strcat(new_path, ent->d_name);
                    searchFiles(new_path, keyword, 1);
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
    }
}

void searchFiles(char *path, char *keyword, int recursive) {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG && ((strstr(ent->d_name, ".c") != NULL) || (strstr(ent->d_name, ".C") != NULL) || (strstr(ent->d_name, ".h") != NULL) || (strstr(ent->d_name, ".H") != NULL)) ) {
                // Construct the full path for the file
                char file_path[255];
                strcpy(file_path, path);
                strcat(file_path, "/");
                strcat(file_path, ent->d_name);
                searchInFile(file_path, keyword);
            } else if (recursive && ent->d_type == DT_DIR &&
                       strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                // Construct the full path for the subdirectory
                char sub_dir_path[255];
                strcpy(sub_dir_path, path);
                strcat(sub_dir_path, "/");
                strcat(sub_dir_path, ent->d_name);
                searchFilesRecursive(sub_dir_path, keyword);
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
    }
    
    if (recursive) {
    	searchFilesRecursive(path, keyword);
    }
}

int main(void) {
    char inputBuffer[MAX_LINE];
    int background;
    char *args[MAX_LINE / 2 + 1];
    while (1) {
        background = 0;
        setup(inputBuffer, args, &background);

        // Check for internal commands
        if (!handleInternalCommands(args)) {
            // If it's not an internal command, execute the command
            // Check for I/O redirection
            handleIOredirection(args);

            // Execute the command
            executeCommand(args, background);
        }
    }
    return 0;
}

void fooooo(){

}

void efe(){
	//-r
	//efe efe
}
