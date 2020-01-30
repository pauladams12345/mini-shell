/*
Program: Mini Shell
Author: Paul Adams
Last Modified: 01 28 2020
Summary: Shell program with limited functionality. Enter a desired command
to run followed by arguments. Add '&' to the end of your command to run
process in background. Toggle background process allowed/disallowed by sending
SIGTSTP (ctrl-z). If a process is requested to be run in the background but
this is currently disallowed, process will be run in the foreground. Enter
'exit' to exit the shell.
*/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<signal.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<fcntl.h>

#define INPUT_SIZE 2048
#define MAX_ARGS 512
#define MAX_BG_PROC 100

void catch_ctrl_z(int signo);
void getCommand(char *command, char **args, char *in_file, char *out_file, int *foreground_ptr);
void executeCommand(char *command, char **args, char *in_file, struct sigaction ctrl_c,
	char *out_file, int *foreground_ptr, int *exit_method, int bg_flag);

pid_t bg_pids[MAX_BG_PROC];	// array of all background pids
int bg_flag; 				// global flag. 1 for allow background processes, 0 for disallow

int main()
{
	char command[INPUT_SIZE];			// command to be executed
	char *args[MAX_ARGS];				// arguments for command
	char in_file[INPUT_SIZE];			// input filename
	char out_file[INPUT_SIZE];			// output filename
	int i;								// index for local use
	int foreground;						// 0 if user requests process to run in background, else 1 for foreground
	int exit_method;					// process status
	int *foreground_ptr;				// for manipulating foreground in functions
	pid_t pid;							// current process id
	struct sigaction ctrl_c = {0};		// handle SIGINT
	struct sigaction ctrl_z = {0};		// handle SIGTSTP

	ctrl_c.sa_handler = SIG_IGN;		// Unless otherwise specified, ignore SIGINT
	sigfillset(&ctrl_c.sa_mask);
	ctrl_c.sa_flags = 0;
	sigaction(SIGINT, &ctrl_c, NULL);	

	ctrl_z.sa_handler = catch_ctrl_z;	// Register custom signal handler for SIGTSTP
	sigfillset(&ctrl_z.sa_mask);
	ctrl_z.sa_flags = 0;
	sigaction(SIGTSTP, &ctrl_z, NULL);

	exit_method = 0;
	bg_flag = 1;						// allow background processes on program start-up
	foreground_ptr = &foreground;

	for (i = 0; i < MAX_BG_PROC; ++i)	// initialize all background pids in array to -1
	{
		bg_pids[i] = -1;
	}

	while (1)							// loop until user enters "exit" command
	{
		memset(command, '\0', sizeof(command));		// Initialize variables for next command
		memset(in_file, '\0', sizeof(in_file));
		memset(out_file, '\0', sizeof(out_file));
		for(i = 0; i < MAX_ARGS; ++i)
		{
			args[i] = NULL;
		}
		foreground = 1;	

		// Get command from user and execute it
		getCommand(command, args, in_file, out_file, foreground_ptr);
		executeCommand(command, args, in_file, ctrl_c, out_file, foreground_ptr, &exit_method, bg_flag);
	}
	
	return 0;
}

// Signal handler for SIGTSTP. Toggles the bg_flag to allow or
// disallow background processes.
void catch_ctrl_z(int signo)
{
	if (bg_flag == 0)		// if currently disallowed, change to allowed
	{
		char *msg = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, msg, 29);
		fflush(stdout);
		bg_flag = 1;
	}
	else					//if currently allowed, change to disallowed
	{
		char *msg = "Entering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, msg, 49);
		fflush(stdout);
		bg_flag = 0;
	}

}

/* Gets input from user. Parses to extract command, arguments, input file,
and output file. First token is interpreted as command. All following commands
are interpreted as arguments until either end of input or one of the following
characters is encountered: "> < &". If "<", next token is input_file. If ">",
next token is output_file. If "&", foreground is set to 0 for false. 
Args:
	char command[INPUT_SIZE];			// command to be executed
	char *args[MAX_ARGS];				// arguments for command
	char in_file[INPUT_SIZE];			// input filename
	char out_file[INPUT_SIZE];			// output filename
	int *foreground_ptr;				// pointer to foreground variable
*/
void getCommand(char *command, char **args, char *in_file, char *out_file, int *foreground_ptr)
{
	char raw_input[INPUT_SIZE];					// user input
	int i, j;									//indices
	memset(raw_input, '\0', sizeof(raw_input));

	printf(": ");								// Get input
	fflush(stdout);
	fgets(raw_input, INPUT_SIZE, stdin);

	// If input is blank or a comment, return control so that executeCommand
	// can wait for terminated children before re-prompting for input
	if (raw_input[0] == '\0' || strncmp(raw_input, "\n", 1) == 0 || strncmp(raw_input, "#", 1) == 0)
	{
		strcpy(command, raw_input);
		return;
	}

	char *buffer = strtok(raw_input, " \n");	// Get command token
	strcpy(command, buffer);
	args[0] = strdup(buffer);

	buffer = strtok(NULL, " \n");				// Get next token

	// While token is an argument, add it to args
	i = 1;
	while (buffer != NULL && strcmp(buffer, "<") != 0 
		&& strcmp(buffer, ">") != 0)
	{
		if (strncmp(buffer, "&\0", 2) == 0)		// If token is "&"
		{
			buffer = strtok(NULL, " \n");		// get next token
			if (buffer == NULL)				
			{
				*foreground_ptr = 0;			// unset foreground flag
			}
			else
			{
				args[i] = strdup("&");			// else there are more tokens, add "&" to args
				++i;
			}
		}
		// Token is not "&", add to args
		else
		{
			args[i] = strdup(buffer);
			buffer = strtok(NULL, " \n");
			++i;
		}
	}

	// Replace '$$' in arguments with process ID
	char prev, cur;
	for (i = 0; i < MAX_ARGS; ++i)
	{
		if (args[i] != NULL)
		{
			cur = '0';
			for (j = 0; j < strlen(args[i]); ++j)
			{
				prev = cur;
				cur = args[i][j];
				if (prev == '$' && cur == '$')
				{
					char prefix[INPUT_SIZE];
					strncpy(prefix, args[i], j-1);
					sprintf(args[i], "%s%d", prefix, getpid());
					strcpy(prefix, "\0");
				}
			}
		}
	}

	// Get input_file and output_file info, if applicable
	while (buffer != NULL && (strncmp(buffer, "<", 1) == 0 || strncmp(buffer, ">", 1) == 0))
	{
		if (strncmp(buffer, "<", 1) == 0)		// set input_file
		{
			buffer = strtok(NULL, " \n");
			strcpy(in_file, buffer);
		}

		else									// set output file
		{
			buffer = strtok(NULL, " \n");
			strcpy(out_file, buffer);
		}

		buffer = strtok(NULL, " \n");			// get next token
	}

	// Check if last token is &, set foreground flag
	if (buffer != NULL && strncmp(buffer, "&", 1) == 0 && strlen(buffer) == 1)
	{
		*foreground_ptr = 0;
	}

}

/* Cleans up any zombie children from the background. Runs the specified 
command with its args. Redirects input from in_file and output to out_file, 
if applicable. If initiating a foreground process, waits until completion.
If initiating a background process, continues execution.
Args
	char command[INPUT_SIZE];			// command to be executed
	char *args[MAX_ARGS];				// arguments for command
	char in_file[INPUT_SIZE];			// input filename
	struct sigaction ctrl_c = {0};		// handle SIGINT
	char out_file[INPUT_SIZE];			// output filename
	int *exit_method;					// pointer to process status
	int *foreground_ptr;				// pointer to foreground flag
	int bg_flag; 				// global flag. 1 for allow background processes, 0 for disallow
*/
void executeCommand(char *command, char **args, char *in_file, struct sigaction ctrl_c,
	char *out_file, int *foreground_ptr, int *exit_method, int bg_flag)
{
	int in_fd;		// nput file descriptor
	int out_fd;		// output file descriptor
	int result, i;

	// Clean up terminated background processes
	pid_t pid;
	pid = waitpid(-1, exit_method, WNOHANG);
	while (pid > 0)
	{
		printf("background pid %d is done: ", pid);
		if (WIFEXITED(*exit_method))				// print message for normal termination
		{
			int exit_status;
			exit_status = WEXITSTATUS(*exit_method);
			printf("exit value %d\n", exit_status);
			fflush(stdout);
		}
		else if (WIFSIGNALED(*exit_method))			// print message for abnormal termination
		{
			int exit_signal;
			exit_signal = WTERMSIG(*exit_method);
			printf("terminated by signal %d\n", exit_signal);
			fflush(stdout);
		}
		
		i = 0;										// Remove reaped background processes from array
		while (bg_pids[i] != pid && i < MAX_BG_PROC)
		{
			++i;
		}
		if (i == MAX_BG_PROC)
		{
			//printf("Unable to locate pid of background process\n");
			//exit(1);
			pid = waitpid(-1, exit_method, WNOHANG);
		}
		else
		{
			bg_pids[i] = -1;
			pid = waitpid(-1, exit_method, WNOHANG);
		}
	}

	// If command is blank or a comment, do nothing and return to main
	if (strncmp(command, "\n", 1) == 0 || strncmp(command, "#", 1) == 0)
	{
		return;
	}

	// Built-in cd command
	if (strcmp(command, "cd") == 0)
	{
		if (args[1] == NULL)				// default (no arguments): cd to HOME directory
		{
			result = chdir(getenv("HOME"));
			if (result == -1)
			{
				printf("Error changing to HOME directory\n");
				fflush(stdout);
			}
		}
		else								// else argument given: cd into it
		{
			result = chdir(args[1]);
			if (result == -1)
			{
				printf("Error changing to specified directory\n");
				fflush(stdout);
			}
		}
	}

	// Built-in exit command
	else if (strcmp(command, "exit") == 0)
	{
		for (i = 0; i < MAX_BG_PROC; ++i)	// kill any background processes
		{
			if (bg_pids[i] != -1)
			{
				kill(bg_pids[i], SIGTERM);
			}
		}
		exit(0);
	}

	// Built-in status command
	else if (strcmp(command, "status") == 0)
	{
		if (exit_method == NULL)					// No commands yet executed
		{
			printf("exit value %d\n", exit_method);
			fflush(stdout);
		}
		else if (WIFEXITED(*exit_method))			// Normal termination
		{
			int exit_status;
			exit_status = WEXITSTATUS(*exit_method);
			printf("exit value %d\n", exit_status);
			fflush(stdout);
		}
		else if (WIFSIGNALED(*exit_method))			// Stopped by signal
		{
			int exit_signal;
			exit_signal = WTERMSIG(*exit_method);
			printf("terminated by signal %d\n", exit_signal);
			fflush(stdout);
		}
	}

	// Non-built-in command
	else
	{
		pid = fork();	// spawn new process for command

		switch (pid)
		{
			// Fork failure
			case -1:
				perror("Fork failure\n");
				break;
			
			// Child process: exec a new process
			case 0:
				// Allow foreground child to be interrupted by SIGINT
				ctrl_c.sa_handler = SIG_DFL;
				sigaction(SIGINT, &ctrl_c, NULL);

				// Redirect stdin, if requested
				if (strcmp(in_file, "") != 0)
				{
					in_fd = open(in_file, O_RDONLY);
					if (in_fd == -1) {printf("cannot open %s for input\n", in_file); exit(1);}
					result = dup2(in_fd, 0);
					if (result == -1) {perror("in_fd dup2() failure"); exit(1);}
					close(in_fd);
				}

				// Redirect stdin to /dev/null for bg processes w/o a stdin redirect request
				else if (bg_flag == 1 && *foreground_ptr == 0)
				{
					in_fd = open("/dev/null", O_RDONLY);
					if (in_fd == -1) {printf("problem opening /dev/null for stdin\n"); exit(1);}
					result = dup2(in_fd, 0);
					if (result == -1) {perror("in_fd dup2() failure"); exit(1);}
					close(in_fd);					
				}

				// Redirect stdout, if requested
				if (strcmp(out_file, "") != 0)
				{

					out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (out_fd == -1) {printf("cannot open %s for output\n", out_file); exit(1);}
					result = dup2(out_fd, 1);
					if (result == -1) {perror("out_fd dup2() failure"); exit(1);}
					close(out_fd);
				}
				// Redirect stdout to /dev/null for bg processes w/o a stdout redirect request
				else if (bg_flag == 1 && *foreground_ptr == 0)
				{
					out_fd = open("/dev/null", O_WRONLY);
					if (out_fd == -1) {printf("problem opening /dev/null for output\n", out_file); exit(1);}
					result = dup2(out_fd, 1);
					if (result == -1) {perror("out_fd dup2() failure"); exit(1);}
					close(out_fd);					
				}

				// Run command
				if (execvp(args[0], (char *const*)args) < 0)
				{
					printf("%s: no such file or directory\n", args[0]);
					fflush(stdout);
					exit(1);
				}
				break;
			
			// Parent process: wait
			default:
				// Process running foreground by request, or by force
				if (*foreground_ptr == 1 || bg_flag == 0)
				{
					pid = waitpid(pid, exit_method, 0);	// wait until child terminates

					// If previous foreground process was killed by a signal, print signal number
					if (WIFSIGNALED(*exit_method))
					{
						int exit_signal;
						exit_signal = WTERMSIG(*exit_method);
						printf("terminated by signal %d\n", exit_signal);
						fflush(stdout);
					}					
				}
				// Background process
				else
				{
					pid_t bg_pid;
					bg_pid = waitpid(pid, exit_method, WNOHANG);	// continue execution
					printf("background pid is %d\n", pid);
					fflush(stdout);

					i = 0;
					while (bg_pids[i] != -1 && i < MAX_BG_PROC)		// add process id to first opens slot in array
					{
						++i;
					}
					if (i == MAX_BG_PROC)
					{
						printf("Ran out of slots for background processes!\n");
						exit(1);
					}
					bg_pids[i] = pid;
				}
		}
	}
}
