/* sfx.c — simple file explorer */

#include "config.h"
#include <stdio.h>
#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#undef ESC /* readline's chardefs.h defines ESC; we define our own below */
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESC       "\033"
#define ESC_CUP   ESC "[%d;%dH" /* cursor position (1-based) */
#define ESC_EL    ESC "[K"      /* erase to end of line */
#define ESC_ED    ESC "[2J"     /* erase display */
#define ESC_HOME  ESC "[H"      /* cursor to top-left */
#define ESC_REV   ESC "[7m"     /* reverse video */
#define ESC_NRM   ESC "[m"      /* attributes off */
#define ESC_BOLD  ESC "[1m"     /* bold */
#define ESC_DIM   ESC "[2m"     /* dim/faint */
#define ESC_CYAN  ESC "[36m"    /* cyan — symlinks */
#define ESC_GREEN ESC "[32m"    /* green — executables */
#define ESC_YEL   ESC "[33m"    /* yellow — special files */
#define ESC_HIDE  ESC "[?25l"   /* hide cursor */
#define ESC_SHOW  ESC "[?25h"   /* show cursor */

#ifndef CLIPBOARD
#define CLIPBOARD "xclip -selection clipboard"
#endif

#define MAXENT   4096
#define MSGBUFSZ 512

enum {
	KEY_UP = 256,
	KEY_DOWN,
	KEY_PGUP,
	KEY_PGDN,
	KEY_HOME,
	KEY_END,
};

#define MAXTABS 8

typedef struct tab Tab;
struct tab {
	char cwd[PATH_MAX];
	int sel;
	int top;
	char search[NAME_MAX + 1];
	int have_search;
	int search_dim;
	int marks[26];
};

typedef struct entry Entry;
struct entry {
	char name[NAME_MAX + 1];
	struct stat st;
};

typedef struct state State;
struct state {
	int rows, cols;
	struct termios orig;
	Entry *ents;
	int nent;
	int sel;
	int top;
	char cwd[PATH_MAX];
	char msg[MSGBUFSZ];
	int have_msg;
	char search[NAME_MAX + 1];
	int have_search;
	int search_dim;       /* dim non-matching entries; cleared on cursor movement */
	int marks[26];        /* jump marks; -1 = unset */
	int visual;           /* visual selection active */
	int vanchor;          /* visual selection anchor index */
	int pending_g;        /* waiting for second 'g' press */
	int split;            /* split-panel mode */
	Entry *pents;         /* preview panel entries */
	int pnent;            /* preview entry count */
	char ppath[PATH_MAX]; /* path currently loaded in pents */
};

/* prototypes */
static void rawmode(void);
static void cookmode(void);
static void query_dims(void);
static int readkey(void);
static void fmt_mode(mode_t m, char *buf);
static void fmt_size(off_t sz, char *buf, size_t bufsz);
static void fmt_time(time_t t, char *buf, size_t bufsz);
static void fmt_entry(Entry *e, char *buf, size_t bufsz);
static int ent_cmp(const void *a, const void *b);
static int load_dir(const char *path);
static void reload_dir(void);
static void nav_to(const char *path);
static void cursor_at(int row, int col);
static void draw_entry(int row, int idx, Entry *e, int col_start, int max_width);
static void draw_status(void);
static void load_preview(void);
static void draw_preview(int col_start, int width, int rows);
static void draw(void);
static const struct opener *match_opener(const char *name);
static void open_bg(const char *cmd, const char *path, const char *name);
static void open_fg(const char *cmd, const char *path);
static void open_entry(void);
static void spawn_shell(void);
static int run_argv_silent(char **argv);
static void run_shell_cmd(const char *cmd);
static void rename_entry(void);
static void delete_entry(void);
static void delete_visual(void);
static void yank_path(void);
static void read_str(const char *prompt, char *buf, int bufsz, const char *prefill);
static void read_cmd(void);
static void read_search(void);
static void search_jump(int dir);
static void tab_save(void);
static void tab_load(int idx);
static void tab_new(const char *path);
static void tab_close(void);
static void bookmark_path(char *buf, size_t bufsz);
static void bookmark_load(void);
static void bookmark_save(void);
static void bookmark_set(int idx);
static void bookmark_jump(int idx);
static void cleanup(void);
static void handle_exit(int sig);
static void handle_sigwinch(int sig);

static State g;
static volatile sig_atomic_t resize_pending;
static Tab tabs[MAXTABS];
static int ntabs;
static int curtab;
static char bookmarks[26][PATH_MAX];

static void
handle_sigwinch(int sig)
{
	(void)sig;
	resize_pending = 1;
}

static void
handle_exit(int sig)
{
	(void)sig;
	cleanup();
	_exit(0);
}

static void
rawmode(void)
{
	static int saved;
	struct termios t;

	if (!saved) {
		tcgetattr(STDIN_FILENO, &g.orig);
		saved = 1;
	}
	t = g.orig;
	t.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	t.c_oflag &= ~(unsigned)OPOST;
	t.c_cflag |= CS8;
	t.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void
cookmode(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g.orig);
}

static void
query_dims(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
		g.rows = (int)ws.ws_row;
		g.cols = (int)ws.ws_col;
	}
	if (g.rows < 2)
		g.rows = 24;
	if (g.cols < 10)
		g.cols = 80;
}

static int
readkey(void)
{
	unsigned char buf[8];
	struct pollfd pfd;
	ssize_t n;

	n = read(STDIN_FILENO, buf, 1);
	if (n <= 0)
		return -1;
	if (buf[0] != 0x1b)
		return (int)buf[0];

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, 50) <= 0)
		return 0x1b;

	n = read(STDIN_FILENO, buf + 1, sizeof(buf) - 1);
	if (n <= 0)
		return 0x1b;

	if (buf[1] == '[' && n >= 2) {
		switch (buf[2]) {
		case 'A':
			return KEY_UP;
		case 'B':
			return KEY_DOWN;
		case 'H':
			return KEY_HOME;
		case 'F':
			return KEY_END;
		case '5':
			if (n >= 4 && buf[3] == '~')
				return KEY_PGUP;
			break;
		case '6':
			if (n >= 4 && buf[3] == '~')
				return KEY_PGDN;
			break;
		}
	}
	return 0x1b;
}

static void
fmt_mode(mode_t m, char *buf)
{
	char t;

	if (S_ISDIR(m))
		t = 'd';
	else if (S_ISLNK(m))
		t = 'l';
	else if (S_ISBLK(m))
		t = 'b';
	else if (S_ISCHR(m))
		t = 'c';
	else if (S_ISFIFO(m))
		t = 'p';
	else if (S_ISSOCK(m))
		t = 's';
	else
		t = '-';

	buf[0] = t;
	buf[1] = (m & S_IRUSR) ? 'r' : '-';
	buf[2] = (m & S_IWUSR) ? 'w' : '-';
	buf[3] = ((m & S_ISUID) && (m & S_IXUSR)) ? 's'
		 : (m & S_ISUID)		  ? 'S'
		 : (m & S_IXUSR)		  ? 'x'
						  : '-';
	buf[4] = (m & S_IRGRP) ? 'r' : '-';
	buf[5] = (m & S_IWGRP) ? 'w' : '-';
	buf[6] = ((m & S_ISGID) && (m & S_IXGRP)) ? 's'
		 : (m & S_ISGID)		  ? 'S'
		 : (m & S_IXGRP)		  ? 'x'
						  : '-';
	buf[7] = (m & S_IROTH) ? 'r' : '-';
	buf[8] = (m & S_IWOTH) ? 'w' : '-';
	buf[9] = ((m & S_ISVTX) && (m & S_IXOTH)) ? 't'
		 : (m & S_ISVTX)		  ? 'T'
		 : (m & S_IXOTH)		  ? 'x'
						  : '-';
	buf[10] = '\0';
}

static void
fmt_size(off_t sz, char *buf, size_t bufsz)
{
	if (sz >= (off_t)1 << 30)
		snprintf(buf, bufsz, "%.1fG", (double)sz / ((off_t)1 << 30));
	else if (sz >= (off_t)1 << 20)
		snprintf(buf, bufsz, "%.1fM", (double)sz / ((off_t)1 << 20));
	else if (sz >= (off_t)1 << 10)
		snprintf(buf, bufsz, "%.1fK", (double)sz / ((off_t)1 << 10));
	else
		snprintf(buf, bufsz, "%lld", (long long)sz);
}

static void
fmt_time(time_t t, char *buf, size_t bufsz)
{
	struct tm *tm;
	time_t now;

	now = time(NULL);
	tm = localtime(&t);
	if (!tm) {
		snprintf(buf, bufsz, "?");
		return;
	}
	if (now - t > (time_t)6 * 30 * 24 * 3600 || t > now)
		strftime(buf, bufsz, "%b %e  %Y", tm);
	else
		strftime(buf, bufsz, "%b %e %H:%M", tm);
}

static void
fmt_entry(Entry *e, char *buf, size_t bufsz)
{
	char mode[12], sz[24], ts[20];
	const char *user, *grp;
	struct passwd *pw;
	struct group *gr;
	char ubuf[32], gbuf[32];

	fmt_mode(e->st.st_mode, mode);
	fmt_size(e->st.st_size, sz, sizeof(sz));
	fmt_time(e->st.st_mtime, ts, sizeof(ts));

	pw = getpwuid(e->st.st_uid);
	if (pw) {
		user = pw->pw_name;
	} else {
		snprintf(ubuf, sizeof(ubuf), "%u", (unsigned)e->st.st_uid);
		user = ubuf;
	}

	gr = getgrgid(e->st.st_gid);
	if (gr) {
		grp = gr->gr_name;
	} else {
		snprintf(gbuf, sizeof(gbuf), "%u", (unsigned)e->st.st_gid);
		grp = gbuf;
	}

	snprintf(buf, bufsz, "%s %2lu %-8s %-8s %5s %s %s", mode,
		 (unsigned long)e->st.st_nlink, user, grp, sz, ts, e->name);
}

static int
ent_cmp(const void *a, const void *b)
{
	const Entry *ea = a, *eb = b;

	if (strcmp(ea->name, ".") == 0)
		return -1;
	if (strcmp(eb->name, ".") == 0)
		return 1;
	if (strcmp(ea->name, "..") == 0)
		return -1;
	if (strcmp(eb->name, "..") == 0)
		return 1;
	return strcmp(ea->name, eb->name);
}

static int
load_dir(const char *path)
{
	DIR *d;
	struct dirent *de;
	Entry *e;
	char full[PATH_MAX];
	int n;

	d = opendir(path);
	if (!d) {
		snprintf(g.msg, sizeof(g.msg), "%s: %s", path, strerror(errno));
		g.have_msg = 1;
		return -1;
	}

	n = 0;
	while ((de = readdir(d)) && n < MAXENT) {
		e = &g.ents[n];
		strncpy(e->name, de->d_name, NAME_MAX);
		e->name[NAME_MAX] = '\0';
		snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
		if (lstat(full, &e->st) < 0)
			memset(&e->st, 0, sizeof(e->st));
		n++;
	}
	closedir(d);

	g.nent = n;
	qsort(g.ents, g.nent, sizeof(*g.ents), ent_cmp);
	return 0;
}

/*
 * Reload the current directory, restoring the cursor to the same filename.
 * Falls back to the last entry if the name no longer exists.
 */
static void
reload_dir(void)
{
	char name[NAME_MAX + 1];
	int i;

	if (g.nent > 0 && g.sel >= 0 && g.sel < g.nent) {
		strncpy(name, g.ents[g.sel].name, NAME_MAX - 1);
		name[NAME_MAX - 1] = '\0';
	} else {
		name[0] = '\0';
	}

	load_dir(g.cwd);

	if (name[0]) {
		for (i = 0; i < g.nent; i++) {
			if (strcmp(g.ents[i].name, name) == 0) {
				g.sel = i;
				return;
			}
		}
	}
	if (g.sel >= g.nent)
		g.sel = g.nent > 0 ? g.nent - 1 : 0;
}

static void
nav_to(const char *path)
{
	char resolved[PATH_MAX];
	char prev[PATH_MAX];
	const char *base;
	char *r;
	int i;

	strncpy(prev, g.cwd, PATH_MAX - 1);
	prev[PATH_MAX - 1] = '\0';

	r = realpath(path, resolved);
	if (!r) {
		snprintf(g.msg, sizeof(g.msg), "%s: %s", path, strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (load_dir(resolved) < 0)
		return;
	if (chdir(resolved) < 0) {
		snprintf(g.msg, sizeof(g.msg), "chdir: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}

	strncpy(g.cwd, resolved, PATH_MAX - 1);
	g.cwd[PATH_MAX - 1] = '\0';
	g.sel = 0;
	g.top = 0;
	g.have_msg = 0;
	g.visual = 0;
	g.ppath[0] = '\0'; /* invalidate preview cache */

	/* navigating up: position cursor on the dir we came from */
	if (prev[0] && strlen(g.cwd) < strlen(prev)) {
		base = strrchr(prev, '/');
		if (base) {
			base++;
			for (i = 0; i < g.nent; i++) {
				if (strcmp(g.ents[i].name, base) == 0) {
					g.sel = i;
					break;
				}
			}
		}
	}
}

static void
cursor_at(int row, int col)
{
	printf(ESC_CUP, row, col);
}

/*
 * Draw a single directory entry.
 * In split mode col_start > 1 or max_width < g.cols; entries are padded
 * to fill the panel rather than erasing to end-of-line.
 */
static void
draw_entry(int row, int idx, Entry *e, int col_start, int max_width)
{
	char buf[512];
	int highlighted, dim, blen, pad;
	int vlo, vhi;
	mode_t mode;

	vlo = vhi = -1;
	if (g.visual) {
		vlo = g.vanchor < g.sel ? g.vanchor : g.sel;
		vhi = g.vanchor > g.sel ? g.vanchor : g.sel;
	}
	highlighted = (idx == g.sel) || (g.visual && idx >= vlo && idx <= vhi);

	fmt_entry(e, buf, sizeof(buf));
	blen = (int)strlen(buf);
	if (blen > max_width) {
		buf[max_width] = '\0';
		blen = max_width;
	}

	cursor_at(row, col_start);

	if (highlighted) {
		fputs(ESC_REV, stdout);
	} else {
		mode = e->st.st_mode;
		dim = g.search_dim && g.search[0] && !strstr(e->name, g.search);
		if (dim)
			fputs(ESC_DIM, stdout);
		else if (S_ISDIR(mode))
			fputs(ESC_BOLD, stdout);
		else if (S_ISLNK(mode))
			fputs(ESC_CYAN, stdout);
		else if (!S_ISREG(mode))
			fputs(ESC_YEL, stdout);
		else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			fputs(ESC_GREEN, stdout);
		else if (e->name[0] == '.')
			fputs(ESC_DIM, stdout);
	}

	fputs(buf, stdout);

	if (max_width < g.cols) {
		/* split mode: pad to fill panel so the right panel isn't clobbered */
		pad = max_width - blen;
		while (pad-- > 0)
			putchar(' ');
		fputs(ESC_NRM, stdout);
	} else {
		fputs(ESC_EL ESC_NRM, stdout);
	}
}

static void
draw_status(void)
{
	const char *text;
	char buf[PATH_MAX + 32];
	int len;

	cursor_at(g.rows, 1);
	if (g.have_msg) {
		text = g.msg;
	} else if (ntabs > 1) {
		snprintf(buf, sizeof(buf), "[%d/%d] %d/%d - %s",
		    curtab + 1, ntabs,
		    g.nent > 0 ? g.sel + 1 : 0, g.nent, g.cwd);
		text = buf;
	} else {
		snprintf(buf, sizeof(buf), "%d/%d - %s",
		    g.nent > 0 ? g.sel + 1 : 0, g.nent, g.cwd);
		text = buf;
	}
	len = (int)strlen(text);
	if (len > g.cols)
		text += len - g.cols;
	fputs(text, stdout);
	fputs(ESC_EL, stdout);
}

/*
 * Load the preview panel for the currently selected entry.
 * Only re-reads from disk when the selection has changed.
 */
static void
load_preview(void)
{
	Entry *e;
	DIR *d;
	struct dirent *de;
	char path[PATH_MAX], full[PATH_MAX];
	int n;

	if (g.nent == 0)
		return;
	e = &g.ents[g.sel];
	if (!S_ISDIR(e->st.st_mode)) {
		g.pnent = 0;
		g.ppath[0] = '\0';
		return;
	}

	snprintf(path, sizeof(path), "%s/%s", g.cwd, e->name);
	if (strcmp(path, g.ppath) == 0)
		return; /* already loaded */

	d = opendir(path);
	if (!d) {
		g.pnent = 0;
		strncpy(g.ppath, path, PATH_MAX - 1);
		g.ppath[PATH_MAX - 1] = '\0';
		return;
	}

	n = 0;
	while ((de = readdir(d)) && n < MAXENT) {
		Entry *pe = &g.pents[n];
		strncpy(pe->name, de->d_name, NAME_MAX);
		pe->name[NAME_MAX] = '\0';
		snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
		if (lstat(full, &pe->st) < 0)
			memset(&pe->st, 0, sizeof(pe->st));
		n++;
	}
	closedir(d);

	g.pnent = n;
	qsort(g.pents, g.pnent, sizeof(*g.pents), ent_cmp);
	strncpy(g.ppath, path, PATH_MAX - 1);
	g.ppath[PATH_MAX - 1] = '\0';
}

/*
 * Draw the right panel of the split view.
 * For directories: lists entry names with type colours.
 * For files: shows mode, size, and timestamp.
 */
static void
draw_preview(int col_start, int width, int rows)
{
	Entry *e, *pe;
	char buf[NAME_MAX + 2];
	char sz[24], ts[20], modebuf[12];
	mode_t mode;
	int i, row, blen, pad;

	if (g.nent == 0) {
		for (row = 1; row <= rows; row++) {
			cursor_at(row, col_start);
			fputs(ESC_EL, stdout);
		}
		return;
	}

	e = &g.ents[g.sel];

	if (!S_ISDIR(e->st.st_mode)) {
		fmt_mode(e->st.st_mode, modebuf);
		fmt_size(e->st.st_size, sz, sizeof(sz));
		fmt_time(e->st.st_mtime, ts, sizeof(ts));
		cursor_at(1, col_start);
		printf("%-*s", width, modebuf);
		cursor_at(2, col_start);
		printf("%-*s", width, sz);
		cursor_at(3, col_start);
		printf("%-*s", width, ts);
		for (row = 4; row <= rows; row++) {
			cursor_at(row, col_start);
			fputs(ESC_EL, stdout);
		}
		return;
	}

	for (i = 0, row = 1; i < g.pnent && row <= rows; i++, row++) {
		pe = &g.pents[i];
		mode = pe->st.st_mode;

		cursor_at(row, col_start);

		if (S_ISDIR(mode))
			fputs(ESC_BOLD, stdout);
		else if (S_ISLNK(mode))
			fputs(ESC_CYAN, stdout);
		else if (!S_ISREG(mode))
			fputs(ESC_YEL, stdout);
		else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			fputs(ESC_GREEN, stdout);
		else if (pe->name[0] == '.')
			fputs(ESC_DIM, stdout);

		strncpy(buf, pe->name, NAME_MAX);
		buf[NAME_MAX] = '\0';
		blen = (int)strlen(buf);
		if (blen > width) {
			buf[width] = '\0';
			blen = width;
		}
		fputs(buf, stdout);
		pad = width - blen;
		while (pad-- > 0)
			putchar(' ');
		fputs(ESC_NRM, stdout);
	}
	for (; row <= rows; row++) {
		cursor_at(row, col_start);
		fputs(ESC_EL, stdout);
	}
}

static void
draw(void)
{
	int i, row, visible, lwidth, sep, rstart, rwidth;

	visible = g.rows - 1;
	if (visible < 1)
		visible = 1;

	if (g.sel < g.top)
		g.top = g.sel;
	if (g.sel >= g.top + visible)
		g.top = g.sel - visible + 1;
	if (g.top < 0)
		g.top = 0;

	if (g.split && g.cols >= 20) {
		lwidth = g.cols / 2 - 1;
		sep    = lwidth + 1;
		rstart = lwidth + 2;
		rwidth = g.cols - rstart + 1;
		load_preview();
	} else {
		lwidth = g.cols;
		sep = rstart = rwidth = 0;
	}

	fputs(ESC_HIDE, stdout);

	for (i = g.top, row = 1; i < g.nent && row <= visible; i++, row++) {
		draw_entry(row, i, &g.ents[i], 1, lwidth);
		if (sep) {
			cursor_at(row, sep);
			putchar('|');
		}
	}
	for (; row <= visible; row++) {
		cursor_at(row, 1);
		fputs(ESC_EL, stdout);
		if (sep) {
			cursor_at(row, sep);
			putchar('|');
		}
	}

	if (rstart)
		draw_preview(rstart, rwidth, visible);

	draw_status();
	fflush(stdout);
	fputs(ESC_SHOW, stdout);
	fflush(stdout);
}

/*
 * Find the opener for a filename by scanning the openers[] table.
 * Returns the first entry whose extension matches, or the default
 * (ext == NULL) entry.
 */
static const struct opener *
match_opener(const char *name)
{
	const struct opener *o;
	size_t nlen, elen;

	nlen = strlen(name);
	for (o = openers; o->ext; o++) {
		elen = strlen(o->ext);
		if (nlen >= elen && strcmp(name + nlen - elen, o->ext) == 0)
			return o;
	}
	return o; /* default entry (ext == NULL) */
}

/*
 * Launch a background (GUI) opener: fork, detach, exec.
 */
static void
open_bg(const char *cmd, const char *path, const char *name)
{
	char *args[3];
	pid_t pid;
	int fd;

	pid = fork();
	if (pid < 0) {
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (pid == 0) {
		fd = open("/dev/null", O_WRONLY);
		if (fd >= 0) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		setsid();
		args[0] = (char *)cmd;
		args[1] = (char *)path;
		args[2] = NULL;
		execvp(cmd, args);
		_exit(127);
	}
	snprintf(g.msg, sizeof(g.msg), "%s %s", cmd, name);
	g.have_msg = 1;
}

/*
 * Launch a foreground (terminal) opener: swap to cooked mode, exec,
 * wait, restore raw mode.
 */
static void
open_fg(const char *cmd, const char *path)
{
	char *args[3];
	pid_t pid;
	int st;

	cookmode();
	fputs(ESC_NRM ESC_ED ESC_HOME, stdout);
	fflush(stdout);

	signal(SIGINT, SIG_IGN);
	pid = fork();
	if (pid < 0) {
		rawmode();
		signal(SIGINT, handle_exit);
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
		args[0] = (char *)cmd;
		args[1] = (char *)path;
		args[2] = NULL;
		execvp(cmd, args);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	signal(SIGINT, handle_exit);
	rawmode();
	query_dims();
	g.have_msg = 0;
}

static void
open_entry(void)
{
	Entry *e;
	char path[PATH_MAX];
	const struct opener *o;
	struct stat follow;
	mode_t mode;

	if (g.nent == 0)
		return;
	e = &g.ents[g.sel];
	mode = e->st.st_mode;

	snprintf(path, sizeof(path), "%s/%s", g.cwd, e->name);

	/* follow symlinks to determine target type */
	if (S_ISLNK(mode) && stat(path, &follow) == 0)
		mode = follow.st_mode;

	if (S_ISDIR(mode)) {
		nav_to(path);
		return;
	}

	if (!S_ISREG(mode))
		return;

	o = match_opener(e->name);
	if (o->bg)
		open_bg(o->cmd, path, e->name);
	else
		open_fg(o->cmd, path);
}

static void
spawn_shell(void)
{
	char *args[2];
	pid_t pid;
	int st;

	cookmode();
	fputs(ESC_NRM ESC_ED ESC_HOME, stdout);
	fflush(stdout);

	signal(SIGINT, SIG_IGN);
	pid = fork();
	if (pid < 0) {
		rawmode();
		signal(SIGINT, handle_exit);
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
		args[0] = SHELL;
		args[1] = NULL;
		execvp(SHELL, args);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	signal(SIGINT, handle_exit);
	rawmode();
	query_dims();
	reload_dir();
	g.have_msg = 0;
}

/*
 * Fork and exec argv without a shell, redirecting stdout/stderr to /dev/null.
 * Returns 0 on success, -1 on fork failure or non-zero exit.
 */
static int
run_argv_silent(char **argv)
{
	pid_t pid;
	int st, fd;

	pid = fork();
	if (pid < 0) {
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return -1;
	}
	if (pid == 0) {
		fd = open("/dev/null", O_WRONLY);
		if (fd >= 0) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		execvp(argv[0], argv);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static void
run_shell_cmd(const char *cmd)
{
	char *args[5];
	pid_t pid;
	int st;

	cookmode();
	fputs(ESC_NRM ESC_ED ESC_HOME, stdout);
	fflush(stdout);

	signal(SIGINT, SIG_IGN);
	pid = fork();
	if (pid < 0) {
		rawmode();
		signal(SIGINT, handle_exit);
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
		args[0] = SHELL;
		args[1] =
		    "-i"; /* interactive: sources rc file, expands aliases */
		args[2] = "-c";
		args[3] = (char *)cmd;
		args[4] = NULL;
		execvp(SHELL, args);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	signal(SIGINT, handle_exit);
	rawmode();
	reload_dir();
	g.have_msg = 0;
}

/*
 * Prompt for a new name and rename the selected entry in-place.
 * The prompt is pre-filled with the current filename.
 */
static void
rename_entry(void)
{
	Entry *e;
	char newname[NAME_MAX + 1];
	char oldpath[PATH_MAX], newpath[PATH_MAX];
	int i;

	if (g.nent == 0)
		return;
	e = &g.ents[g.sel];
	if (strcmp(e->name, ".") == 0 || strcmp(e->name, "..") == 0)
		return;

	read_str("rename: ", newname, sizeof(newname), e->name);
	if (newname[0] == '\0' || strcmp(newname, e->name) == 0)
		return;

	snprintf(oldpath, sizeof(oldpath), "%s/%s", g.cwd, e->name);
	snprintf(newpath, sizeof(newpath), "%s/%s", g.cwd, newname);

	if (rename(oldpath, newpath) < 0) {
		snprintf(g.msg, sizeof(g.msg), "rename: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}

	load_dir(g.cwd);
	g.have_msg = 0;
	g.ppath[0] = '\0';
	for (i = 0; i < g.nent; i++) {
		if (strcmp(g.ents[i].name, newname) == 0) {
			g.sel = i;
			return;
		}
	}
	if (g.sel >= g.nent)
		g.sel = g.nent > 0 ? g.nent - 1 : 0;
}

/* Confirm and delete the selected entry using rm -rf. */
static void
delete_entry(void)
{
	Entry *e;
	char path[PATH_MAX];
	char prompt[NAME_MAX + 32];
	char *argv[4];
	int c;

	if (g.nent == 0)
		return;
	e = &g.ents[g.sel];
	if (strcmp(e->name, ".") == 0 || strcmp(e->name, "..") == 0)
		return;

	snprintf(prompt, sizeof(prompt), "delete '%s'? [y/N] ", e->name);
	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL, stdout);
	fputs(prompt, stdout);
	fflush(stdout);

	c = readkey();
	if (c != 'y' && c != 'Y') {
		g.have_msg = 0;
		return;
	}

	snprintf(path, sizeof(path), "%s/%s", g.cwd, e->name);
	argv[0] = "rm";
	argv[1] = "-rf";
	argv[2] = path;
	argv[3] = NULL;

	if (run_argv_silent(argv) < 0) {
		snprintf(g.msg, sizeof(g.msg), "delete failed: %s", strerror(errno));
		g.have_msg = 1;
	} else {
		g.ppath[0] = '\0';
		reload_dir();
		g.have_msg = 0;
	}
}

/* Confirm and delete all entries in the visual selection. */
static void
delete_visual(void)
{
	char path[PATH_MAX];
	char prompt[64];
	char *argv[4];
	int lo, hi, n, i, c, failed;

	lo = g.vanchor < g.sel ? g.vanchor : g.sel;
	hi = g.vanchor > g.sel ? g.vanchor : g.sel;

	/* skip . and .. */
	while (lo <= hi &&
	       (strcmp(g.ents[lo].name, ".") == 0 ||
	        strcmp(g.ents[lo].name, "..") == 0))
		lo++;
	while (hi >= lo &&
	       (strcmp(g.ents[hi].name, ".") == 0 ||
	        strcmp(g.ents[hi].name, "..") == 0))
		hi--;
	if (lo > hi)
		return;

	n = hi - lo + 1;
	snprintf(prompt, sizeof(prompt), "delete %d entries? [y/N] ", n);
	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL, stdout);
	fputs(prompt, stdout);
	fflush(stdout);

	c = readkey();
	if (c != 'y' && c != 'Y') {
		g.have_msg = 0;
		return;
	}

	argv[0] = "rm";
	argv[1] = "-rf";
	argv[2] = path;
	argv[3] = NULL;

	failed = 0;
	for (i = lo; i <= hi; i++) {
		snprintf(path, sizeof(path), "%s/%s", g.cwd, g.ents[i].name);
		if (run_argv_silent(argv) < 0)
			failed++;
	}

	g.ppath[0] = '\0';
	reload_dir();
	if (failed) {
		snprintf(g.msg, sizeof(g.msg), "%d deletion(s) failed", failed);
		g.have_msg = 1;
	} else {
		g.have_msg = 0;
	}
}

/*
 * Pipe the full path of the selected entry to CLIPBOARD.
 * The path is passed via stdin to avoid shell quoting issues.
 */
static void
yank_path(void)
{
	char path[PATH_MAX];
	char *args[4];
	pid_t pid;
	int pipefd[2], st, n;

	if (g.nent == 0)
		return;
	snprintf(path, sizeof(path), "%s/%s", g.cwd, g.ents[g.sel].name);
	n = (int)strlen(path);

	if (pipe(pipefd) < 0) {
		snprintf(g.msg, sizeof(g.msg), "pipe: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		snprintf(g.msg, sizeof(g.msg), "fork: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	if (pid == 0) {
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);
		args[0] = SHELL;
		args[1] = "-c";
		args[2] = CLIPBOARD;
		args[3] = NULL;
		execvp(SHELL, args);
		_exit(127);
	}
	close(pipefd[0]);
	write(pipefd[1], path, n);
	close(pipefd[1]);
	waitpid(pid, &st, 0);

	if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
		snprintf(g.msg, sizeof(g.msg), "yanked: %s", path);
	} else {
		snprintf(g.msg, sizeof(g.msg), "yank failed — check CLIPBOARD in config.h");
	}
	g.have_msg = 1;
}

#ifdef USE_READLINE
static const char *rl_prefill_str;
static int
rl_prefill_hook(void)
{
	if (rl_prefill_str && rl_prefill_str[0])
		rl_insert_text(rl_prefill_str);
	rl_prefill_str = NULL;
	return 0;
}
#endif

/*
 * Read a string from the user into buf (at most bufsz-1 chars).
 * Shows prompt at the status bar. If prefill is non-NULL the input
 * is pre-populated with that string (useful for rename).
 * Sets buf[0] = '\0' if the user aborts with Escape.
 */
static void
read_str(const char *prompt, char *buf, int bufsz, const char *prefill)
{
#ifdef USE_READLINE
	char *line;

	cookmode();
	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL, stdout);
	fflush(stdout);
	rl_prefill_str = prefill;
	rl_startup_hook = rl_prefill_hook;
	line = readline(prompt);
	rawmode();
	if (line == NULL) {
		buf[0] = '\0';
		return;
	}
	strncpy(buf, line, bufsz - 1);
	buf[bufsz - 1] = '\0';
	free(line);
#else
	int len, c;

	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL, stdout);
	fputs(prompt, stdout);

	len = 0;
	if (prefill && prefill[0]) {
		len = (int)strlen(prefill);
		if (len >= bufsz)
			len = bufsz - 1;
		memcpy(buf, prefill, len);
		buf[len] = '\0';
		fputs(buf, stdout);
	}
	fflush(stdout);

	for (;;) {
		c = readkey();
		if (c == '\r' || c == '\n')
			break;
		if (c == 0x1b || c < 0) {
			buf[0] = '\0';
			return;
		}
		if ((c == 0x7f || c == '\b') && len > 0) {
			len--;
			fputs("\b \b", stdout);
			fflush(stdout);
			continue;
		}
		if (c >= ' ' && c < 0x7f && len < bufsz - 1) {
			buf[len++] = (char)c;
			putchar(c);
			fflush(stdout);
		}
	}
	buf[len] = '\0';
#endif
}

static void
read_cmd(void)
{
	char cmd[512];
	char path[PATH_MAX];
	const char *arg;
	const char *home;

	read_str(":", cmd, sizeof(cmd), NULL);
	if (cmd[0] == '\0')
		return;
#ifdef USE_READLINE
	add_history(cmd);
#endif

	if (strcmp(cmd, "cd") == 0) {
		/* bare :cd — go home */
		home = getenv("HOME");
		nav_to(home ? home : "/");
		return;
	}

	if (strncmp(cmd, "cd ", 3) == 0) {
		arg = cmd + 3;
		while (*arg == ' ')
			arg++;
		if (arg[0] == '~' && (arg[1] == '/' || arg[1] == '\0')) {
			home = getenv("HOME");
			if (home)
				snprintf(path, sizeof(path), "%s%s", home, arg + 1);
			else
				snprintf(path, sizeof(path), "%s", arg);
		} else {
			snprintf(path, sizeof(path), "%s", arg);
		}
		nav_to(path);
		return;
	}

	if (strcmp(cmd, "sh") == 0)
		spawn_shell();
	else
		run_shell_cmd(cmd);
}

static void
search_jump(int dir)
{
	int i, count;

	if (!g.have_search || g.search[0] == '\0' || g.nent == 0)
		return;
	i = (g.sel + dir + g.nent) % g.nent;
	for (count = 0; count < g.nent; count++) {
		if (strstr(g.ents[i].name, g.search)) {
			g.sel = i;
			g.have_msg = 0;
			return;
		}
		i = (i + dir + g.nent) % g.nent;
	}
	snprintf(g.msg, sizeof(g.msg), "no match: %s", g.search);
	g.have_msg = 1;
}

static void
read_search(void)
{
	int len, c;

	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL "/", stdout);
	fflush(stdout);

	len = 0;
	for (;;) {
		c = readkey();
		if (c == '\r' || c == '\n')
			break;
		if (c == 0x1b || c < 0)
			return;
		if ((c == 0x7f || c == '\b') && len > 0) {
			len--;
			fputs("\b \b", stdout);
			fflush(stdout);
			continue;
		}
		if (c >= ' ' && c < 0x7f && len < NAME_MAX) {
			g.search[len++] = (char)c;
			putchar(c);
			fflush(stdout);
		}
	}
	g.search[len] = '\0';
	if (len == 0)
		return;
	g.have_search = 1;
	g.search_dim = 1;
	search_jump(1);
}

/* Save active state into tabs[curtab]. */
static void
tab_save(void)
{
	Tab *t = &tabs[curtab];
	int i;

	snprintf(t->cwd, sizeof(t->cwd), "%s", g.cwd);
	t->sel = g.sel;
	t->top = g.top;
	snprintf(t->search, sizeof(t->search), "%s", g.search);
	t->have_search = g.have_search;
	t->search_dim = g.search_dim;
	for (i = 0; i < 26; i++)
		t->marks[i] = g.marks[i];
}

/* Restore tabs[idx] into active state and reload its directory. */
static void
tab_load(int idx)
{
	Tab *t = &tabs[idx];
	int i;

	curtab = idx;
	strncpy(g.cwd, t->cwd, PATH_MAX - 1);
	g.cwd[PATH_MAX - 1] = '\0';
	g.sel = t->sel;
	g.top = t->top;
	strncpy(g.search, t->search, NAME_MAX);
	g.search[NAME_MAX] = '\0';
	g.have_search = t->have_search;
	g.search_dim = t->search_dim;
	for (i = 0; i < 26; i++)
		g.marks[i] = t->marks[i];
	g.visual = 0;
	g.pending_g = 0;
	g.ppath[0] = '\0';
	g.have_msg = 0;

	if (chdir(g.cwd) < 0) {
		snprintf(g.msg, sizeof(g.msg), "chdir: %s", strerror(errno));
		g.have_msg = 1;
	}
	load_dir(g.cwd);
	if (g.sel >= g.nent)
		g.sel = g.nent > 0 ? g.nent - 1 : 0;
}

/* Open a new tab at path, inserted after the current tab. */
static void
tab_new(const char *path)
{
	char resolved[PATH_MAX];
	char *r;
	int i;

	if (ntabs >= MAXTABS) {
		snprintf(g.msg, sizeof(g.msg), "tab limit reached (%d)", MAXTABS);
		g.have_msg = 1;
		return;
	}
	r = realpath(path, resolved);
	if (!r) {
		snprintf(g.msg, sizeof(g.msg), "%s: %s", path, strerror(errno));
		g.have_msg = 1;
		return;
	}

	tab_save();

	for (i = ntabs; i > curtab + 1; i--)
		tabs[i] = tabs[i - 1];
	ntabs++;
	curtab++;

	{
		Tab *t = &tabs[curtab];
		snprintf(t->cwd, sizeof(t->cwd), "%s", resolved);
		t->sel = 0;
		t->top = 0;
		t->search[0] = '\0';
		t->have_search = 0;
		t->search_dim = 0;
		for (i = 0; i < 26; i++)
			t->marks[i] = -1;
	}
	tab_load(curtab);
}

/* Close the current tab and switch to the adjacent one. */
static void
tab_close(void)
{
	int i;

	if (ntabs <= 1) {
		snprintf(g.msg, sizeof(g.msg), "only one tab");
		g.have_msg = 1;
		return;
	}
	for (i = curtab; i < ntabs - 1; i++)
		tabs[i] = tabs[i + 1];
	ntabs--;
	if (curtab >= ntabs)
		curtab = ntabs - 1;
	tab_load(curtab);
}

/* Resolve the bookmarks file path honouring XDG_DATA_HOME. */
static void
bookmark_path(char *buf, size_t bufsz)
{
	const char *xdg = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");

	if (xdg && xdg[0])
		snprintf(buf, bufsz, "%s/sfx/bookmarks", xdg);
	else if (home)
		snprintf(buf, bufsz, "%s/.local/share/sfx/bookmarks", home);
	else
		snprintf(buf, bufsz, "/tmp/sfx-bookmarks");
}

/* Load bookmarks from disk into the bookmarks[] array. */
static void
bookmark_load(void)
{
	char bpath[PATH_MAX];
	char line[PATH_MAX + 4];
	FILE *f;
	int idx, n;

	bookmark_path(bpath, sizeof(bpath));
	f = fopen(bpath, "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		n = (int)strlen(line);
		if (n < 3 || line[0] < 'a' || line[0] > 'z' || line[1] != ' ')
			continue;
		idx = line[0] - 'a';
		if (line[n - 1] == '\n')
			line[n - 1] = '\0';
		snprintf(bookmarks[idx], PATH_MAX, "%s", line + 2);
	}
	fclose(f);
}

/* Write the bookmarks[] array to disk, creating the directory if needed. */
static void
bookmark_save(void)
{
	char bpath[PATH_MAX], dir[PATH_MAX];
	char *p;
	FILE *f;
	int i;

	bookmark_path(bpath, sizeof(bpath));
	strncpy(dir, bpath, PATH_MAX - 1);
	dir[PATH_MAX - 1] = '\0';
	p = strrchr(dir, '/');
	if (p) {
		*p = '\0';
		mkdir(dir, 0700); /* no-op if already exists */
	}

	f = fopen(bpath, "w");
	if (!f) {
		snprintf(g.msg, sizeof(g.msg), "bookmarks: %s", strerror(errno));
		g.have_msg = 1;
		return;
	}
	for (i = 0; i < 26; i++) {
		if (bookmarks[i][0])
			fprintf(f, "%c %s\n", 'a' + i, bookmarks[i]);
	}
	fclose(f);
}

/* Set bookmark idx to the current directory and persist it. */
static void
bookmark_set(int idx)
{
	snprintf(bookmarks[idx], PATH_MAX, "%s", g.cwd);
	bookmark_save();
	snprintf(g.msg, sizeof(g.msg), "bookmark '%c' -> %s", 'A' + idx, g.cwd);
	g.have_msg = 1;
}

/* Jump to bookmark idx, or show an error if it is not set. */
static void
bookmark_jump(int idx)
{
	if (bookmarks[idx][0] == '\0') {
		snprintf(g.msg, sizeof(g.msg), "bookmark '%c' not set", 'A' + idx);
		g.have_msg = 1;
		return;
	}
	nav_to(bookmarks[idx]);
}

static void
cleanup(void)
{
	cookmode();
	fputs(ESC_NRM ESC_ED ESC_HOME ESC_SHOW, stdout);
	fflush(stdout);
}

int
main(int argc, char *argv[])
{
	const char *start;
	int c, half, full, i;

	if (argc > 2) {
		fprintf(stderr, "usage: sfx [directory]\n");
		return 1;
	}
	start = argc == 2 ? argv[1] : ".";

	g.ents = malloc(MAXENT * sizeof(*g.ents));
	g.pents = malloc(MAXENT * sizeof(*g.pents));
	if (!g.ents || !g.pents) {
		fprintf(stderr, "sfx: out of memory\n");
		return 1;
	}
	for (i = 0; i < 26; i++)
		g.marks[i] = -1;

	signal(SIGWINCH, handle_sigwinch);
	signal(SIGTERM, handle_exit);
	signal(SIGINT, handle_exit);
	signal(SIGPIPE, SIG_IGN);

	rawmode();
	query_dims();
	nav_to(start);
	ntabs = 1;
	curtab = 0;
	tab_save();
	bookmark_load();
	draw();

	for (;;) {
		if (resize_pending) {
			resize_pending = 0;
			query_dims();
			draw();
		}

		c = readkey();

		/* consume the second key of a g-prefixed sequence */
		if (g.pending_g) {
			g.pending_g = 0;
			if (c == 'g') {
				g.sel = 0;
				g.top = 0;
				g.have_msg = 0;
				g.search_dim = 0;
				draw();
			} else if (c == 't') {
				tab_save();
				tab_load((curtab + 1) % ntabs);
				draw();
			} else if (c == 'T') {
				tab_save();
				tab_load((curtab - 1 + ntabs) % ntabs);
				draw();
			}
			continue;
		}

		half = (g.rows - 1) / 2;
		full = g.rows - 1;

		switch (c) {
		case 3: /* Ctrl-C */ /* FALLTHROUGH */
		case 'q':	     /* FALLTHROUGH */
		case 'Q':
			cleanup();
			free(g.ents);
			free(g.pents);
			return 0;

		case 0x1b: /* Escape — exit visual mode */
			if (g.visual) {
				g.visual = 0;
				draw();
			}
			break;

		case 'j': /* FALLTHROUGH */
		case KEY_DOWN:
			if (g.sel < g.nent - 1) {
				g.sel++;
				g.have_msg = 0;
				g.search_dim = 0;
				draw();
			}
			break;

		case 'k': /* FALLTHROUGH */
		case KEY_UP:
			if (g.sel > 0) {
				g.sel--;
				g.have_msg = 0;
				g.search_dim = 0;
				draw();
			}
			break;

		case '\r': /* FALLTHROUGH */
		case '\n': /* FALLTHROUGH */
		case 'l':
			g.search_dim = 0;
			open_entry();
			draw();
			break;

		case 0x7f: /* Backspace */ /* FALLTHROUGH */
		case 'h': {
			char par[PATH_MAX];
			char *p;

			strncpy(par, g.cwd, sizeof(par) - 1);
			par[sizeof(par) - 1] = '\0';
			p = strrchr(par, '/');
			if (p == par)
				p[1] = '\0'; /* root: keep "/" */
			else if (p)
				p[0] = '\0';
			g.search_dim = 0;
			nav_to(par);
			draw();
		} break;

		case 'g':
			g.pending_g = 1;
			break;

		case KEY_HOME:
			g.sel = 0;
			g.top = 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 'G': /* FALLTHROUGH */
		case KEY_END:
			g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 4: /* Ctrl-D */
			g.sel += half;
			if (g.sel >= g.nent)
				g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 21: /* Ctrl-U */
			g.sel -= half;
			if (g.sel < 0)
				g.sel = 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 6: /* Ctrl-F */ /* FALLTHROUGH */
		case KEY_PGDN:
			g.sel += full;
			if (g.sel >= g.nent)
				g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 2: /* Ctrl-B */ /* FALLTHROUGH */
		case KEY_PGUP:
			g.sel -= full;
			if (g.sel < 0)
				g.sel = 0;
			g.have_msg = 0;
			g.search_dim = 0;
			draw();
			break;

		case 12: /* Ctrl-L */
			query_dims();
			draw();
			break;

		case 'V':
			if (g.visual) {
				g.visual = 0;
			} else {
				g.visual = 1;
				g.vanchor = g.sel;
			}
			draw();
			break;

		case 'm': {
			int mk = readkey();
			if (mk >= 'a' && mk <= 'z')
				g.marks[mk - 'a'] = g.sel;
			else if (mk >= 'A' && mk <= 'Z')
				bookmark_set(mk - 'A');
		} break;

		case '\'': {
			int mk = readkey();
			if (mk >= 'a' && mk <= 'z' && g.marks[mk - 'a'] >= 0) {
				g.sel = g.marks[mk - 'a'];
				if (g.sel >= g.nent)
					g.sel = g.nent > 0 ? g.nent - 1 : 0;
				g.have_msg = 0;
				draw();
			} else if (mk >= 'A' && mk <= 'Z') {
				bookmark_jump(mk - 'A');
				draw();
			}
		} break;

		case 'r':
			rename_entry();
			draw();
			break;

		case 'd':
			if (g.visual) {
				delete_visual();
				g.visual = 0;
			} else {
				delete_entry();
			}
			draw();
			break;

		case 'y':
			yank_path();
			draw();
			break;

		case 't':
			tab_new(g.cwd);
			draw();
			break;

		case 'T': {
			const char *home = getenv("HOME");
			tab_new(home ? home : "/");
			draw();
		} break;

		case 'x':
			tab_close();
			draw();
			break;

		case '|':
			g.split = !g.split;
			g.ppath[0] = '\0'; /* force preview reload */
			draw();
			break;

		case ':':
			read_cmd();
			draw();
			break;

		case '/':
			read_search();
			draw();
			break;

		case 'n':
			search_jump(1);
			draw();
			break;

		case 'N':
			search_jump(-1);
			draw();
			break;

		default:
			break;
		}
	}
}
