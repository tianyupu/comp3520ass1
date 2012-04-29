/* Author: Tianyu Pu
 * SID: 310182212
 * 
 * A simple shell in C. Features:
 *  - support for internal commands (see readme)
 *  - supports I/O redirection using <, > and >>
 *  - support for background execution using the & operator
 *  - supports fork/exec of external commands
 *  - keyboard interactive input
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_BUFFER 1024 // max size of the line buffer
#define MAX_ARGS 64 // max number of args
#define SEPARATORS " \t\n" // token sparators
#define OUTPUT_TRUNCATE 1 // flag for truncate output
#define OUTPUT_APPEND 2 // flag for append output

extern char **environ;

char *getcurrdir(void) {
  /* Gets the current working directory and returns the path in a string. */
  char *s = (char *)malloc(sizeof(char)*MAX_BUFFER); // allocate a line
  int maxsize = MAX_BUFFER;
  while (!getcwd(s, maxsize)) {
    // if we tried to get the current working directory path but it's too big
    free(s); // free the chunk we just allocated
    maxsize = maxsize*2; // double the size and allocate again
    s = (char *)malloc(sizeof(char)*maxsize);
  }
  return s;
}

void truncargs(char **args) {
  /* Truncates the arguments so that they can be executed in the child process. */
  int i;
  // flag to indicate whether we want to start setting things to null
  int set_null = 0; 
  for (i=0; i<MAX_ARGS; ++i) {
    if (args[i]) {
      if (set_null == 1) {
        args[i] = NULL;
        continue;
      }
      if (!strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
        // as soon as we see one of these operators, we know we only want to keep the
        // arguments that came before this operator
        set_null = 1; // switch the flag on
        args[i] = NULL;
      }
    }
    else { // it's already null, so no point setting it to null
      break;
    }
  }
}

void forkcmd(char **args, int waitflag, char *in, char *out, int outputflag) {
  /* Fork a child process and execute a set of arguments.
   *
   * Arguments:
   *  - args: the args we want to execute
   *  - waitflag: whether we want to have background execution or not
   *  - in: the input filename, if any (for stdin redirect)
   *  - out: the output filename, if any (for stdout redirect)
   *  - outputflag: a flag indicating what kind of output we want (truncate or append)
   */
  int pid;
  int status;
  int in_fd, out_fd;
  if (in || out) { // if we have at least one kind of redirection
    if (in) { // if there's a file we want to use as input
      in_fd = open(in, O_RDONLY);
    }
    if (out) { // there's a file we want to use as an output destination
      // replace stdout with the appropriate file descriptor depending on the mode
      if (outputflag == OUTPUT_TRUNCATE) {
        out_fd = open(out, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
      }
      if (outputflag == OUTPUT_APPEND) {
        out_fd = open(out, O_APPEND | O_RDWR | O_CREAT, S_IRWXU);
      }
    }
    truncargs(args);
  }
  switch (pid = fork()) {
    case -1:
      printf("an error occurred while attempting to create a child process. exiting.\n");
      exit(-1);
    case 0: // successful fork, inside child process
      // firstly, map the I/O as appropriate
      if (in) {
        if (in_fd == -1) {
          printf("an error occurred while attempting to open the file for reading.\n");
          exit(-1);
        }
        dup2(in_fd, STDIN_FILENO); // replace the file descriptor!
      }
      if (out) {
        if (out_fd == -1) {
          printf("an error occurred while attempting to open the file for writing.\n");
          exit(-1);
        }
        dup2(out_fd, STDOUT_FILENO); // replace the file descriptor!
      }
      // secondly, execute the command with the new I/O
      execvp(args[0], args);
      printf("there was an error executing your command.\n");
      exit(-1);
    default:
      if (waitflag) { // the parent typically waits for the child to finish executing
        waitpid(pid, &status, WUNTRACED);
      }
      // tidy up by closing the file descriptors
      close(in_fd);
      close(out_fd);
  }
}

int checkamp(char **args) {
  /* Checks whether a given array of arguments contains an ampersand (the background operator).
   * Returns 1 if an ampersand was found, 0 otherwise.
   */
  int i = 0;
  while (i < MAX_ARGS) {
    if (args[i]) {
      if (!strcmp(args[i], "&")) {
        args[i] = NULL; // delete the ampersand so we can execute the command as usual
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
  /* Print the current working directory. */
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
  /* Get the file names for I/O redirection and set the output flag.
   *
   * Arguments:
   *  - args: the set of arguments that we want to scan for redirects in
   *  - infname: a pointer to where the input filename (if found) will be written
   *  - outfname: a pointer to where the output filename (if found) will be written
   *  - outmode: a pointer to an integer that determines the output mode (if applicable)
   */
  int i;
  for (i=0; i<MAX_ARGS; ++i) { // for each argument
    if (args[i]) { // if it isn't null
      // compare it to an operator to see if there's a match
      // if there is, allocate some memory and copy the filename for later use
      // if there is an output filename, make sure to set the right output mode too
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
  // infname and outfname were originally null so if they're still null after
  // the above for-loop, then this set of arguments didn't have redirects in it
  if (*infname == NULL && *outfname == NULL) {
    return 0;
  }
  return 1;
}

void cd(char *dir) {
  /* Changes the current directory to the directory specified by dir. */
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
  /* Lists the files in the directory d in alphabetical order. Filenames only. If no directory given,
   * list current directory. (Not used in the shell.)
   */ 
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
  /* Lists the files in theh directory d using ls -al. */
  char *cmd = "ls -al";
  if (d) { // if we gave dir an argument
    // build the command
    char *s = (char *)malloc(strlen(cmd)+strlen(d)+strlen(" "));
    strcpy(s, cmd);
    strcat(s, " ");
    strcat(s, d);
    // execute it
    system(s);
    free(s);
    return;
  }
  // else, just execute the generic command on the current directory
  system(cmd);
}

void help(void) {
  /* Opens the user manual. */
  char *cmd = "more -d readme";
  system(cmd);
}

void shellpause(void) {
  /* Do nothing until the Enter key is pressed. */
  char *cmd = "read -r key";
  system(cmd);
}

void env(char **e) {
  /* Print all the environment variables. */
  char **env = e; // we need to introduce the temporary variable
  // env so that we don't modify the global variable environ
  // this is so that when we call environ again, it still works
  while (*env) {
    printf("%s\n",*env++);
  }
}

void echo(char **s) {
  /* Prints its arguments to the screen, with no extra whitespace. */
  int i = 1;
  if (s[i]) { // print the first one
    printf(s[i]);
  }
  while (s[i]) { // print the rest, separated by spaces
    ++i;
    printf(" ");
    printf(s[i]);
  }
  printf("\n"); // print a newline to maintain prettiness
}

int main(int argc, char **argv) {
  char buf[MAX_BUFFER]; // line buffer
  char *args[MAX_ARGS]; // pointers to arg strings
  char **arg; // working pointer thru args

  while (!feof(stdin)) { 
    int waitflag = 1; // default value is 1 because usually you want sequential execution
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
          waitflag = 0; // set the waitflag if there's an ampersand
        }
        // check for redirection and set file names and flags
        // doesn't change anything if no redirects found
        getfnames(args, &infile, &outfile, &outputflag);

        // internal commands
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
