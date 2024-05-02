# Sagit

Sagit is a small CLI utility which aims to make `git add` more flexible by allowing to stage changes on a line-by-line basis.

Works on Linux and MacOS

## Building and Installing

Requirements: C compiler, ncursesw, pkg-config

```console
$ make install
```

Note that the default binary installation path is `/usr/local/bin`, but it can be changed:

```console
$ make install BIN_PATH="/home/example_user/bin"
```

## Usage

There are multiple options for using it:

### Directly

```console
$ sagit
```

### Git alias

Alias it to, e.g. `git sadd`:

```console
$ git config alias.sadd '!sagit'
```

### VSCode integration

You can add keybinding like this to open full screen terminal in VSCode (to `keybindings.json`):

```json
{
    "key": "KEYBINDING",
    "command": "runCommands",
    "args": {
        "commands": [
            "workbench.action.createTerminalEditor",
            {
                "command": "workbench.action.terminal.sendSequence",
                "args": {
                    "text": "sagit && exit\u000D"
                }
            }
        ]
    }
}
```

NOTE: `\u000D` acts like Enter and is used to run the command

## Configuring

Configuration is done by modifying `src/config.h` and rebuilding. \
If changes didn't apply, try running `make clean` before rebuilding again.

## Keybindings

Press `h` to see help page along with keybindings

## Uninstalling

Don't forget to set the same `BIN_PATH`.

```console
$ make uninstall BIN_PATH="/home/example_user/bin"
```
