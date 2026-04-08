/* config.h — sfx user configuration */

#define SHELL "bash"	   /* shell for :sh and running commands */
#define RCFILE "~/.bashrc" /* sourced before every status-bar command */

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
	{ ".pdf",  "evince", 1 },
	{ 0,       "vic",    0 },  /* default: terminal editor */
};
