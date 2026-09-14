# sfx Tutorial

`sfx` is a small terminal file explorer. This document walks through its
features in the order you are likely to need them. For a strict reference
of everything that is implemented, including limits and edge cases, see
`SUPPORTED.md`.

---

## Building

```
make
```

The first build copies `config.def.h` to `config.h`. From then on
`config.h` is yours: it is never overwritten, and `make clean` leaves it
alone. Edit it and run `make` again to change the configuration.

```
make install        # installs /usr/local/bin/sfx
```

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
or an error message when something goes wrong. With more than one tab
open, the tab indicator `[current/total]` is prepended.

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
| `R`                        | force a full refresh            |
| `q` / `Q` / `Ctrl-C`       | quit                            |

Entries are listed with `.` first, `..` second, then everything else
in alphabetical order. When navigating up with `h`, the cursor lands on
the directory you came from. Resizing the terminal redraws the screen
automatically.

---

## Opening files

Pressing `l` or `Enter` on a regular file opens it with the opener that
matches its name (see `openers[]` in `config.h`). Symbolic links are
resolved first, so a link to a directory is entered and a link to a file
is opened. Devices, pipes, and sockets are ignored.

A terminal opener takes over the screen: `sfx` suspends, the program runs,
and `sfx` resumes when it exits. A graphical opener is launched detached
instead, so `sfx` stays on screen and remains usable.

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

Type a substring and press `Enter`. Matching is case-sensitive. The cursor
jumps to the first matching entry and all non-matching entries are dimmed.
Press `n`/`N` to cycle through further matches, wrapping around the end of
the list — the dimming stays active while you navigate search results.

Moving with `j`, `k`, `gg`, `G`, `h`, `l`, or any page key clears the
dimming and restores all colours, while keeping the search term in
memory for the next `n` or `/`.

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

`.` and `..` are never deleted, even when the range covers them.

---

## Marks

Marks let you record positions and jump back to them instantly, similar
to Vim's marks.

| Key    | Action                                  |
|--------|-----------------------------------------|
| `ma`–`mz` | set mark `a`–`z` at the current entry |
| `'a`–`'z` | jump to mark `a`–`z`                  |

A mark stores a position in the list, not a file name, so it is only
meaningful inside the directory where you set it. Marks belong to the
current tab and are lost when `sfx` exits.

---

## Bookmarks

Bookmarks are the persistent counterpart of marks: they record a
directory, and they survive across runs.

| Key       | Action                                      |
|-----------|---------------------------------------------|
| `mA`–`mZ` | bookmark the current directory under `A`–`Z` |
| `'A`–`'Z` | jump to bookmark `A`–`Z`                     |

The bookmark file is written immediately on every `m<UPPERCASE>` press.
It lives at `$XDG_DATA_HOME/sfx/bookmarks`, or at
`~/.local/share/sfx/bookmarks` when `XDG_DATA_HOME` is unset. Its format
is one line per bookmark: a letter, a space, then the path.

---

## Tabs

| Key   | Action                                       |
|-------|----------------------------------------------|
| `t`   | open a new tab on the current directory      |
| `T`   | open a new tab on `$HOME`                    |
| `gt`  | switch to the next tab                       |
| `gT`  | switch to the previous tab                   |
| `x`   | close the current tab                        |

A new tab opens immediately to the right of the current one and becomes
active. `gt` and `gT` wrap around the ends. Up to eight tabs can be open
at once.

Each tab remembers its own directory, cursor position, scroll offset,
search term, and marks. Switching to a tab re-reads its directory from
disk, so changes made elsewhere show up.

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
- **Anything else** — shows the entry's mode, size, and modification time.

Press `|` again to return to the full-width single-panel view. The split
needs at least 20 columns; in a narrower terminal the view stays
single-panel.

---

## Status bar commands

Press `:` to type a command in the status bar. Press `Enter` to run it,
`Esc` to cancel.

```
:ls -la
:mkdir notes
:rm old.txt
:vi README.md
```

Commands run through the configured shell (see `config.h`) as an
interactive shell, so the shell sources its own rc file and aliases are
available. The canvas is reloaded after every command and the cursor stays
on the same filename if it still exists.

### Changing directory

```
:cd /var/log
:cd ~/Projects
:cd            ← bare :cd goes to $HOME
```

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
benefits: the current filename is pre-filled and fully editable. The
search prompt (`/`) always uses the built-in line reader.

---

## Working directory

`sfx` calls `chdir` as you navigate, so the process working directory
always matches what is shown in the status bar. Shell commands typed
in the status bar operate in that directory:

```
:vi notes.md        ← creates/opens notes.md in the current directory
:mkdir archive      ← creates archive/ here
```

---

## Configuration

Edit `config.h` and recompile (`make`) to change defaults.

| Define      | Default                        | Description                                        |
|-------------|--------------------------------|----------------------------------------------------|
| `SHELL`     | `"bash"`                       | Shell used for `:sh` and status bar commands       |
| `CLIPBOARD` | `"xclip -selection clipboard"` | Command that reads a path from stdin and copies it |
| `openers[]` | `vi` for everything            | Which program opens which file                     |

`CLIPBOARD` is not present in `config.def.h`; its default lives in
`sfx.c`. Define it in `config.h` to override it:

```c
#define SHELL     "zsh"
#define CLIPBOARD "wl-copy"   /* Wayland; use "pbcopy" on macOS */
```

### Choosing openers

`openers[]` maps a file name suffix to a program. Rows are scanned in
order and the first match wins. The last row has `0` in place of a suffix,
and it is the fallback for everything unmatched.

```c
} openers[] = {
	{ ".png", "feh",     1 },  /* image viewer, launched detached */
	{ ".pdf", "zathura", 1 },
	{ ".md",  "vi",      0 },  /* terminal program, takes over the screen */
	{ 0,      "vi",      0 },  /* default */
};
```

The third field is the background flag: `1` launches the program detached
so `sfx` stays usable, and `0` runs it in the foreground.
