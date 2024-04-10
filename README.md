# Sagit - Selective add for git

Sagit is a small CLI utility which aims to make `git add` more flexible by allowing to stage changes on a line-by-line basis.

## Building and Installing

Requirements: C compiler, ncursesw

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

## Uninstalling

Don't forget to set the same `BIN_PATH`.

```console
make uninstall
```
