#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_BUFFER 1024                        // max line buffer
#define MAX_ARGS 64                            // max # args
#define SEPARATORS " \t\n"                     // token sparators
#define OUTPUT_TRUNCATE 1
#define OUTPUT_APPEND 2

extern char **environ;

char *getcurrdir(void) {
  char *s = (char *)malloc(sizeof(char)*MAX_BUFFER);
  int maxsize = MAX_BUFFER;
  while (!getcwd(s, maxsize)) {
    free(s);
    maxsize = maxsize*2;
    s = (char *)malloc(sizeof(char)*maxsize);
  }
  return s;
}

void truncargs(char **args) {
  int i;
  int set_null = 0;
//  char **newargs = (char **)malloc(sizeof(char *)*MAX_ARGS);
  for (i=0; i<MAX_ARGS; ++i) {
    if (args[i]) {
      if (set_null == 1) {
        args[i] = NULL;
        continue;
      }
      if (!strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
        set_null = 1;
        args[i] = NULL;
//        break;
      }
//      newargs[i] = (char *)malloc(strlen(args[i])+1);
//      strcpy(newargs[i], args[i]);
    }
    else 
      break;
  }
//  return newargs;
}

void forkcmd(char **args, int waitflag, char *in, char *out, int outputflag) {
  int pid;
  int status;
  int in_fd, out_fd;
       if (in || out) { // if we have at least one kind of redirection
        if (in) { // if there's a file we want to use as input
          if (!(access(in, F_OK) == 0)) { // if the file doesn't exist
            printf("input file does not exist\n");
            exit(-1);
          }
          if (!(access(in, R_OK) == 0)) { // if the file cannot be read
            printf("cannot read input file\n");
            exit(-1);
          }
          in_fd = open(in, O_RDONLY);
        }
        if (out) { // there's a file we want to use as an output destination
          if (access(out, F_OK) == 0) {
            if (!(access(out, W_OK) == 0)) { // if the file exists but we can't write to it
              printf("cannot write to output: permission denied\n");
              exit(-1);
            }
          }
          // replace stdout with the appropriate file descriptor depending on the mode
          if (outputflag == OUTPUT_TRUNCATE) {
            out_fd = open(out, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
          }
          if (outputflag == OUTPUT_APPEND) {
            out_fd = open(out, O_APPEND | O_RDWR | O_CREAT, S_IRWXU);
         }
        }
//        char **newargs = truncargs(args);
//        args = newargs;
        truncargs(args);
      }
 switch (pid = fork()) {
    case -1:
      printf("an error occurred while attempting to create a child process. exiting.\n");
      exit(-1);
    case 0: // successful fork, inside child process
      if (in) {
        dup2(in_fd, STDIN_FILENO);
      }
      if (out) {
        dup2(out_fd, STDOUT_FILENO);
      }
      execvp(args[0], args);
      printf("there was an error executing your command.\n"); // if it executes this, then there's an error
      exit(-1);
    default:
      if (waitflag) {
        waitpid(pid, &status, WUNTRACED);
      }
      close(in_fd);
      close(out_fd);
  }
}

int checkamp(char **args) {
  int i = 0;
  while (i < MAX_ARGS) {
    if (args[i]) {
      if (!strcmp(args[i], "&")) {
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

void printprompt(void) {
  /* the prompt is the current working directory */
  /* write the curr directory path and a $ sign for the prompt */ 
  char *prompt;
  char *currpath = getcurrdir();
  prompt = (char *)malloc(strlen(currpath)+sizeof("$ "));
  strcpy(prompt, currpath);
  strcat(prompt, "$ ");
  fputs(prompt, stdout);
  free(currpath);
}

int getfnames(char **args, char **infname, char **outfname, int *outmode) {
  int i;
  for (i=0; i<MAX_ARGS; ++i) {
    if (args[i]) {
      if (!strcmp(args[i], "<")) {
        *infname = (char *)malloc(strlen(args[i+1]));
        strcpy(*infname, args[i+1]);
        continue;
      }
      if (!strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
        *outfname = (char *)malloc(strlen(args[i+1]));
        strcpy(*outfname, args[i+1]);
        if (!strcmp(args[i], ">")) {
          *outmode = OUTPUT_TRUNCATE;
        }
        else {
          *outmode = OUTPUT_APPEND;
        }
        continue;
      }
    }
    else {
      break;
    }
  }
  if (*infname == NULL && *outfname == NULL) {
    return 0;
  }
  return 1;
}

void cd(char *dir) {
  if (dir) { // if we gave cd an argument, then change to that directory
    int ret = chdir(dir);
    if (ret == 0) { // if we successfully changed the directory, update PWD
      char *val = (char *)malloc(strlen(getcurrdir())+sizeof("PWD=")); // this should not be freed
      strcpy(val, "PWD="); // create the string for PWD
      strcat(val, getcurrdir());
      putenv(val); // set the PWD environment variable
      return;
    }
    printf("cd: error: the directory you provided does not exist\n");
    return;
  }
  // else, just print out the current working directory
  char *currdir = getcurrdir();
  printf("%s\n", currdir);
  free(currdir);
}

void dir2(char *d) {
  struct dirent **filelist = {0};
  int fcount = -1;
  char *directory;
  if (d) {
    directory = d;
  }
  else {
    directory = ".";
  }
  fcount = scandir(directory, &filelist, 0, alphasort);
  if (fcount < 0) {
    printf("dir: error accessing the specified directory\n");
    return;
  }
  int i = 0;
  for (i=0; i<fcount; ++i) {
    printf("%s\n", filelist[i]->d_name);
    free(filelist[i]);
  }
  free(filelist);
}

void dir(char *d) {
  char *cmd = "ls -al";
  if (d) {
    char *s = (char *)malloc(strlen(cmd)+strlen(d)+strlen(" "));
    strcpy(s, cmd);
    strcat(s, " ");
    strcat(s, d);
    system(s);
    free(s);
    return;
  }
  system(cmd);
}

void help(void) {
  char *cmd = "more -d readme";
  system(cmd);
}

void shellpause(void) {
  char *cmd = "read -r key";
  system(cmd);
}

void env(char **e) {
  char **env = e; // we need to introduce the temporary variable
  // env so that we don't modify the global variable environ
  // this is so that when we call environ again, it still works
  while (*env) {
    printf("%s\n",*env++);
  }
}

void echo(char **s) {
  int i = 1;
  if (s[i]) {
    printf(s[i]);
  }
  while (s[i]) {
    ++i;
    printf(" ");
    printf(s[i]);
  }
  printf("\n");
}

int main(int argc, char **argv) {
  char buf[MAX_BUFFER]; // line buffer
  char *args[MAX_ARGS]; // pointers to arg strings
  char **arg; // working pointer thru args
/* keep reading input until "quit" command or eof of redirected input */

  while (!feof(stdin)) { 
/* get command line from input */

// default value is 1 because usually you want sequential execution
// assume this for every new line input
    int waitflag = 1;
    int outputflag = 0;

    printprompt(); // print the prompt for the shell

    // placeholders for i/o redirection
    char *infile = NULL;
    char *outfile = NULL;

    if (fgets(buf, MAX_BUFFER, stdin)) { // read a line into buf
      arg = args;
      *arg++ = strtok(buf, SEPARATORS); // get the first token of the input
      while ((*arg++ = strtok(NULL, SEPARATORS))); // read the rest of the line

      if (args[0]) { // if there's anything there
        if (checkamp(args)) {
          waitflag = 0;
        }
        getfnames(args, &infile, &outfile, &outputflag);
        printf("infile: %s\n", infile);
        printf("outfile: %s\n", outfile);
        if (!strcmp(args[0], "clr")) {
          system("clear");
          continue;
        }
        if (!strcmp(args[0], "dir")) {
          dir(args[1]);
          continue;
        }
        if (!strcmp(args[0], "environ")) {
          env(environ);
          continue;
        }
        if (!strcmp(args[0], "quit"))
          break;
        if (!strcmp(args[0], "cd")) {
          cd(args[1]);
          continue;
        }
        if (!strcmp(args[0], "help")) {
          help();
          continue;
        }
        if (!strcmp(args[0], "pause")) {
          shellpause();
          continue;
        }
        if (!strcmp(args[0], "echo")) {
          echo(args);
          continue;
        }
        // if the item matches none of the internal commands, then run the generic call
        forkcmd(args, waitflag, infile, outfile, outputflag);
      }
    }
  }
  return 0; 
}
