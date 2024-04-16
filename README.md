# Sagit - Selective add for git

Sagit is a small CLI utility which aims to make `git add` more flexible by allowing to stage changes on a line-by-line basis.

## Building and Installing

Requirements: C compiler, ncursesw, pkg-config

Build with `make` and install with `make install`.

Note that the default binary installation path is `/usr/local/bin`, but it can be changed:

```console
make install BIN_PATH="/home/example_user/bin"
```

## Usage

Either use it directly or alias to e.g. sadd:

```console
git config alias.sadd '!sagit'
```

To see keybindings press `h`.

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

## Uninstalling

Don't forget to set the same `BIN_PATH`.

```console
make uninstall
```
