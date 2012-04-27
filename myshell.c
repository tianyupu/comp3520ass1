/* #define _GNU_SOURCE 1 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_BUFFER 1024                        // max line buffer
#define MAX_ARGS 64                            // max # args
#define SEPARATORS " \t\n"                     // token sparators

extern char **environ;

char *getcurrdir() {
  char *s = (char *)malloc(MAX_BUFFER);
  int maxsize = MAX_BUFFER;
  while (!getcwd(s, maxsize)) {
    free(s);
    maxsize = maxsize*2;
    s = (char *)malloc(maxsize);
  }
  return s;
}

void forkcmd(char **args, int waitflag) {
  int pid = fork();
  int status;
  switch (pid) {
    case -1:
      printf("fork-error\n");
      exit(-1);
    case 0: // successful fork
      execvp(args[0], args);
      printf("exec-error\n"); // if it executes this, then there's an error
      exit(-1);
    default:
      if (waitflag) {
        waitpid(pid, &status, WUNTRACED);
      }
  }
}

int checkamp(char **args) {
  int i = 0;
  while (i < MAX_ARGS) {
    if (args[i]) {
      if (!strcmp(args[i],"&")) {
        args[i] = NULL;
        return 1;
      }
      i++;
    }
    else {
      return 0;
    }
  }
  return 0;
}

int getfnames(char **args, char **infname, char **outfname) {

  return 0;
}

int main(int argc, char **argv) {
  char buf[MAX_BUFFER];                      // line buffer
  char *args[MAX_ARGS];                     // pointers to arg strings
  char **arg;                               // working pointer thru args
  char *prompt;                    // shell prompt

/* keep reading input until "quit" command or eof of redirected input */

  while (!feof(stdin)) { 
/* get command line from input */

// default value is 1 because usually you want sequential execution
// assume this for every new line input
    int waitflag = 1;

// default value is 0 because you don't assume redirection until you find it
    int redirectflag = 0;

    /* the prompt is the current working directory */
    /* write the curr directory path and a $ sign for the prompt */
    char *currpath = getcurrdir();
    prompt = (char *)malloc(strlen(currpath)+sizeof("$ "));
    strcpy(prompt, currpath);
    strcat(prompt, "$ ");
    fputs(prompt, stdout);                // write prompt
    if (fgets(buf, MAX_BUFFER, stdin)) { // read a line

/* tokenize the input into args array */

/* strtok(buf,sep) gives the first token every time. to get the
 * rest of the tokens, use a while loop strtok(NULL,sep) */
      arg = args;
      *arg++ = strtok(buf,SEPARATORS);   // tokenize input
      while ((*arg++ = strtok(NULL,SEPARATORS))); // last entry will be NULL 

      if (args[0]) { // if there's anything there
        if (checkamp(args)) {
          waitflag = 0;
        }
/* check for internal/external command */
        if (!strcmp(args[0],"clear")) { // "clear" command
          system("clear");
          /* system(clear) is equiv to sh -c clear */
          /* exec() does NOT return to the calling function */
          continue;
        }
        if (!strcmp(args[0],"clr")) {
          system("clear");
          continue;
        }
        if (!strcmp(args[0],"dir")) {
          //char cmd[][] = {"ls", "-al"};
          /*
          if (args[1]) { // if we gave dir an argument
            char *s;
            s = (char *)malloc(strlen(cmd)+strlen(args[1])+1);
            strcpy(s,cmd);
            strcat(s,args[1]);
            system(s);
            free(s);
          }
          */
          // else we just execute the default
          //system(cmd);
          forkcmd(args, waitflag);
          continue;
        }
        if (!strcmp(args[0],"environ")) {
          char **env = environ; // we need to introduce the temporary variable
          // env so that we don't modify the global variable environ
          // this is so that when we call environ again, it still works
          while (*env) {
            printf("%s\n",*env++);
          }
          continue;
        }
        if (!strcmp(args[0],"quit"))   // "quit" command
          break;                     // break out of 'while' loop

        if (!strcmp(args[0],"cd")) {
          if (args[1]) { // if the path does not exist, print the current working directory
            chdir(args[1]);
            char *val = (char *)malloc(strlen(args[1])+strlen("PWD=")); // this should not be freed
            strcpy(val,"PWD=");
            strcat(val,args[1]);
            putenv(val);
            continue;
          }
          printf("%s\n", getcurrdir());
        }
        forkcmd(args, waitflag);
      }
    }
  }
  return 0; 
}
