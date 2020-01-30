# Mini Shell

C-based UNIX shell program. Redirects I/O and allows simultaneous processes in the foreground and background.

## Compilation

Simply compile using your favorite compiler, no special options necessary. Here's an example using GCC:

gcc -o minishell minishell.c

## Use

Once you've started the program, you can run one of the following built-in commands:

* `exit`: Exit the shell
* `cd`: Change directory. Takes one or zero arguments. With one argument, changes to the specified directory. With no arguments, changes to the HOME directory of the environment variable
* `status`: Prints the exit status or terminating signal of the last foreground process
 Add '&' to the end of your command to run process in background. Toggle background process allowed/disallowed by sending SIGTSTP (ctrl-z). If a process is requested to be run in the background but this is currently disallowed, process will be run in the foreground. Enter 'exit' to exit the shell.

You can also run non-built-in commands by specifying their name. Commands run in the foreground by default, but you can also add the '&' character after the command name to run it in the background. The complete syntax is:

`command [arg1 arg2 ...] [< input_filename] [> output_filename] [&]`

Examples:

* `ls > directory.txt`: List the contents of the current directory and redirect output to a file named directory.txt
* `sleep 5 &`: Run the sleep command in the background for 5 seconds

## Authors

Paul Adams