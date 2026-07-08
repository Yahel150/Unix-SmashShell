# Unix SmashShell

Unix SmashShell is a small Unix-like shell implementation written in C++ for an operating systems assignment. The executable is named `smash` and provides a prompt, built-in commands, external command execution, background jobs, pipes, redirection, aliases, and signal handling.

## Files

- `smash.cpp` - program entry point and command loop.
- `Commands.h` / `Commands.cpp` - shell command classes and command execution logic.
- `signals.h` / `signals.cpp` - signal handler declarations and implementation.
- `Makefile` - build, clean, and submission archive targets.

## Build

Build the shell with:

```sh
make
```

This creates the `smash` executable in the project directory.

## Run

Start the shell with:

```sh
./smash
```

The default prompt is:


Use `quit` to exit. Use `quit kill` to terminate all background jobs before exiting.

## Supported commands

Built-in commands include:

- `chprompt [prompt]` - change the shell prompt, or reset it to `smash`.
- `showpid` - print the shell process ID.
- `pwd` - print the current working directory.
- `cd <path>` / `cd -` - change directory.
- `jobs` - list background jobs.
- `fg [job-id]` - move a background job to the foreground.
- `kill -<signal> <job-id>` - send a signal to a job.
- `quit [kill]` - exit the shell.
- `alias name='command'` - define an alias.
- `unalias <name> ...` - remove aliases.
- `unsetenv <name> ...` - remove environment variables.
- `sysinfo` - print system information.
- `whoami` - print current user information.
- `du [path]` - print disk usage.
- `usbinfo` - print detected USB device information.

The shell also supports external commands, background execution with `&`, output redirection with `>` and `>>`, and pipes with `|` and `|&`.

## Clean

Remove generated build artifacts with:

```sh
make clean
```

