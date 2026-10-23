/* config.h — sfx user configuration */

#define SHELL "bash" /* shell for :sh and running commands */

/*
 * Command that reads a path on standard input and puts it on the
 * clipboard, used by the `y` key. Use the one your system provides:
 *   macOS    pbcopy
 *   X11      xclip -selection clipboard
 *   Wayland  wl-copy
 */
#define CLIPBOARD "pbcopy"

#define SHOWHIDDEN 1 /* 1 = list dot files at start; H toggles it */

#define REFRESHMS 500 /* ms between checks for outside changes; 0 = off */

/*
 * File opener dispatch table.
 * ext: file extension (NULL = default for unmatched files)
 * cmd: program to run
 * bg:  1 = launch detached (GUI apps), 0 = foreground (terminal apps)
 */
static const struct opener {
	const char *ext;
	const char *cmd;
	int bg;
} openers[] = {
    {0, "vi", 0}, /* default: terminal editor */
};

/*
 * Commands that need the terminal.
 * A `:` command whose first word matches one of these runs full-screen,
 * like `:sh`. Every other `:` command runs with its output captured
 * into the output pane. The list ends with 0.
 */
static const char *ttycmds[] = {
    "vi",    "vim",  "nvim",   "nano",	  "emacs", "ed",   "less",
    "more",  "most", "man",    "info",	  "top",   "htop", "btop",
    "watch", "ssh",  "tmux",   "screen",  "fzf",   "bash", "zsh",
    "fish",  "ksh",  "python", "python3", "irb",   "node", 0};
