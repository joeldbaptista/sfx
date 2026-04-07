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

#define ESC "\033"
#define ESC_CUP ESC "[%d;%dH" /* cursor position (1-based) */
#define ESC_EL ESC "[K"	      /* erase to end of line */
#define ESC_ED ESC "[2J"      /* erase display */
#define ESC_HOME ESC "[H"     /* cursor to top-left */
#define ESC_REV ESC "[7m"     /* reverse video on */
#define ESC_NRM ESC "[m"      /* attributes off */
#define ESC_BOLD ESC "[1m"    /* bold on */
#define ESC_HIDE ESC "[?25l"  /* hide cursor */
#define ESC_SHOW ESC "[?25h"  /* show cursor */

#define MAXENT 4096
#define MSGBUFSZ 512

enum {
	KEY_UP = 256,
	KEY_DOWN,
	KEY_PGUP,
	KEY_PGDN,
	KEY_HOME,
	KEY_END,
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
static void nav_to(const char *path);
static void cursor_at(int row, int col);
static void draw_entry(int row, Entry *e, int selected);
static void draw_status(void);
static void draw(void);
static void open_entry(void);
static void spawn_shell(void);
static void run_shell_cmd(const char *cmd);
static void read_cmd(void);
static void read_search(void);
static void search_jump(int dir);
static void cleanup(void);
static void handle_exit(int sig);
static void handle_sigwinch(int sig);

static State g;
static volatile sig_atomic_t resize_pending;

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

static void
draw_entry(int row, Entry *e, int selected)
{
	char buf[512];

	fmt_entry(e, buf, sizeof(buf));
	if ((int)strlen(buf) > g.cols)
		buf[g.cols] = '\0';

	cursor_at(row, 1);
	if (selected)
		fputs(ESC_REV, stdout);
	if (S_ISDIR(e->st.st_mode))
		fputs(ESC_BOLD, stdout);
	fputs(buf, stdout);
	fputs(ESC_EL, stdout);
	if (S_ISDIR(e->st.st_mode) || selected)
		fputs(ESC_NRM, stdout);
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

static void
draw(void)
{
	int i, row, visible;

	visible = g.rows - 1;
	if (visible < 1)
		visible = 1;

	if (g.sel < g.top)
		g.top = g.sel;
	if (g.sel >= g.top + visible)
		g.top = g.sel - visible + 1;
	if (g.top < 0)
		g.top = 0;

	fputs(ESC_HIDE, stdout);
	for (i = g.top, row = 1; i < g.nent && row <= visible; i++, row++)
		draw_entry(row, &g.ents[i], i == g.sel);
	for (; row <= visible; row++) {
		cursor_at(row, 1);
		fputs(ESC_EL, stdout);
	}
	draw_status();
	fflush(stdout);
	fputs(ESC_SHOW, stdout);
	fflush(stdout);
}

static void
open_entry(void)
{
	Entry *e;
	char path[PATH_MAX];
	char *args[3];
	pid_t pid;
	int st;
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
		args[0] = EDITOR;
		args[1] = path;
		args[2] = NULL;
		execvp(EDITOR, args);
		_exit(127);
	}
	waitpid(pid, &st, 0);
	signal(SIGINT, handle_exit);
	rawmode();
	query_dims();
	g.have_msg = 0;
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
	load_dir(g.cwd);
	if (g.sel >= g.nent)
		g.sel = g.nent > 0 ? g.nent - 1 : 0;
	g.have_msg = 0;
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
	load_dir(g.cwd);
	if (g.sel >= g.nent)
		g.sel = g.nent > 0 ? g.nent - 1 : 0;
	g.have_msg = 0;
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
	search_jump(1);
}

static void
read_cmd(void)
{
#ifdef USE_READLINE
	char *line;

	cookmode();
	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL, stdout);
	fflush(stdout);
	line = readline(":");
	rawmode();
	if (line == NULL || line[0] == '\0') {
		free(line);
		return;
	}
	add_history(line);
	if (strcmp(line, "sh") == 0)
		spawn_shell();
	else
		run_shell_cmd(line);
	free(line);
#else
	char cmd[512];
	int len, c;

	cursor_at(g.rows, 1);
	fputs(ESC_NRM ESC_EL ":", stdout);
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
		if (c >= ' ' && c < 0x7f && len < (int)sizeof(cmd) - 1) {
			cmd[len++] = (char)c;
			putchar(c);
			fflush(stdout);
		}
	}
	cmd[len] = '\0';
	if (len == 0)
		return;
	if (strcmp(cmd, "sh") == 0)
		spawn_shell();
	else
		run_shell_cmd(cmd);
#endif
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
	int c, half, full;

	if (argc > 2) {
		fprintf(stderr, "usage: sfx [directory]\n");
		return 1;
	}
	start = argc == 2 ? argv[1] : ".";

	g.ents = malloc(MAXENT * sizeof(*g.ents));
	if (!g.ents) {
		fprintf(stderr, "sfx: out of memory\n");
		return 1;
	}

	signal(SIGWINCH, handle_sigwinch);
	signal(SIGTERM, handle_exit);
	signal(SIGINT, handle_exit);
	signal(SIGPIPE, SIG_IGN);

	rawmode();
	query_dims();
	nav_to(start);
	draw();

	for (;;) {
		if (resize_pending) {
			resize_pending = 0;
			query_dims();
			draw();
		}

		c = readkey();
		half = (g.rows - 1) / 2;
		full = g.rows - 1;

		switch (c) {
		case 3: /* Ctrl-C */ /* FALLTHROUGH */
		case 'q':	     /* FALLTHROUGH */
		case 'Q':
			cleanup();
			free(g.ents);
			return 0;

		case 'j': /* FALLTHROUGH */
		case KEY_DOWN:
			if (g.sel < g.nent - 1) {
				g.sel++;
				g.have_msg = 0;
				draw();
			}
			break;

		case 'k': /* FALLTHROUGH */
		case KEY_UP:
			if (g.sel > 0) {
				g.sel--;
				g.have_msg = 0;
				draw();
			}
			break;

		case '\r': /* FALLTHROUGH */
		case '\n': /* FALLTHROUGH */
		case 'l':
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
			nav_to(par);
			draw();
		} break;

		case 'g': /* FALLTHROUGH */
		case KEY_HOME:
			g.sel = 0;
			g.top = 0;
			g.have_msg = 0;
			draw();
			break;

		case 'G': /* FALLTHROUGH */
		case KEY_END:
			g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			draw();
			break;

		case 4: /* Ctrl-D */
			g.sel += half;
			if (g.sel >= g.nent)
				g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			draw();
			break;

		case 21: /* Ctrl-U */
			g.sel -= half;
			if (g.sel < 0)
				g.sel = 0;
			g.have_msg = 0;
			draw();
			break;

		case 6: /* Ctrl-F */ /* FALLTHROUGH */
		case KEY_PGDN:
			g.sel += full;
			if (g.sel >= g.nent)
				g.sel = g.nent > 0 ? g.nent - 1 : 0;
			g.have_msg = 0;
			draw();
			break;

		case 2: /* Ctrl-B */ /* FALLTHROUGH */
		case KEY_PGUP:
			g.sel -= full;
			if (g.sel < 0)
				g.sel = 0;
			g.have_msg = 0;
			draw();
			break;

		case 12: /* Ctrl-L */
			query_dims();
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
