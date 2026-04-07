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
│ 2/14 - /home/joel/projects/sfx          │  ← status bar
└─────────────────────────────────────────┘
```

The **canvas** lists the contents of the current directory in `ls -la`
format. The selected entry is highlighted. The **status bar** shows the
position in the list (`k/total`) followed by the current directory path,
or an error message when something goes wrong.

---

## Navigation

| Key                        | Action                          |
|----------------------------|---------------------------------|
| `j` / `↓`                  | move selection down             |
| `k` / `↑`                  | move selection up               |
| `h` / `Backspace`          | go to parent directory          |
| `l` / `Enter`              | enter directory or open file    |
| `gg`                       | jump to first entry             |
| `G` / `End`                | jump to last entry              |
| `Home`                     | jump to first entry             |
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

## File colours

Entries are coloured by type to make the listing easier to scan at a
glance:

| Colour  | Meaning                               |
|---------|---------------------------------------|
| bold    | directory                             |
| cyan    | symbolic link                         |
| green   | executable file                       |
| yellow  | special file (device, pipe, socket)   |
| dim     | hidden file (name starts with `.`)    |

---

## Search

| Key  | Action                                        |
|------|-----------------------------------------------|
| `/`  | open search prompt                            |
| `n`  | jump to next match                            |
| `N`  | jump to previous match                        |

Type a substring and press `Enter`. The cursor jumps to the first
matching entry and all non-matching entries are dimmed. Press `n`/`N`
to cycle through further matches — the dimming stays active while you
navigate search results.

Moving with `j`, `k`, `gg`, `G`, `h`, `l`, or any page key clears the
dimming and restores all colours, while keeping the search term in
memory for the next `/`.

---

## File operations

### Rename — `r`

Press `r` to rename the selected entry. A prompt appears in the status
bar pre-filled with the current name. Edit it and press `Enter` to
confirm, or `Esc` to cancel. The cursor follows the renamed entry.

### Delete — `d`

Press `d` on a single entry to delete it. A confirmation prompt
(`[y/N]`) appears before anything is removed. Directories are deleted
recursively.

---

## Visual selection

Press `V` to enter visual selection mode. The anchor is set at the
current position; moving with `j`/`k` extends the highlighted range.

| Key   | Action                              |
|-------|-------------------------------------|
| `V`   | start / stop visual selection       |
| `Esc` | cancel visual selection             |
| `d`   | delete all selected entries (`[y/N]` confirm) |

---

## Marks

Marks let you record positions and jump back to them instantly, similar
to Vim's marks.

| Key    | Action                                  |
|--------|-----------------------------------------|
| `ma`–`mz` | set mark `a`–`z` at the current entry |
| `'a`–`'z` | jump to mark `a`–`z`                  |

Marks are per-session and lost when `sfx` exits.

---

## Yank path — `y`

Press `y` to copy the full path of the selected entry to the clipboard.
The path is sent via stdin to the command defined as `CLIPBOARD` in
`config.h` (default: `xclip -selection clipboard`). A confirmation
message appears in the status bar.

---

## Split panel — `|`

Press `|` to toggle a two-panel view. The left panel shows the current
directory listing; the right panel shows a preview of the selected entry:

- **Directory** — lists the names of its contents with the same colour
  coding as the main panel.
- **Regular file** — shows the file's mode, size, and modification time.

Press `|` again to return to the full-width single-panel view.

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
The canvas is reloaded after every command and the cursor stays on the
same filename if it still exists.

### Starting a shell

```
:sh
```

This drops into an interactive shell in the current directory. Exit
the shell (e.g. `exit` or `Ctrl-D`) to return to `sfx`.

### Tab completion (optional)

Build with `make USE_READLINE=1` to get GNU readline in the `:` prompt.
This adds tab completion (filenames by default), arrow-key line editing,
and within-session command history (`↑`/`↓`). The rename prompt also
benefits: the current filename is pre-filled and fully editable.

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

| Define      | Default                        | Description                                    |
|-------------|--------------------------------|------------------------------------------------|
| `EDITOR`    | `"vic"`                        | Editor launched when opening a file            |
| `SHELL`     | `"bash"`                       | Shell used for `:sh` and status bar commands   |
| `RCFILE`    | `"~/.bashrc"`                  | Sourced before each command so aliases work    |
| `CLIPBOARD` | `"xclip -selection clipboard"` | Command that reads a path from stdin and copies it |

To use a different editor, shell, or clipboard tool:

```c
#define EDITOR    "nano"
#define SHELL     "zsh"
#define RCFILE    "~/.zshrc"
#define CLIPBOARD "wl-copy"   /* Wayland */
```

Set `RCFILE` to `""` to skip sourcing entirely.
