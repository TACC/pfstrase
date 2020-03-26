#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "n_buf.h"
#include "stats.h"
#include "screen.h"

int screen_is_active;
#define MIN(x, y) ({                            \
  typeof(x) _min1 = (x);                    \
  typeof(y) _min2 = (y);                    \
  (void) (&_min1 == &_min2);                \
  _min1 < _min2 ? _min1 : _min2; })

#define MAX(x, y) ({                            \
  typeof(x) _max1 = (x);                    \
  typeof(y) _max2 = (y);                    \
  (void) (&_max1 == &_max2);                \
  _max1 > _max2 ? _max1 : _max2; })

//static void (*screen_refresh_cb)(EV_P_ int LINES, int COLS);

static double top_interval = 10;
static size_t top_k_length;
static N_BUF(top_nb);
static int scroll_start, scroll_delta;
static char status_bar[256];
static double status_bar_time;
static int top_color_pair = CP_BLACK;


static void screen_refresh_cb(EV_P_ int LINES, int COLS);
static void screen_key_cb(EV_P_ int);

static struct ev_timer refresh_timer_w;
static struct ev_timer top_timer_w;

static struct ev_io stdin_io_w;
static struct ev_signal sigint_w;
static struct ev_signal sigterm_w;
static struct ev_signal sigwinch_w;

static unsigned long bkgd_attr[2];

static void refresh_timer_cb(EV_P_ struct ev_timer *w, int revents);
static void stdin_io_cb(EV_P_ struct ev_io *w, int revents);
static void sigint_cb(EV_P_ ev_signal *w, int revents);
static void sigwinch_cb(EV_P_ ev_signal *w, int revents);

static void top_msg_cb(char *msg, size_t msg_len)
{
  int i;
  char *s[2];
  /*
  struct xl_k *k = &top_k[top_k_length];

  if (split(&msg, &s[0], &s[1], (char **) NULL) != 2 || msg == NULL)
    return;

  for (i = 0; i < 2; i++)
    if (xl_sep(s[i], &k->k_type[i], &k->k_x[i]) < 0 || k->k_x[i] == NULL)
      return;
  
  if (sscanf(msg, "%lf "SCN_K_STATS_FMT, &k->k_t, SCN_K_STATS_ARG(k)) !=
      1 + NR_K_STATS)
    return;

  TRACE("%s %s "PRI_STATS_FMT("%f")"\n",
        k->k_x[0], k->k_x[1], PRI_STATS_ARG(k->k_rate));
  */
  top_k_length++;
}

static void top_timer_cb(EV_P_ ev_timer *w, int revents)
{
  double now = ev_now(EV_A);
  char *msg;
  size_t msg_len;


  top_k_length = 0;
  n_buf_destroy(&top_nb);

  //if (curl_x_get(&curl_x, "top", top_query, &top_nb) < 0)
  //return;

  while (n_buf_get_msg(&top_nb, &msg, &msg_len) == 0)
    top_msg_cb(msg, msg_len);

  status_bar_time = 0;
  screen_refresh(EV_A);
}
//int screen_init(void (*refresh_cb)(EV_P_ int, int), double interval)
int screen_init(double interval)
{
  //screen_refresh_cb = refresh_cb;
  //ev_timer_init(&top_timer_w, &top_timer_cb, 0.1, top_interval);
  //ev_timer_start(EV_DEFAULT_ &top_timer_w);

  ev_timer_init(&refresh_timer_w, &refresh_timer_cb, 0.001, interval);
  ev_io_init(&stdin_io_w, &stdin_io_cb, STDIN_FILENO, EV_READ);
  ev_signal_init(&sigint_w, &sigint_cb, SIGINT);
  ev_signal_init(&sigterm_w, &sigint_cb, SIGTERM);
  ev_signal_init(&sigwinch_w, &sigwinch_cb, SIGWINCH);
  return 0;
}

void screen_start(EV_P)
{
  short f[2], b[2];

  /* Begin curses magic. */
  if (initscr() == NULL) {
    fprintf(stderr, "cannot initialize screen: %m\n");
    exit(1);
  }

  cbreak();
  noecho();
  nonl();
  clear();
  intrflush(stdscr, 0);
  
  keypad(stdscr, 1);
  nodelay(stdscr, 1);

  bkgd_attr[0] = getbkgd(stdscr);
  pair_content(bkgd_attr[0], &f[0], &b[0]);

  if (!has_colors())
    fprintf(stderr, "terminal has no color capabilities\n");

  use_default_colors();
  start_color();

  init_pair(CP_BLACK, -1, -1);
  init_pair(CP_RED, COLOR_RED, -1);
  init_pair(CP_YELLOW, COLOR_YELLOW, -1);
  init_pair(CP_GREEN, COLOR_GREEN, -1);
  /* init_pair(CP_BLUE, COLOR_BLUE, -1); */
  init_pair(CP_MAGENTA, COLOR_MAGENTA, -1);
  /* init_pair(CP_CYAN, COLOR_CYAN, -1); */
  /* init_pair(CP_WHITE, COLOR_WHITE, -1); */

  bkgd(COLOR_PAIR(1));
  bkgd_attr[1] = getbkgd(stdscr);
  pair_content(bkgd_attr[1], &f[1], &b[1]);

  curs_set(0); /* Hide the cursor. */

  ev_timer_start(EV_A_ &refresh_timer_w);
  ev_io_start(EV_A_ &stdin_io_w);
  ev_signal_start(EV_A_ &sigint_w);
  ev_signal_start(EV_A_ &sigterm_w);
  ev_signal_start(EV_A_ &sigwinch_w);
  
  screen_is_active = 1;
  printf(">>>>>>>>>>>>>>>>>Screen is activated\n");
}

void screen_refresh(EV_P)
{
  if (!ev_is_pending(&refresh_timer_w))
    ev_feed_event(EV_A_ &refresh_timer_w, EV_TIMER);
}

void screen_stop(EV_P)
{
  ev_timer_stop(EV_A_ &refresh_timer_w);
  endwin();

  screen_is_active = 0;
}

static void refresh_timer_cb(EV_P_ ev_timer *w, int revents)
{
  //(*screen_refresh_cb)(EV_A_ LINES, COLS);
  screen_refresh_cb(EV_A_ LINES, COLS);
  ev_clear_pending(EV_A_ w);
  refresh();
}

void screen_set_key_cb(void(*cb)(EV_P_ int))
{
  //screen_key_cb = cb;
}

static void stdin_io_cb(EV_P_ ev_io *w, int revents)
{
  int key = getch();
  if (key == ERR)
    return;


  if (screen_key_cb != NULL) {
    (*screen_key_cb)(EV_A_ key);
    return;
  }

  /* TODO Remove switch. */
  switch (key) {
  case ' ':
  case '\n':
    screen_refresh(EV_A);
    break;
  case 'q':
    ev_break(EV_A_ EVBREAK_ALL);
    break;
  default:
    fprintf(stderr, "unknown command `%c': try `h' for help\n", key);
    break;
  }
}

static void sigwinch_cb(EV_P_ ev_signal *w, int revents)
{

  struct winsize ws;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
    fprintf(stderr, "cannot get window size: %m\n");
    return;
  }

  LINES = ws.ws_row;
  COLS = ws.ws_col;
  resizeterm(LINES, COLS);

  screen_refresh(EV_A);
}

static void sigint_cb(EV_P_ ev_signal *w, int revents)
{
  ev_break(EV_A_ EVBREAK_ALL);
}


static void screen_key_cb(EV_P_ int key)
{
  switch (tolower(key)) {
  case ' ':
  case '\n':
    ev_feed_event(EV_A_ &top_timer_w, EV_TIMER);
    break;
  case 'q':
    ev_break(EV_A_ EVBREAK_ALL); /* XXX */
    return;
  case KEY_DOWN:
    scroll_delta += 1;
    break;
  case KEY_HOME:
    scroll_delta = INT_MIN / 2;
    break;
  case KEY_END:
    scroll_delta = INT_MAX / 2;
    break;
  case KEY_UP:
    scroll_delta -= 1;
    break;
  case KEY_NPAGE:
    scroll_delta += LINES;
    break;
  case KEY_PPAGE:
    scroll_delta -= LINES;
    break;
  default:
    if (isascii(key)) {
      status_bar_time = ev_now(EV_A);
      snprintf(status_bar, sizeof(status_bar), "unknown command `%c'", key);
    }
    break;
  }

  screen_refresh(EV_A);
}

void status_bar_vprintf(EV_P_ const char *fmt, va_list args)
{
  vsnprintf(status_bar, sizeof(status_bar), fmt, args);
  status_bar_time = ev_now(EV_A);
  screen_refresh(EV_A);
}
void status_bar_printf(EV_P_ const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  status_bar_vprintf(EV_A_ fmt, args);
  va_end(args);
}

#define NR_STATS 3
struct xl_k {
  char *k_x[2];
  int k_type[2];
  double k_t; /* Timestamp. */
  double k_pending[NR_STATS];
  double k_rate[NR_STATS]; /* EWMA bytes (or reqs) per second. */
  double k_sum[NR_STATS];
};

struct xl_col {
  char *c_name;
  int (*c_get_s)(struct xl_col *c, struct xl_k *k, char **s, int *n);
  int (*c_get_d)(struct xl_col *c, struct xl_k *k, double *d);
  int (*c_get_z)(struct xl_col *c, struct xl_k *k, size_t *z);
  size_t c_offset, c_scale;
  int c_width, c_right, c_prec;
};

static struct xl_k *top_k;
static struct xl_col top_col[24];

int c_get_s(struct xl_col *c, struct xl_k *k, char **s, int *n)
{
  char *x = k->k_x[c->c_offset];
  int t = k->k_type[c->c_offset];

  *s = x;
  /*
  if (show_full_names || (t == X_HOST && isdigit(*x)))
    *n = strlen(x);
  else
    *n = strcspn(x, "@.");
    */
  return 0;
}

void c_print(int y, int x, struct xl_col *c, struct xl_k *k)
{
  if (c->c_get_s != NULL) {
    char *s = NULL;
    int n = 0;

    if ((*c->c_get_s)(c, k, &s, &n) < 0 || s == NULL)
      return;

    n = MIN(n, c->c_width);

    mvprintw(y, x, "%-*.*s", c->c_width, n, s);
  } else if (c->c_get_d != NULL) {
    double d;

    if ((*c->c_get_d)(c, k, &d) < 0)
      return;

    if (c->c_scale > 0)
      d /= c->c_scale;

    mvprintw(y, x, "%*.*f", c->c_width, c->c_prec, d);
  } else if (c->c_get_z != NULL) {
    size_t z;

    if ((*c->c_get_z)(c, k, &z) < 0)
      return;

    if (c->c_scale > 0)
      z /= c->c_scale;

    mvprintw(y, x, "%*zu", c->c_width, z);
  }
}
static void print_k(int line, struct xl_col *c, struct xl_k *k)
{
  int i;
  for (i = 0; c->c_name != NULL; c++) {
    c_print(line, i, c, k);
    i += c->c_width + 2;
  }
}
static void screen_refresh_cb(EV_P_ int LINES, int COLS)
{

  time_t now = ev_now(EV_A);
  int line = 0, i;
  //struct xl_fs *f;
  struct xl_col *c;

  erase();

  mvprintw(line, 0, "%-15s  %6s %6s %6s %6s %6s    %6s %6s %6s %6s %6s    %6s",
           "FILESYSTEM",
           "MDS/T", "LOAD1", "LOAD5", "LOAD15", "TASKS",
           "OSS/T", "LOAD1", "LOAD5", "LOAD15", "TASKS", "NIDS");
  mvchgat(line, 0, -1, A_STANDOUT, CP_BLACK, NULL);
  line++;

  mvprintw(line, 0, "%-15s %10s : %10s %16s %16s[MB]\n", 
	   "filesystem", "server", "client", "iops", "bytes");
  //mvchgat(line, 0, -1, A_STANDOUT, CP_BLACK, NULL);
  line++;
 
  double cf = 1.0/(1024*1024);
  json_object_object_foreach(server_tag_sum, servername, server_entry) {
    json_object_object_foreach(server_entry, clientname, client_entry) {
      json_object *export;
      json_object *jid, *uid, *fid;
      if (json_object_object_get_ex(host_map, clientname, &export)) {
	if (!json_object_object_get_ex(export, "jid", &jid) ||
	    !json_object_object_get_ex(export, "uid", &uid) ||
	    !json_object_object_get_ex(export, "fid", &fid))
	  continue;
      }
      json_object *iops, *bytes;
      if (json_object_object_get_ex(client_entry, "iops", &iops) && \
          json_object_object_get_ex(client_entry, "bytes", &bytes))
        mvprintw(line++,0,"%-15s %10s : %10s %16lu %16.1f\n", json_object_get_string(fid), 
		 servername, clientname, 
		  json_object_get_int64(iops),
               ((double)json_object_get_int64(bytes))*cf);
    }
  }
  /*
  list_for_each_entry(f, &fs_list, f_link) {
    char m_buf[80], o_buf[80];

    snprintf(m_buf, sizeof(m_buf), "%zu/%zu", f->f_nr_mds, f->f_nr_mdt);
    snprintf(o_buf, sizeof(o_buf), "%zu/%zu", f->f_nr_oss, f->f_nr_ost);

    mvprintw(line++, 0,
             "%-15s  %6s %6.2f %6.2f %6.2f %6zu    %6s %6.2f %6.2f %6.2f %6zu    %6zu",
             f->f_name,
             m_buf, f->f_mds_load[0], f->f_mds_load[1], f->f_mds_load[2],
             f->f_max_mds_task,
             o_buf, f->f_oss_load[0], f->f_oss_load[1], f->f_oss_load[2],
             f->f_max_oss_task,
             f->f_nr_nid);
  }
  */
  /*
  for (i = 0, c = top_col; c->c_name != NULL; i += c->c_width + 2, c++)
    mvprintw(line, i,
             c->c_right ? "%*.*s  " : "%-*.*s  ",
             c->c_width, c->c_width, c->c_name);
  mvchgat(line, 0, -1, A_STANDOUT, top_color_pair, NULL);
  line++;
  */
  int new_start = scroll_start + scroll_delta;
  int max_start = top_k_length - (LINES - line - 1);
  
  new_start = MIN(new_start, max_start);
  new_start = MAX(new_start, 0);
  
  int j = new_start;
  for (; j < (int) top_k_length && line < LINES - 1; j++, line++)
    print_k(line, top_col, &top_k[j]);
  
  // Get rid of crud that wrapped into this line.  Gross.
  move(line, 0);
  clrtobot();
  
  if (new_start != scroll_start || status_bar_time + 4 < now)
    status_bar_printf(EV_A_ "%d-%d out of %zu",
                      new_start + (top_k_length != 0),
                      j, top_k_length);
  
  mvprintw(LINES - 1, 0, "%.*s", COLS, status_bar);

  int prog_ctime_len = strlen(program_invocation_short_name) + 28;
  //printf(">>>>>>>>>>>>>starting screen_refresh_cb\n");
  /*
  if (strlen(status_bar) + prog_ctime_len < COLS)
    mvprintw(LINES - 1, COLS - prog_ctime_len, "%s - %s",
             program_invocation_short_name, ctime(&now));
  */
  mvchgat(LINES - 1, 0, -1, A_STANDOUT, CP_BLACK, NULL);

  scroll_start = new_start;
  scroll_delta = 0;
  //printf(">>>>>>>>>>>>>exiting screen_refresh_cb\n");
}

