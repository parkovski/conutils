# Windows Console Utilities

## glob - Shell glob for Windows

Usage: `glob <globs...>`

Since Windows shells don't support globbing, this utility provides it for you.
Currently supported glob features: *, ?, [], {}.

## touch - You know what this is

PowerShell doesn't have a built-in touch command so I made one.

## isvt - Windows console mode utility

A simple tool to report and change console modes for any accessible process.
Run without arguments to see the mode for the current console.
Supported arguments:
- `i+ / i-` &rarr; Enable/disable VT input.
- `o+ / o-` &rarr; Enable/disable VT output.
- `a+ / a-` &rarr; Enable/disable VT input and output.
- `i/o=xxxx` &rarr; Set a custom mode (hex or names, 0x optional)
- `-p pid` &rarr; Attach to process identified by `pid`.
- `-h` &rarr; Show help.
- `-l` &rarr; List valid input/output names and values.

Using the `i/o=` option, names and values can be combined - see help for details.

## syserr - Print a system error description.

Usage: `syserr <result code>`.

Looks up the result code and prints a description of it according to the
operating system's message formatting function. This program works on Windows
and Linux/Unix.

## conevents - Print console events.

Usage: `conevents [-v] [-bv|-ba] [-e[fkmus]]`.

The `-v` option sets virtual terminal input mode. `-bv` and `-ba` set up the
alternate buffer. `-e` specifies which events to show.

Press Ctrl-C twice to exit.

## darkmode - Is dark mode enabled?

Usage: `darkmode`.

## scrollhook - Horizontal scroll toggle.

Usage: `scrollhook [-d]`.

*Must be run from an administrator console.*

Makes the selected key function as a toggle - when the key is held down, the
mouse scroll wheel will send horizontal scroll messages.

The `-d` option inverts the horizontal scroll direction.

See `-h` for debug options.

## istty - Are the program's streams terminal devices?

Usage: `istty -[p][ioe]`

Using `-p` causes `y`/`n` to be printed, otherwise the program returns 0 for
yes and 1 for no.

Also builds on Unix platforms.

## readreparse - Read a Windows reparse point.

Usage: `readreparse <file>`

Supported reparse point types are: mount point, symbolic link, and app exec
link.

## pipesrv - Communicate over named pipes.

Usage: `pipesrv [-w|-s] [pipe_name] [message]`

Use `-s` to run a server, `-w` to wait for messages, and no flags to send a
message. Messages are broadcast to all waiters.

## Scripts

- `mkccmds.js` Run with node.js to generate `compile_commands.json`.
- `build.ps1` Run from a VS dev command prompt to build all (or selected) programs.
