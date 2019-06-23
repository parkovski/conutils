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

Usage: `hresult <result code>`

## conevents - Print console events.

Usage: `conevents [-v]`. The `-v` option sets virtual terminal input mode.

Press Ctrl-C twice to exit.
