# sfx Tutorial

`sfx` is a small terminal file explorer. This document covers every
feature currently implemented.

---

## Invocation

```
sfx [directory]
```

Without an argument, `sfx` opens the current working directory.

---

## Layout

```
┌─────────────────────────────────────────┐
│ drwxr-xr-x  2 joel users  4.0K Apr 6 . │
│ drwxr-xr-x 12 joel users  4.0K Apr 6 ..│  ← canvas
│ -rw-r--r--  1 joel users  1.2K Apr 6 ..│
│                                         │
├─────────────────────────────────────────┤
│ /home/joel/projects/sfx                 │  ← status bar
└─────────────────────────────────────────┘
```

The **canvas** lists the contents of the current directory in `ls -la`
format. The selected entry is highlighted. The **status bar** shows the
current directory path, or an error message when something goes wrong.

---

## Navigation

| Key                        | Action                          |
|----------------------------|---------------------------------|
| `j` / `↓`                  | move selection down             |
| `k` / `↑`                  | move selection up               |
| `h` / `Backspace`          | go to parent directory          |
| `l` / `Enter`              | enter directory or open file    |
| `g` / `Home`               | jump to first entry             |
| `G` / `End`                | jump to last entry              |
| `Ctrl-D`                   | half-page down                  |
| `Ctrl-U`                   | half-page up                    |
| `Ctrl-F` / `Page Down`     | full page down                  |
| `Ctrl-B` / `Page Up`       | full page up                    |
| `Ctrl-L`                   | redraw the screen               |
| `q` / `Q` / `Ctrl-C`       | quit                            |

Entries are listed with `.` first, `..` second, then everything else
in alphabetical order. When navigating up with `h`, the cursor lands on
the directory you came from.

---

## Opening files

Pressing `l` or `Enter` on a regular file opens it with the configured
editor (see `config.h`). `sfx` suspends, the editor takes over the
terminal, and `sfx` resumes when the editor exits.

---

## Status bar commands

Press `:` to type a command in the status bar. Press `Enter` to run it,
`Esc` to cancel.

```
:ls -la
:mkdir notes
:rm old.txt
:vic README.md
```

Commands run through the configured shell (see `config.h`), with the
shell's rc file sourced first so aliases and exports are available.
The canvas is reloaded after every command, so changes to the directory
are reflected immediately.

### Starting a shell

```
:sh
```

This drops into an interactive shell in the current directory. Exit
the shell (e.g. `exit` or `Ctrl-D`) to return to `sfx`.

---

## Working directory

`sfx` calls `chdir` as you navigate, so the process working directory
always matches what is shown in the status bar. Shell commands typed
in the status bar operate in that directory:

```
:vic notes.md       ← creates/opens notes.md in the current directory
:mkdir archive      ← creates archive/ here
```

---

## Configuration

Edit `config.h` and recompile (`make`) to change defaults.

| Define   | Default       | Description                                    |
|----------|---------------|------------------------------------------------|
| `EDITOR` | `"vic"`       | Editor launched when opening a file            |
| `SHELL`  | `"bash"`      | Shell used for `:sh` and status bar commands   |
| `RCFILE` | `"~/.bashrc"` | Sourced before each command so aliases work    |

To use a different editor or shell:

```c
#define EDITOR "nano"
#define SHELL  "zsh"
#define RCFILE "~/.zshrc"
```

Set `RCFILE` to `""` to skip sourcing entirely.
