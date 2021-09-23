# Windows Console Utilities

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

## hresult - Print an hresult value.

Usage: `hresult [-w] <result code>`.

Looks up the result code as an `NTSTATUS`. If `-w` is passed, it is tried as
an `HRESULT` (DOS error).

## conevents - Print console events.

Usage: `conevents [-v] [-bv|-ba] [-e[fkmus]]`.

The `-v` option sets virtual terminal input mode. `-bv` and `-ba` set up the
alternate buffer. `-e` specifies which events to show.

Press Ctrl-C twice to exit.

## darkmode - Is dark mode enabled?

Usage: `darkmode`.

## resize - Send a resize event to a console.

Usage: `resize <pid>`.

Might not work.

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

## Scripts

- `mkccmds.js` Run with node.js to generate `compile_commands.json`.
- `build.ps1` Run from a VS dev command prompt to build all (or selected) programs.
