# Supported functionality

This document describes every functionality implemented in `sfx` at the
current revision, and how to use each one. The list of features is
exhaustive: it is derived from `sfx.c`, `config.def.h` and the `Makefile`.

`sfx` is a terminal file explorer. It draws a listing of one directory on
a canvas, keeps a one-line status bar at the bottom, and reads single
keystrokes in raw mode.

---

## 1. Building and installing

| Command | Effect |
|---------|--------|
| `make` | Compiles `sfx` from `sfx.c` and `config.h` |
| `make USE_READLINE=1` | Compiles with GNU readline support (see section 12) |
| `make install` | Installs the binary as `/usr/local/bin/sfx`, mode 755 |
| `make frmt` | Runs `clang-format -i` over `*.c` and `*.h` |
| `make clean` | Removes the `sfx` binary |

The build uses `cc -std=c99 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -O2`.
On macOS the `Makefile` adds `-D_DARWIN_C_SOURCE`, because macOS hides
`SIGWINCH` under a strict `_XOPEN_SOURCE`.

`config.h` is not tracked in git. The `Makefile` creates it once by copying
`config.def.h`, and never overwrites it afterwards, so local configuration
survives rebuilds. `make clean` does not delete it. To restart from the
template, delete `config.h` and run `make` again.

---

## 2. Invocation

```
sfx [directory]
```

With no argument `sfx` opens the current working directory. With one
argument it opens that directory. With two or more arguments it prints
`usage: sfx [directory]` to standard error and exits with status 1.

`sfx` calls `chdir` on every navigation, so the process working directory
always matches the path shown in the status bar.

---

## 3. Screen layout

```
┌──────────────────────────────────────────────┐
│ drwxr-xr-x  5 joel staff  160 Sep 14 09:12 . │
│ drwxr-xr-x 20 joel staff  640 Sep 14 08:22 ..│  ← canvas
│ -rw-r--r--  1 joel staff  37K Sep 14 08:33 s │
├──────────────────────────────────────────────┤
│ 3/13 - /Users/joel/Projects/sfx              │  ← status bar
└──────────────────────────────────────────────┘
```

The canvas occupies every row except the last one. The last row is the
status bar.

Each entry is rendered in `ls -la` format with these columns, in order:
mode string, hard link count, owner name, group name, size, modification
time, and file name. The owner and group fall back to the numeric uid and
gid when no passwd or group record exists. Sizes are printed in binary
units (`K`, `M`, `G`) with one decimal, or as a plain byte count below 1K.
Timestamps older than about six months, and timestamps in the future, are
printed as `Mon DD  YYYY`; every other timestamp is printed as
`Mon DD HH:MM`. Lines longer than the panel are truncated.

The status bar shows one of the following, in this order of precedence:

1. The current message, when there is one (an error, a confirmation, or a
   prompt).
2. `[tab/ntabs] k/total - /current/path`, when more than one tab is open.
3. `k/total - /current/path`, otherwise.

`k` is the 1-based index of the selection and `total` is the number of
entries. When the text is wider than the terminal, its left side is cut so
that the tail remains visible.

Entries are sorted with `.` first, `..` second, and the rest in byte-wise
alphabetical order. Hidden files are always listed. At most 4096 entries
per directory are read; entries beyond that limit are ignored.

---

## 4. Entry colours

Colours are applied in this order of precedence, and the first match wins.
This list is exhaustive.

| Appearance | Meaning |
|------------|---------|
| reverse video | the selection, or any entry inside the visual selection |
| dim | non-matching entry while search dimming is active |
| bold | directory |
| cyan | symbolic link |
| yellow | other non-regular file (device, FIFO, socket) |
| green | regular file with any execute bit set |
| dim | regular file whose name starts with `.` |

---

## 5. Navigation

| Key | Action |
|-----|--------|
| `j`, `↓` | move the selection down one entry |
| `k`, `↑` | move the selection up one entry |
| `h`, `Backspace` | go to the parent directory |
| `l`, `Enter` | enter the directory, or open the file |
| `gg` | jump to the first entry |
| `Home` | jump to the first entry |
| `G`, `End` | jump to the last entry |
| `Ctrl-D` | move down half a page |
| `Ctrl-U` | move up half a page |
| `Ctrl-F`, `Page Down` | move down one full page |
| `Ctrl-B`, `Page Up` | move up one full page |
| `Ctrl-L` | re-read the terminal size and redraw |
| `R` | force a full refresh (re-query size and redraw) |
| `q`, `Q`, `Ctrl-C` | quit |

The canvas scrolls automatically to keep the selection visible. At the
filesystem root, `h` keeps you at `/`. When you move up out of a
directory, the cursor is placed on the directory you came from.

Every movement key listed above clears search dimming and clears the
current status message. The search term itself is kept.

The terminal is restored on exit, on `SIGTERM`, and on `SIGINT`. A window
resize (`SIGWINCH`) triggers an automatic redraw.

---

## 6. Opening files

Pressing `l` or `Enter` on an entry does one of the following, and these
cases are mutually exclusive and cover every entry type:

- **Directory** — `sfx` navigates into it.
- **Symbolic link** — the link is resolved with `stat`, and the target's
  type decides the behaviour. A link to a directory is entered; a link to
  a regular file is opened.
- **Regular file** — the file is opened with the matching opener from the
  `openers[]` table in `config.h` (see section 11).
- **Any other type** (device, FIFO, socket) — nothing happens.

An opener marked `bg = 0` runs in the foreground: `sfx` leaves raw mode,
clears the screen, runs the command with the file path as its only
argument, waits for it to exit, then restores raw mode and redraws. If the
command cannot be executed, the status bar reports
`<cmd>: command not found`.

An opener marked `bg = 1` runs detached: `sfx` forks, calls `setsid`,
redirects the child's standard output and standard error to `/dev/null`,
and execs the command. The status bar then shows `<cmd> <name>`. `sfx`
stays on screen and remains usable.

---

## 7. Search

| Key | Action |
|-----|--------|
| `/` | open the search prompt |
| `n` | jump to the next match |
| `N` | jump to the previous match |

Type a substring and press `Enter`. Matching is a plain case-sensitive
substring test against the entry name. `Esc` cancels the prompt, and
`Backspace` deletes the last character. An empty search is ignored.

On confirmation the cursor jumps to the next match and every non-matching
entry is dimmed. `n` and `N` cycle forwards and backwards through matches,
wrapping around the end of the list, and they keep the dimming active. If
no entry matches, the status bar shows `no match: <term>`.

Any cursor movement key clears the dimming but keeps the term, so pressing
`n` afterwards still works.

---

## 8. File operations

### Rename — `r`

Press `r` to rename the selected entry. The status bar shows a `rename: `
prompt pre-filled with the current name. `Enter` confirms, `Esc` cancels.
An empty name, or a name equal to the old one, cancels the operation. The
rename is a single `rename(2)` call, so it cannot move an entry across
filesystems. On success the cursor follows the new name. On failure the
status bar shows `rename: <errno message>`. The entries `.` and `..`
cannot be renamed.

### Delete — `d`

Press `d` to delete the selected entry. The status bar asks
`delete '<name>'? [y/N] `, and only `y` or `Y` proceeds. Deletion runs
`rm -rf <path>` with output discarded, so directories are removed
recursively. The entries `.` and `..` cannot be deleted. On failure the
status bar shows `delete failed: <errno message>`.

When a visual selection is active, `d` deletes the whole selection instead;
see section 9.

### Yank path — `y`

Press `y` to copy the absolute path of the selected entry to the
clipboard. The path is written to the standard input of the `CLIPBOARD`
command, which avoids all shell quoting problems. On success the status
bar shows `yanked: <path>`; otherwise it shows
`yank failed — check CLIPBOARD in config.h`.

---

## 9. Visual selection

| Key | Action |
|-----|--------|
| `V` | start visual selection, or stop it if already active |
| `Esc` | cancel visual selection |
| `j`, `k` | extend the selected range |
| `d` | delete every entry in the range, after confirmation |

`V` sets the anchor at the current entry. The range covers every entry
between the anchor and the cursor, inclusive, and the whole range is shown
in reverse video.

Pressing `d` asks `delete N entries? [y/N] `, and only `y` or `Y`
proceeds. `.` and `..` are trimmed from both ends of the range before the
count is computed, so they are never deleted. Each entry is removed with a
separate `rm -rf`. If any removal fails, the status bar reports
`N deletion(s) failed`. The selection is dropped afterwards, whatever the
outcome.

Visual selection is also dropped whenever you change directory or switch
tab.

---

## 10. Marks, bookmarks, tabs and the split panel

### Marks — `ma`–`mz` and `'a`–`'z`

`m` followed by a lower-case letter records the current entry index under
that letter. `'` followed by the same letter moves the cursor back to that
index. Marks store an index, not a name, so they are only meaningful
within one directory listing. Marks belong to a tab and are lost when
`sfx` exits.

### Bookmarks — `mA`–`mZ` and `'A`–`'Z`

`m` followed by an upper-case letter stores the current directory as a
bookmark under that letter, and writes the bookmark file to disk
immediately. The status bar confirms with `bookmark 'X' -> <path>`.

`'` followed by an upper-case letter navigates to that bookmark. If the
letter is unset, the status bar shows `bookmark 'X' not set`.

Bookmarks are persistent across runs. The file is chosen as follows, and
the first applicable case wins:

1. `$XDG_DATA_HOME/sfx/bookmarks`, when `XDG_DATA_HOME` is set and not
   empty.
2. `$HOME/.local/share/sfx/bookmarks`, when `HOME` is set.
3. `/tmp/sfx-bookmarks`, otherwise.

The directory is created with mode 0700 when needed. The format is one
line per bookmark: a lower-case letter, a space, then the path. Malformed
lines are skipped on load.

### Tabs

| Key | Action |
|-----|--------|
| `t` | open a new tab on the current directory |
| `T` | open a new tab on `$HOME` (or `/` when `HOME` is unset) |
| `gt` | switch to the next tab, wrapping around |
| `gT` | switch to the previous tab, wrapping around |
| `x` | close the current tab |

A new tab is inserted immediately after the current one and becomes
active. At most 8 tabs can exist; beyond that the status bar shows
`tab limit reached (8)`. Closing the last remaining tab is refused with
`only one tab`.

Each tab keeps its own directory, cursor position, scroll offset, search
term, search dimming state, and set of 26 marks. Switching tabs reloads
the directory from disk. The status bar shows `[current/total]` whenever
more than one tab is open.

### Split panel — `|`

Press `|` to toggle a two-panel view, and press it again to return to a
single full-width panel. The split is only drawn when the terminal is at
least 20 columns wide; below that width the view stays single-panel.

The left panel is the normal listing. The right panel previews the
selected entry, and the two cases below are mutually exclusive:

- **Directory** — the names of its contents, sorted the same way and
  coloured by the same rules as the main listing.
- **Anything else** — three lines showing the mode string, the size, and
  the modification time.

The preview is cached and re-read only when the selected directory
changes, or when an operation invalidates the cache (navigation, rename,
deletion, tab switch, or toggling the split).

---

## 11. Status-bar commands

Press `:` to type a command in the status bar. `Enter` runs it and `Esc`
cancels it. The following forms are recognised, and this list is
exhaustive:

| Input | Effect |
|-------|--------|
| `:cd` | navigate to `$HOME`, or to `/` when `HOME` is unset |
| `:cd <path>` | navigate to `<path>`; a leading `~` is expanded to `$HOME` |
| `:sh` | start an interactive shell in the current directory |
| a command whose first word is listed in `ttycmds[]` | run it full-screen, with the terminal handed over |
| anything else | run it with its output captured into the output pane |

Every command other than `:sh` runs as `SHELL -i -c "<command>"`. The
`-i` flag makes the shell interactive, so it sources the user's rc file
and expands aliases. The listing is reloaded after the command, and the
cursor stays on the same file name when that name still exists.

### Captured commands and the output pane

A command whose first word is not listed in `ttycmds[]` runs with its
standard output and standard error captured, and with `/dev/null` on its
standard input. `sfx` stays in raw mode and does not clear the screen, so
the listing stays visible while the command runs.

When the command produced output, that output is shown in a pane at the
bottom of the screen: a rule, then the first lines of the output. The
pane takes at most a third of the screen, and it always leaves at least
one row for the listing. The status line below the pane reports the
command, its exit status when that status is not 0, and how many lines
did not fit. `Enter` dismisses the pane and returns the status bar to its
normal contents. Navigating to another directory dismisses it too,
because the output belongs to the directory it was produced in. Every
other key leaves the pane on screen.

When the command produced no output, no pane appears, and the status line
reports `:<command> — exit <n>` instead.

Captured output is sanitised before it is shown: control characters other
than newline become spaces, so a command cannot move the cursor or set
attributes on the screen.

While a captured command runs, the terminal signal keys are enabled, so
`Ctrl-C` interrupts the command. `Ctrl-Z` remains disabled, because
stopping `sfx` would leave the terminal in raw mode.

### Full-screen commands

`:sh` execs `SHELL` with no arguments. A command whose first word matches
an entry of `ttycmds[]` runs as `SHELL -i -c "<command>"`. In both cases
`sfx` leaves raw mode, clears the screen, and hands the terminal over for
the duration, which is what programs that draw their own screen (`vi`,
`less`, `man`, `top`) require. Leave the program (`exit`, `Ctrl-D`, `:q`,
`q`, as the program requires) to return to `sfx`. The listing is reloaded
on return.

Only the first word of the command is matched against `ttycmds[]`, and
its directory part is ignored, so `:/usr/bin/vim notes.md` is recognised.
A program that needs the terminal but is not listed misbehaves under
capture; add it to `ttycmds[]` in `config.h` to fix that.

After any child that used the terminal exits, `sfx` makes its own process
group the foreground process group of the terminal again. An interactive
shell takes the terminal for job control and does not hand it back, and a
process that then touches the terminal from a background process group is
stopped by `SIGTTOU`.

Because `sfx` keeps the process working directory synchronised with the
displayed path, relative paths in these commands resolve against the
directory on screen:

```
:mkdir archive      creates ./archive here
:rm old.txt         removes ./old.txt here
```

---

## 12. Configuration

Configuration is compile-time, in `config.h`. Edit the file and run `make`
to apply changes. The complete set of settings follows.

### `SHELL`

The shell used for `:sh`, for general `:` commands, and for the clipboard
pipeline. Default: `"bash"`.

### `CLIPBOARD`

The command that reads a path on standard input and copies it. It is not
defined in `config.def.h`; `sfx.c` supplies the default
`"xclip -selection clipboard"` when `config.h` does not define it. Define
it in `config.h` to override, for instance:

```c
#define CLIPBOARD "wl-copy"                 /* Wayland */
#define CLIPBOARD "pbcopy"                  /* macOS */
```

### `openers[]`

The file opener dispatch table. Each row has three fields:

| Field | Meaning |
|-------|---------|
| `ext` | file name suffix to match, or `0` for the default row |
| `cmd` | program to execute, with the file path as its only argument |
| `bg`  | `1` to launch detached, `0` to run in the foreground |

Matching is a plain suffix comparison against the whole file name, so the
suffix may include the dot, and it need not be a conventional extension.
Rows are scanned in order and the first match wins. The row with `ext = 0`
terminates the table and acts as the fallback for unmatched files, so it
must come last. Example:

```c
} openers[] = {
	{ ".png",  "feh",   1 },  /* image viewer, detached */
	{ ".pdf",  "zathura", 1 },
	{ ".md",   "vi",    0 },
	{ 0,       "vi",    0 },  /* default: terminal editor */
};
```

### `ttycmds[]`

The list of commands that need the terminal. A `:` command whose first
word matches an entry runs full-screen instead of being captured. The
list is an array of strings terminated by `0`:

```c
static const char *ttycmds[] = {
	"vi", "vim", "less", "man", "top",
	0
};
```

Matching is an exact comparison of whole words, so `vim` does not match
`vimdiff`. The default list covers the common terminal editors, pagers,
monitors and shells.

### Readline (build-time option)

Build with `make USE_READLINE=1` to link against GNU readline. This
changes the `:` prompt and the rename prompt to full readline line
editing, which adds tab completion of file names, cursor movement within
the line, and within-session history on `↑` and `↓`. Without this option
the prompts support printable characters, `Backspace`, `Enter` to confirm,
and `Esc` to cancel.

The search prompt (`/`) never uses readline; it always uses the built-in
line reader.

---

## 13. Limits and known constraints

This list is exhaustive with respect to the fixed limits compiled into the
program.

- At most 4096 entries are read per directory, for the main listing and
  for the preview panel alike.
- At most 8 tabs.
- At most 26 marks per tab and 26 bookmarks in total, one per letter.
- Entry names are truncated to `NAME_MAX` bytes and paths to `PATH_MAX`
  bytes.
- Status messages are truncated to 512 bytes, and `:` commands to 512
  bytes.
- Captured command output is truncated to 8192 bytes and to 256 lines,
  and the command name shown in the status line to 63 bytes.
- Output is byte-oriented, so multi-byte characters in file names are
  counted as several columns when a line is truncated.

---

## 14. Licence

MIT. See `LICENSE.md`.
