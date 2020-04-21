#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include "stats.h"
#include "screen.h"

int screen_is_active;
static int detailed;
static int scroll_start; 
static int scroll_delta;

static char status_bar[256];
static char sortbykey[32];

static double status_bar_time;
static int top_color_pair = CP_BLACK;

static void screen_refresh_cb(EV_P_ int LINES, int COLS);
static void screen_key_cb(EV_P_ int);

static struct ev_timer refresh_timer_w;

static struct ev_io stdin_io_w;
static struct ev_signal sigint_w;
static struct ev_signal sigterm_w;
static struct ev_signal sigwinch_w;

static unsigned long bkgd_attr[2];

static void refresh_timer_cb(EV_P_ struct ev_timer *w, int revents);
static void stdin_io_cb(EV_P_ struct ev_io *w, int revents);
static void sigint_cb(EV_P_ ev_signal *w, int revents);
static void sigwinch_cb(EV_P_ ev_signal *w, int revents);

json_object *data_map;

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

int screen_init(double interval)
{
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

  /* Sort by load by default */
  snprintf(sortbykey, sizeof(sortbykey), "load");
  data_map = server_tag_sum;
  detailed = 0;
  screen_is_active = 1;
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
  screen_refresh_cb(EV_A_ LINES, COLS);
  ev_clear_pending(EV_A_ w);
  refresh();
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
    /*
  case '\n':
    ev_feed_event(EV_A_ &top_timer_w, EV_TIMER);
    break;
    */
  case 'q':
    ev_break(EV_A_ EVBREAK_ALL); /* XXX */
    return;
  case 'f':
    groupby = 1;
    group_statsbytags(2, "fid", "server");
    break;
  case 'u':
    groupby = 2;
    group_statsbytags(3, "fid", "server", "uid");
    break;
  case 'j':
    groupby = 3;
    group_statsbytags(4, "fid", "server", "jid", "uid");
    break;
  case 'c':
    groupby = 4;
    group_statsbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  case 's':
    groupby = 5;
    group_statsbytags(1, "server");
    break;
  case 'l':
    snprintf(sortbykey, sizeof(sortbykey), "load");
    break;
  case 'i':
    if (!detailed)
      snprintf(sortbykey, sizeof(sortbykey), "iops");   
    else  
      snprintf(sortbykey, sizeof(sortbykey), "open");      
    break;
  case 'b':
    if (!detailed)
      snprintf(sortbykey, sizeof(sortbykey), "bytes");
    else
      snprintf(sortbykey, sizeof(sortbykey), "read_bytes");
    break;
  case 'd':
    if (detailed) {
      snprintf(sortbykey, sizeof(sortbykey), "load");
      data_map = server_tag_sum;
      detailed = 0;
    }
    else {
      snprintf(sortbykey, sizeof(sortbykey), "load");
      data_map = server_tag_rate_map;
      detailed = 1;
    }
    break;
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


/* Sort by specified field value */
static int sort_da(const void *j1, const void *j2) {
  json_object * const *jso1, * const *jso2;
  double d1, d2;
  
  jso1 = (json_object* const*)j1;
  jso2 = (json_object* const*)j2;

  json_object *val1, *val2;
  if (!json_object_object_get_ex(*jso1, sortbykey, &val1) &&
      !json_object_object_get_ex(*jso2, sortbykey, &val2))
    return 0;
  if (!json_object_object_get_ex(*jso1, sortbykey, &val1))
      return 1;
  if (!json_object_object_get_ex(*jso2, sortbykey, &val2))
      return -1;

  d1 = json_object_get_double(val1);
  d2 = json_object_get_double(val2);

  return (int)(d2 - d1);

}

enum json_tokener_error error = json_tokener_success;
static void screen_refresh_cb(EV_P_ int LINES, int COLS)
{

  time_t now = ev_now(EV_A);
  int line = 0, i;

  json_object *eid, *tid;      

  erase();

  json_object *data_array = json_object_new_array();
  json_object *events = json_object_new_object();
  json_object_object_foreach(data_map, s, se) {
    json_object_object_foreach(se, t, te) {
      json_object *tags = NULL;
      if (strcmp(t, "time") == 0) continue;

      tags = json_tokener_parse_verbose(t, &error);
      if (error != json_tokener_success) {
        fprintf(stderr, "tags format incorrect `%s': %s\n", t, json_tokener_error_desc(error));
        goto end;
      }

      json_object_object_foreach(te, event, val) {
	if (strcmp(event, "load") != 0 && json_object_get_double(val) < 2) continue;
	json_object_object_add(events, event, json_object_new_string(""));
	json_object_object_add(tags, event, json_object_get(val));
      }
      json_object_array_add(data_array, json_object_get(tags));

    end:
      if(tags)
	json_object_put(tags);
    }
  }
  json_object_array_sort(data_array, sort_da);
  int data_length = json_object_array_length(data_array);

  /* Construct string to use in tags part of header */
  char header_tags[512];
  snprintf(header_tags, sizeof(header_tags), "%s", "");
  json_object_object_foreach(group_tags, t, v) {
    char t_str[32];
    snprintf(t_str, sizeof(t_str), "%-32s", t);
    strncat(header_tags, t_str, sizeof(header_tags));
  }

  /* Construct string to use in events/rates part of header */
  char header_events[512];
  snprintf(header_events, sizeof(header_events), "%s", "");
  json_object_object_foreach(events, e, val) {
    char e_str[32];
    if (strcmp("bytes", e) == 0 || strcmp("read_bytes", e) == 0 || 
	strcmp("write_bytes", e) == 0)
      snprintf(e_str, sizeof(e_str), "%10s[MB/s]", e);
    else if (strcmp("load", e) == 0)
      snprintf(e_str, sizeof(e_str), "%16s", e);
    else
      snprintf(e_str, sizeof(e_str), "%11s[#/s]", e);
    strncat(header_events, e_str, sizeof(header_events));
  }

  double cf = 1.0/(1024*1024);

  mvprintw(line, 0, "%s %s", header_tags, header_events);
  mvchgat(line, 0, -1, A_STANDOUT, CP_BLACK, NULL);
  line++;

  int new_start = scroll_start + scroll_delta;
  int max_start = data_length - (LINES - line - 1);
  
  new_start = MIN(new_start, max_start);
  new_start = MAX(new_start, 0);

  int j;
  for (j = new_start; j < data_length && line < (LINES - 1); j++) {
    json_object *de = json_object_array_get_idx(data_array, j);

    char row_tags[512];
    snprintf(row_tags, sizeof(row_tags), "%s", "");
    json_object_object_foreach(group_tags, t, v) {
      char tag_str[32];
      if (json_object_object_get_ex(de, t, &tid))
	snprintf(tag_str, sizeof(tag_str), "%-32s", json_object_get_string(tid));
      else 
	snprintf(tag_str, sizeof(tag_str), "%-32s", "");
      strncat(row_tags, tag_str, sizeof(row_tags));
    }

    char row_events[512];    
    snprintf(row_events, sizeof(row_events), "%s", "");
    json_object_object_foreach(events, e, val) {
      char e_str[32];
      if (json_object_object_get_ex(de, e, &eid)) {
	double factor = 1;
	if (strcmp("bytes", e) == 0 || strcmp("read_bytes", e) == 0 || strcmp("write_bytes", e) == 0)
	  factor = cf; 
	if (strcmp("load", e) == 0 )
	  factor = 0.01;
	snprintf(e_str, sizeof(e_str), "%16.2f", json_object_get_double(eid)*factor);
      }
      else
	snprintf(e_str, sizeof(e_str), "%16.1f", 0.0);
      strncat(row_events, e_str, sizeof(row_events));
    }

    mvprintw(line++, 0, "%s %s", row_tags, row_events); 

  }
  json_object_put(data_array);
  move(line, 0);
  clrtobot();
  
  if (new_start != scroll_start || status_bar_time + 4 < now)
    status_bar_printf(EV_A_ "%d-%d out of %d",
                      new_start + (data_length != 0),
                      j, data_length);
  
  mvprintw(LINES - 1, 0, "%.*s", COLS, status_bar);
  
  int prog_ctime_len = strlen(program_invocation_short_name) + 28;
  
  if (strlen(status_bar) + prog_ctime_len < COLS)
    mvprintw(LINES - 1, COLS - prog_ctime_len, "%s - %s",
             program_invocation_short_name, ctime(&now));
  
  mvchgat(LINES - 1, 0, -1, A_STANDOUT, CP_BLACK, NULL);

  scroll_start = new_start;
  scroll_delta = 0;  
}
